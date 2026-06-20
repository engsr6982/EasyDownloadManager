#include "Dispatcher.h"
#include "Global.h"
#include "downloader/DownloadState.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"
#include "model/TaskModel.h"
#include "utils/TaskLogger.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

namespace {

void printUsage(char const* program) {
    std::cerr << "Usage: " << program
              << " --url <url> --output-dir <dir> [--threads <n>] [--retry <n>]"
                 " [--timeout-seconds <n>] [--user-agent <value>] [--proxy <url>]\n";
}

std::unordered_map<std::string, std::string> parseArgs(int argc, char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            args.emplace("--help", "");
            continue;
        }
        if (!key.starts_with("--") || i + 1 >= argc) {
            throw std::runtime_error("Invalid argument: " + key);
        }
        args.emplace(std::move(key), argv[++i]);
    }
    return args;
}

int parseInt(std::unordered_map<std::string, std::string> const& args, std::string const& key, int fallback) {
    auto it = args.find(key);
    if (it == args.end()) return fallback;
    return std::stoi(it->second);
}

std::string getRequired(std::unordered_map<std::string, std::string> const& args, std::string const& key) {
    auto it = args.find(key);
    if (it == args.end() || it->second.empty()) {
        throw std::runtime_error("Missing required argument: " + key);
    }
    return it->second;
}

bool isTerminal(edm::TaskState state) {
    return state == edm::TaskState::Finished || state == edm::TaskState::Failed || state == edm::TaskState::Canceled;
}

} // namespace

int main(int argc, char* argv[]) try {
    auto args = parseArgs(argc, argv);
    if (args.contains("--help")) {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
    }

    auto url       = getRequired(args, "--url");
    auto outputDir = getRequired(args, "--output-dir");

    // to absolute path
    if (std::filesystem::path(outputDir).is_relative()) {
        outputDir = std::filesystem::absolute(outputDir).string();
    }
    std::filesystem::create_directories(outputDir);

    auto threads        = parseInt(args, "--threads", 4);
    auto retries        = parseInt(args, "--retry", 2);
    auto timeoutSeconds = parseInt(args, "--timeout-seconds", 30);

    auto model         = edm::TaskModel::make();
    model->url         = url;
    model->saveDir     = outputDir;
    model->threadCount = threads;
    model->retryCount  = retries;
    model->bandLimit   = edm::GlobalDefaults::kDefaultBandwidthLimit;
    model->state       = edm::TaskState::Pending;
    model->firstTry    = std::time(nullptr);
    model->lastTry     = model->firstTry;

    edm::TaskConfigureDefaults defaults;
    defaults.threadCount = threads;
    defaults.retryCount  = retries;
    defaults.bandLimit   = model->bandLimit;
    if (auto it = args.find("--user-agent"); it != args.end()) {
        defaults.userAgent = it->second;
        model->userAgent   = it->second;
    }
    if (auto it = args.find("--proxy"); it != args.end()) {
        defaults.proxyUrl = it->second;
    }

    auto configure = edm::TaskConfigure::fromUrl(url, outputDir, std::move(defaults));

    edm::TaskLoggerManager::init();

    std::shared_ptr<edm::TaskModel> latestModel;
    edm::Dispatcher                 dispatcher({
                        .taskLoader = [&latestModel](edm::TaskId id) -> std::shared_ptr<edm::TaskModel> {
            return latestModel && latestModel->id == id ? latestModel : nullptr;
        },
                        .onTaskChanged =
            [&latestModel](std::shared_ptr<edm::TaskContext> const& ctx) {
                if (ctx && ctx->model) {
                    latestModel = ctx->model;
                }
            },
                        .configureFactory =
            [](std::shared_ptr<edm::TaskModel> const& task) { return std::make_shared<edm::TaskConfigure>(task); },
    });

    auto created = dispatcher.createTask(model, configure);
    if (!created) {
        std::cerr << "create_error=" << created.error().message() << '\n';
        if (model->id > edm::kInvalidTaskID) {
            std::cerr << "task_id=" << model->id << '\n';
            std::cerr << "task_state=Failed\n";
            std::cerr << "error_message=" << model->errorMsg << '\n';
        }
        return EXIT_FAILURE;
    }

    auto taskId   = created.value()->model->id;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    while (std::chrono::steady_clock::now() < deadline) {
        dispatcher.updateSpeed(taskId);
        auto state = dispatcher.getTaskState(taskId);
        if (!state) {
            std::cerr << "state_error=missing task state\n";
            return EXIT_FAILURE;
        }
        if (isTerminal(state->state())) {
            auto finalState = state->state();
            std::cout << "task_id=" << taskId << '\n';
            std::cout << "downloaded_bytes=" << state->downloadedBytes() << '\n';
            std::cout << "total_bytes=" << state->totalBytes() << '\n';
            std::cout << "output_path=" << state->outputPath() << '\n';
            if (finalState == edm::TaskState::Finished) {
                std::cout << "task_state=Finished\n";
                return EXIT_SUCCESS;
            }
            std::cerr << "task_state=Failed\n";
            std::cerr << "error_message=" << state->errorMessage() << '\n';
            return EXIT_FAILURE;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cerr << "timeout_seconds=" << timeoutSeconds << '\n';
    return EXIT_FAILURE;
} catch (std::exception const& ex) {
    std::cerr << "fatal_error=" << ex.what() << '\n';
    printUsage(argv[0]);
    return EXIT_FAILURE;
}
