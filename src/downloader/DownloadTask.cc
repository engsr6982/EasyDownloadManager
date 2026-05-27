#include "DownloadTask.h"

#include "DownloadWorker.h"
#include "FileVerifier.h"
#include "MetaInfoFetcher.h"
#include "TaskConfigure.h"
#include "dto/TaskContext.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <regex>

namespace edm::downloader {

namespace {

constexpr auto    kTempFileExt   = ".edm";
constexpr auto    kMetaFileExt   = ".meta";
constexpr int64_t kMinChunkSize  = 1024 * 1024;
constexpr int64_t kMaxChunkSize  = 8 * 1024 * 1024;
constexpr int64_t kChunkFanout   = 8;

std::string sanitizeFileName(std::string value) {
    if (value.empty()) return kInvalidFileName;
    for (char& c : value) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            c = '_';
        }
    }
    return value.empty() ? kInvalidFileName : value;
}

std::string fileNameFromUrl(std::string const& url) {
    auto queryPos = url.find('?');
    auto cleanUrl = url.substr(0, queryPos);
    auto slashPos = cleanUrl.find_last_of('/');
    if (slashPos == std::string::npos || slashPos + 1 >= cleanUrl.size()) return kInvalidFileName;
    return sanitizeFileName(cleanUrl.substr(slashPos + 1));
}

std::optional<std::string> fileNameFromContentDisposition(std::string const& value) {
    static const std::regex filenameStarRe(R"(filename\*=UTF-8''([^;]+))", std::regex::icase);
    static const std::regex filenameRe(R"re(filename="?([^";]+)"?)re", std::regex::icase);

    std::smatch match;
    if (std::regex_search(value, match, filenameStarRe) && match.size() > 1) {
        return sanitizeFileName(match[1].str());
    }
    if (std::regex_search(value, match, filenameRe) && match.size() > 1) {
        return sanitizeFileName(match[1].str());
    }
    return std::nullopt;
}

std::filesystem::path uniqueFinalPath(std::filesystem::path path) {
    if (!std::filesystem::exists(path)) return path;

    auto dir  = path.parent_path();
    auto stem = path.stem().string();
    auto ext  = path.extension().string();
    for (int counter = 1;; ++counter) {
        auto candidate = dir / (stem + "(" + std::to_string(counter) + ")" + ext);
        if (!std::filesystem::exists(candidate)) return candidate;
    }
}

} // namespace

DownloadTask::DownloadTask(std::shared_ptr<edm::TaskContext> ctx, StateChangedCallback onStateChanged)
: context_(std::move(ctx)),
  onStateChanged_(std::move(onStateChanged)),
  isRunningFlag_(std::make_shared<std::atomic<bool>>(false)) {
    assert(context_);
    assert(context_->model);
    state_->setState(context_->model->state);
    state_->setTotalBytes(context_->model->fileSize);
}

DownloadTask::~DownloadTask() {
    isRunningFlag_->store(false, std::memory_order_release);
    joinWorkers();
    if (context_ && context_->model && context_->model->state == TaskState::Running) {
        setState(TaskState::Paused);
        saveProgress();
    }
}

void DownloadTask::setState(TaskState state) {
    context_->model->state = state;
    state_->setState(state);
    notifyStateChanged();
}

void DownloadTask::setError(std::string message) {
    context_->model->errorMsg = std::move(message);
    state_->setErrorMessage(context_->model->errorMsg);
}

void DownloadTask::notifyStateChanged() const {
    if (onStateChanged_) {
        onStateChanged_(context_);
    }
}

bool DownloadTask::prepareTask() {
    auto model = context_->model;
    auto conf  = context_->configure;
    if (!model || !conf) {
        return false;
    }

    if (!context_->meta) {
        MetaInfoFetcher fetcher(conf);
        auto            metaRes = fetcher.fetchAll();
        if (!metaRes) {
            setError(metaRes.error().message());
            setState(TaskState::Failed);
            return false;
        }
        context_->meta = std::move(metaRes.value());
    }

    auto const& meta = *context_->meta;
    if (meta.fileSize) model->fileSize = *meta.fileSize;
    model->resumable = meta.supportRange ? Resumable::Yes : Resumable::No;
    if (meta.contentType) model->mimeType = *meta.contentType;
    if (model->fileName == kInvalidFileName || model->fileName.empty()) {
        if (meta.contentDisposition) {
            model->fileName = fileNameFromContentDisposition(*meta.contentDisposition).value_or(fileNameFromUrl(meta.finalUrl));
        } else {
            model->fileName = fileNameFromUrl(meta.finalUrl);
        }
    }

    state_->setTotalBytes(model->fileSize);
    std::filesystem::create_directories(conf->saveDir_);
    outFilePath_  = (std::filesystem::path(conf->saveDir_) / (sanitizeFileName(model->fileName) + kTempFileExt)).string();
    metaFilePath_ = outFilePath_ + kMetaFileExt;
    state_->setOutputPath(outFilePath_);

    if (model->fileSize < 0) {
        model->resumable = Resumable::No;
        state_->ranges.clear();
        state_->ranges.push_back(std::make_shared<DownloadRange>(0, -1));
        return true;
    }

    std::filesystem::path tmpPath(outFilePath_);
    if (!std::filesystem::exists(tmpPath)) {
        std::ofstream ofs(tmpPath, std::ios::binary);
        ofs.close();
    }

    std::error_code ec;
    std::filesystem::resize_file(tmpPath, model->fileSize, ec);
    if (ec) {
        setError("Failed to pre-allocate disk space: " + ec.message());
        setState(TaskState::Failed);
        return false;
    }

    if (model->resumable != Resumable::Yes || !loadProgress()) {
        rebuildRanges();
    }

    return true;
}

void DownloadTask::rebuildRanges() {
    state_->ranges.clear();
    auto model = context_->model;

    int threads = model->resumable == Resumable::Yes ? context_->configure->threadCount_ : 1;
    if (threads <= 0) threads = 1;

    if (model->resumable != Resumable::Yes) {
        state_->ranges.push_back(std::make_shared<DownloadRange>(0, model->fileSize - 1));
        return;
    }

    auto targetChunks = std::max<int64_t>(threads * kChunkFanout, 1);
    auto chunkSize    = std::clamp(model->fileSize / targetChunks, kMinChunkSize, kMaxChunkSize);
    for (int64_t start = 0; start < model->fileSize; start += chunkSize) {
        auto end = std::min<int64_t>(start + chunkSize - 1, model->fileSize - 1);
        state_->ranges.push_back(std::make_shared<DownloadRange>(start, end));
    }
}

bool DownloadTask::loadProgress() {
    std::ifstream in(metaFilePath_);
    if (!in) return false;

    std::string tag;
    int64_t     fileSize = 0;
    in >> tag >> fileSize;
    if (tag != "EDM_PROGRESS_V1" || fileSize != context_->model->fileSize) return false;

    state_->ranges.clear();
    int64_t start = 0;
    int64_t end = 0;
    int64_t downloaded = 0;
    bool    completed = false;
    while (in >> start >> end >> downloaded >> completed) {
        auto range = std::make_shared<DownloadRange>(start, end);
        range->downloaded.store(std::clamp<int64_t>(downloaded, 0, range->size()), std::memory_order_relaxed);
        range->completed.store(completed || range->downloaded.load() == range->size(), std::memory_order_relaxed);
        state_->ranges.push_back(range);
    }
    return !state_->ranges.empty();
}

void DownloadTask::saveProgress() const {
    if (metaFilePath_.empty() || context_->model->fileSize < 0) return;

    std::ofstream out(metaFilePath_, std::ios::trunc);
    if (!out) return;

    out << "EDM_PROGRESS_V1 " << context_->model->fileSize << '\n';
    for (auto const& range : state_->ranges) {
        out << range->start << ' ' << range->end << ' ' << range->downloaded.load(std::memory_order_relaxed) << ' '
            << range->completed.load(std::memory_order_relaxed) << '\n';
    }
}

void DownloadTask::launchWorkers() {
    joinWorkers();
    nextRangeIndex_.store(0, std::memory_order_relaxed);
    activeWorkers_.store(0, std::memory_order_relaxed);
    isRunningFlag_->store(true, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);

    int workerCount = context_->model->resumable == Resumable::Yes ? context_->configure->threadCount_ : 1;
    workerCount     = std::max(workerCount, 1);
    workerCount     = std::min<int>(workerCount, static_cast<int>(std::max<size_t>(state_->ranges.size(), 1)));
    activeWorkers_.store(workerCount, std::memory_order_relaxed);

    workers_.reserve(static_cast<size_t>(workerCount));
    auto self = shared_from_this();
    for (int i = 0; i < workerCount; ++i) {
        workers_.emplace_back([self]() {
            self->workerLoop();
            if (self->activeWorkers_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                self->finalizeTask();
            }
        });
    }
}

void DownloadTask::workerLoop() {
    DownloadWorker worker(context_->configure, outFilePath_, isRunningFlag_);

    while (isRunningFlag_->load(std::memory_order_acquire)) {
        auto idx = nextRangeIndex_.fetch_add(1, std::memory_order_relaxed);
        if (idx >= state_->ranges.size()) break;

        auto range = state_->ranges[idx];
        if (range->completed.load(std::memory_order_relaxed)) continue;

        auto maxRetries = std::max(context_->configure->retryCount_, 0);
        for (int attempt = 0; attempt <= maxRetries && isRunningFlag_->load(std::memory_order_acquire); ++attempt) {
            range->attempts.store(attempt, std::memory_order_relaxed);

            auto res = context_->model->resumable == Resumable::Yes ? worker.downloadRange(range)
                                                                    : worker.downloadWholeFile(range);
            if (res) break;

            if (!isRunningFlag_->load(std::memory_order_acquire)) break;
            if (attempt == maxRetries) {
                setError(res.error().message());
                isRunningFlag_->store(false, std::memory_order_release);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(300 * (attempt + 1)));
            }
        }
    }
}

void DownloadTask::joinWorkers() {
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

bool DownloadTask::start() {
    std::unique_lock lock{mutex_};
    auto             model = context_->model;
    if (model->state != TaskState::Pending && model->state != TaskState::Paused && model->state != TaskState::Failed) {
        return false;
    }

    if (!prepareTask()) return false;

    model->lastTry = std::time(nullptr);
    setState(TaskState::Running);
    launchWorkers();
    return true;
}

void DownloadTask::finalizeTask() {
    std::unique_lock lock{mutex_};

    if (context_->model->state == TaskState::Paused || context_->model->state == TaskState::Canceled) {
        saveProgress();
        return;
    }

    int64_t totalDownloaded = getDownloadedBytes();
    state_->setDownloadedBytes(totalDownloaded);

    if (cancelRequested_.load(std::memory_order_acquire)) {
        setState(TaskState::Canceled);
        return;
    }

    if (!isRunningFlag_->load(std::memory_order_acquire)) {
        setState(context_->model->errorMsg.empty() ? TaskState::Paused : TaskState::Failed);
        saveProgress();
        return;
    }

    if (context_->model->fileSize >= 0 && totalDownloaded < context_->model->fileSize) {
        setError("Download incomplete after retries");
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    auto verifyRes = FileVerifier::verify(
        outFilePath_,
        context_->model->fileSize,
        context_->meta ? context_->meta->md5 : std::optional<std::string>{}
    );
    if (!verifyRes) {
        setError(verifyRes.error().message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }
    if (!verifyRes.value().ok) {
        setError(verifyRes.value().message);
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    auto finalPath = uniqueFinalPath(std::filesystem::path(context_->configure->saveDir_) / sanitizeFileName(context_->model->fileName));
    std::error_code ec;
    std::filesystem::rename(outFilePath_, finalPath, ec);
    if (ec) {
        setError("Failed to move completed file: " + ec.message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    std::filesystem::remove(metaFilePath_, ec);
    context_->model->fileName = finalPath.filename().string();
    currentSpeed_             = 0.0;
    state_->setSpeed(0.0);
    state_->setOutputPath(finalPath.string());
    setState(TaskState::Finished);
}

bool DownloadTask::pause() {
    {
        std::unique_lock lock{mutex_};
        if (context_->model->state != TaskState::Running) return false;
        setState(TaskState::Paused);
        isRunningFlag_->store(false, std::memory_order_release);
    }
    joinWorkers();
    saveProgress();
    return true;
}

bool DownloadTask::resume() {
    return start();
}

bool DownloadTask::cancel() {
    {
        std::unique_lock lock{mutex_};
        if (context_->model->state != TaskState::Running && context_->model->state != TaskState::Paused) return false;
        cancelRequested_.store(true, std::memory_order_release);
        setState(TaskState::Canceled);
        isRunningFlag_->store(false, std::memory_order_release);
    }

    joinWorkers();
    std::error_code ec;
    std::filesystem::remove(outFilePath_, ec);
    std::filesystem::remove(metaFilePath_, ec);
    return true;
}

bool DownloadTask::isFinished() const {
    std::shared_lock lock{mutex_};
    return context_->model->state == TaskState::Finished;
}

bool DownloadTask::isPaused() const {
    std::shared_lock lock{mutex_};
    return context_->model->state == TaskState::Paused;
}

bool DownloadTask::isCanceled() const {
    std::shared_lock lock{mutex_};
    return context_->model->state == TaskState::Canceled;
}

bool DownloadTask::isRunning() const {
    std::shared_lock lock{mutex_};
    return context_->model->state == TaskState::Running;
}

double DownloadTask::getProgress() {
    std::shared_lock lock{mutex_};
    auto             fileSize = context_->model->fileSize;
    if (fileSize <= 0) return 0.0;
    return static_cast<double>(getDownloadedBytes()) / static_cast<double>(fileSize);
}

void DownloadTask::updateSpeed() {
    std::shared_lock lock{mutex_};
    if (context_->model->state != TaskState::Running) {
        currentSpeed_ = 0.0;
        state_->setSpeed(0.0);
        return;
    }
    auto total     = getDownloadedBytes();
    currentSpeed_  = static_cast<double>(total - lastDownloadedBytes_);
    lastDownloadedBytes_ = total;
    state_->setDownloadedBytes(total);
    state_->setSpeed(currentSpeed_);
}

std::shared_ptr<edm::TaskContext> DownloadTask::getTaskContext() const { return context_; }
std::shared_ptr<DownloadState> DownloadTask::getStateObject() const { return state_; }

int64_t DownloadTask::getDownloadedBytes() const {
    int64_t total = 0;
    for (const auto& r : state_->ranges) {
        total += r->downloaded.load(std::memory_order_relaxed);
    }
    return total;
}

double DownloadTask::getSpeed() const {
    std::shared_lock lock{mutex_};
    return currentSpeed_;
}

std::optional<std::string> DownloadTask::getLastError() {
    std::shared_lock lock{mutex_};
    if (context_->model->errorMsg.empty()) return std::nullopt;
    return context_->model->errorMsg;
}

TaskState DownloadTask::getState() const {
    std::shared_lock lock{mutex_};
    return context_->model->state;
}

} // namespace edm::downloader
