#include "DownloadTask.h"

#include "DownloadWorker.h"
#include "FileVerifier.h"
#include "MetaInfoFetcher.h"
#include "TaskConfigure.h"
#include "dto/TaskContext.h"
#include "utils/TaskLogger.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

namespace edm::downloader {

namespace {

constexpr auto     kTempFileExt     = ".edm";
constexpr auto     kMetaFileExt     = ".meta";
constexpr int64_t  kMinChunkSize    = 1024 * 1024;
constexpr int64_t  kMaxChunkSize    = 8 * 1024 * 1024;
constexpr int64_t  kChunkFanout     = 8;
constexpr uint32_t kProgressMagic   = 0x4D444545; // "EEDM" little-endian marker for EDM progress metadata.
constexpr uint32_t kProgressVersion = 2;


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

uint64_t fnv1a(uint64_t hash, void const* data, size_t len) {
    auto bytes = static_cast<unsigned char const*>(data);
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <typename T>
void appendBytes(std::vector<unsigned char>& out, T const& value) {
    auto ptr = reinterpret_cast<unsigned char const*>(&value);
    out.insert(out.end(), ptr, ptr + sizeof(T));
}

template <typename T>
bool readValue(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return in.good();
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
    EDM_LOG_INFO(context_->model->id, "DownloadTask created, url={}", context_->model->url);
}

DownloadTask::~DownloadTask() {
    if (context_ && context_->model) {
        EDM_LOG_DEBUG(context_->model->id, "DownloadTask destroying");
    }
    isRunningFlag_->store(false, std::memory_order_release);
    joinWorkers();
    if (context_ && context_->model && context_->model->state == TaskState::Running) {
        setState(TaskState::Paused);
        saveProgress();
    }
    if (context_ && context_->model) {
        EDM_LOG_DEBUG(context_->model->id, "DownloadTask destroyed, releasing logger");
        TaskLoggerManager::remove(context_->model->id);
    }
}

void DownloadTask::setState(TaskState state) {
    EDM_LOG_DEBUG(context_->model->id, "State transition: {} -> {}", static_cast<int>(context_->model->state), static_cast<int>(state));
    context_->model->state = state;
    state_->setState(state);
    notifyStateChanged();
}

void DownloadTask::setError(std::string message) {
    EDM_LOG_ERROR(context_->model->id, "Error: {}", message);
    context_->model->errorMsg = std::move(message);
    state_->setErrorMessage(context_->model->errorMsg);
}

void DownloadTask::notifyStateChanged() const {
    if (onStateChanged_) {
        onStateChanged_(context_);
    }
}

Expected<> DownloadTask::prepareTask() {
    auto taskId = context_->model->id;
    EDM_LOG_DEBUG(taskId, "Preparing task...");
    auto model = context_->model;
    auto conf  = context_->configure;
    if (!model || !conf) {
        EDM_LOG_ERROR(taskId, "Task model or configure is missing");
        return makeStringError("Task model or configure is missing");
    }

    if (!context_->meta) {
        EDM_LOG_DEBUG(taskId, "Fetching meta info from server");
        MetaInfoFetcher fetcher(conf, taskId);
        auto            metaRes = fetcher.fetchAll();
        if (!metaRes) {
            EDM_LOG_ERROR(taskId, "Meta info fetch failed: {}", metaRes.error().message());
            setError(metaRes.error().message());
            setState(TaskState::Failed);
            return makeStringError(context_->model->errorMsg);
        }
        context_->meta = std::move(metaRes.value());
        EDM_LOG_INFO(taskId, "Meta info fetched, fileSize={}, supportRange={}",
                     context_->meta->fileSize.value_or(-1), context_->meta->supportRange);
    }

    auto const& meta = *context_->meta;
    if (meta.fileSize) model->fileSize = *meta.fileSize;
    model->resumable = meta.supportRange ? Resumable::Yes : Resumable::No;
    if (meta.contentType) model->mimeType = *meta.contentType;
    if (model->fileName == kInvalidFileName || model->fileName.empty()) {
        if (meta.contentDisposition) {
            model->fileName =
                fileNameFromContentDisposition(*meta.contentDisposition).value_or(fileNameFromUrl(meta.finalUrl));
        } else {
            model->fileName = fileNameFromUrl(meta.finalUrl);
        }
    }
    EDM_LOG_DEBUG(taskId, "Resolved fileName={}, fileSize={}, resumable={}",
                  model->fileName, model->fileSize, static_cast<int>(model->resumable));

    state_->setTotalBytes(model->fileSize);
    std::error_code dirEc;
    std::filesystem::create_directories(conf->saveDir_, dirEc);
    if (dirEc) {
        EDM_LOG_ERROR(taskId, "Failed to create save directory: {}", dirEc.message());
        setError("Failed to create save directory: " + dirEc.message());
        setState(TaskState::Failed);
        return makeStringError(context_->model->errorMsg);
    }
    outFilePath_ =
        (std::filesystem::path(conf->saveDir_) / (sanitizeFileName(model->fileName) + kTempFileExt)).string();
    metaFilePath_ = outFilePath_ + kMetaFileExt;
    state_->setOutputPath(outFilePath_);
    EDM_LOG_DEBUG(taskId, "Output path: {}, Meta path: {}", outFilePath_, metaFilePath_);

    if (model->fileSize < 0) {
        EDM_LOG_WARN(taskId, "Unknown file size, falling back to single-thread download");
        // TODO: 非标准服务端返回数据, 转为单线程下载
        model->resumable = Resumable::No;
        state_->ranges.clear();
        state_->ranges.push_back(std::make_shared<DownloadRange>(0, -1));

        // Ensure the temp file exists so that downloadWholeFile can open it.
        std::filesystem::path tmpPath(outFilePath_);
        if (!std::filesystem::exists(tmpPath)) {
            std::ofstream ofs(tmpPath, std::ios::binary);
            ofs.close();
        }
        return {};
    }

    std::filesystem::path tmpPath(outFilePath_);
    if (!std::filesystem::exists(tmpPath)) {
        std::ofstream ofs(tmpPath, std::ios::binary);
        ofs.close();
    }

    std::error_code ec;
    std::filesystem::resize_file(tmpPath, model->fileSize, ec); // TODO
    if (ec) {
        EDM_LOG_ERROR(taskId, "Failed to pre-allocate disk space: {}", ec.message());
        setError("Failed to pre-allocate disk space: " + ec.message());
        setState(TaskState::Failed);
        return makeStringError(context_->model->errorMsg);
    }
    EDM_LOG_DEBUG(taskId, "Pre-allocated {} bytes on disk", model->fileSize);

    if (model->resumable != Resumable::Yes || !loadProgress()) {
        rebuildRanges();
    }
    EDM_LOG_INFO(taskId, "Task prepared with {} range(s)", state_->ranges.size());

    return {};
}

void DownloadTask::rebuildRanges() {
    state_->ranges.clear();
    auto model = context_->model;

    int threads = model->resumable == Resumable::Yes ? context_->configure->threadCount_ : 1;
    if (threads <= 0) threads = 1;

    if (model->resumable != Resumable::Yes) {
        state_->ranges.push_back(std::make_shared<DownloadRange>(0, model->fileSize - 1));
        EDM_LOG_DEBUG(model->id, "Non-resumable: single range [0, {})", model->fileSize);
        return;
    }

    auto targetChunks = std::max<int64_t>(threads * kChunkFanout, 1);
    auto chunkSize    = std::clamp(model->fileSize / targetChunks, kMinChunkSize, kMaxChunkSize);
    for (int64_t start = 0; start < model->fileSize; start += chunkSize) {
        auto end = std::min<int64_t>(start + chunkSize - 1, model->fileSize - 1);
        state_->ranges.push_back(std::make_shared<DownloadRange>(start, end));
    }
    EDM_LOG_DEBUG(model->id, "Built {} ranges with chunkSize={}, threads={}",
                  state_->ranges.size(), chunkSize, threads);
}

bool DownloadTask::loadProgress() {
    auto taskId = context_->model->id;
    EDM_LOG_DEBUG(taskId, "Attempting to load progress from {}", metaFilePath_);
    std::ifstream in(metaFilePath_, std::ios::binary);
    if (!in) {
        EDM_LOG_DEBUG(taskId, "No existing progress file, starting fresh");
        return false;
    }

    // Progress metadata is deliberately binary and versioned. It is not a security boundary,
    // but the magic/version/checksum combination prevents accidental edits from being trusted.
    uint32_t magic            = 0;
    uint32_t version          = 0;
    int64_t  fileSize         = 0;
    uint64_t rangeCount       = 0;
    uint64_t expectedChecksum = 0;
    if (!readValue(in, magic) || !readValue(in, version) || !readValue(in, fileSize) || !readValue(in, rangeCount)
        || !readValue(in, expectedChecksum)) {
        EDM_LOG_WARN(taskId, "Progress file read error (header)");
        return false;
    }
    if (magic != kProgressMagic || version != kProgressVersion || fileSize != context_->model->fileSize
        || rangeCount == 0) {
        EDM_LOG_WARN(taskId, "Progress file invalid: magic={:#x}, version={}, fileSize={}, rangeCount={}",
                     magic, version, fileSize, rangeCount);
        return false;
    }

    state_->ranges.clear();
    std::vector<unsigned char> checksumBuffer;
    checksumBuffer.reserve(static_cast<size_t>(rangeCount) * (sizeof(int64_t) * 3 + sizeof(uint8_t)));

    for (uint64_t i = 0; i < rangeCount; ++i) {
        int64_t start      = 0;
        int64_t end        = 0;
        int64_t downloaded = 0;
        uint8_t completed  = 0;
        if (!readValue(in, start) || !readValue(in, end) || !readValue(in, downloaded) || !readValue(in, completed)) {
            EDM_LOG_WARN(taskId, "Progress file read error at range {}", i);
            state_->ranges.clear();
            return false;
        }
        appendBytes(checksumBuffer, start);
        appendBytes(checksumBuffer, end);
        appendBytes(checksumBuffer, downloaded);
        appendBytes(checksumBuffer, completed);

        auto range = std::make_shared<DownloadRange>(start, end);
        range->downloaded.store(std::clamp<int64_t>(downloaded, 0, range->size()), std::memory_order_relaxed);
        range->completed.store(completed != 0 || range->downloaded.load() == range->size(), std::memory_order_relaxed);
        state_->ranges.push_back(range);
    }

    auto actualChecksum = fnv1a(1469598103934665603ULL, checksumBuffer.data(), checksumBuffer.size());
    if (actualChecksum != expectedChecksum) {
        EDM_LOG_WARN(taskId, "Progress file checksum mismatch (expected={:#x}, actual={:#x})",
                     expectedChecksum, actualChecksum);
        state_->ranges.clear();
        return false;
    }
    EDM_LOG_INFO(taskId, "Loaded progress: {} range(s) from previous session", rangeCount);
    return !state_->ranges.empty();
}

void DownloadTask::saveProgress() const {
    if (metaFilePath_.empty() || context_->model->fileSize < 0) return;
    auto taskId = context_->model->id;
    EDM_LOG_DEBUG(taskId, "Saving progress to {}", metaFilePath_);

    // Keep the record payload separate so the checksum covers exactly the mutable range data.
    // Header fields remain readable enough for future migrations without accepting stale ranges.
    std::vector<unsigned char> records;
    records.reserve(state_->ranges.size() * (sizeof(int64_t) * 3 + sizeof(uint8_t)));
    for (auto const& range : state_->ranges) {
        auto downloaded = range->downloaded.load(std::memory_order_relaxed);
        auto completed  = static_cast<uint8_t>(range->completed.load(std::memory_order_relaxed) ? 1 : 0);
        appendBytes(records, range->start);
        appendBytes(records, range->end);
        appendBytes(records, downloaded);
        appendBytes(records, completed);
    }

    std::ofstream out(metaFilePath_, std::ios::binary | std::ios::trunc);
    if (!out) {
        EDM_LOG_ERROR(taskId, "Failed to open progress file for writing");
        return;
    }

    auto magic      = kProgressMagic;
    auto version    = kProgressVersion;
    auto fileSize   = context_->model->fileSize;
    auto rangeCount = static_cast<uint64_t>(state_->ranges.size());
    auto checksum   = fnv1a(1469598103934665603ULL, records.data(), records.size());

    out.write(reinterpret_cast<char const*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<char const*>(&version), sizeof(version));
    out.write(reinterpret_cast<char const*>(&fileSize), sizeof(fileSize));
    out.write(reinterpret_cast<char const*>(&rangeCount), sizeof(rangeCount));
    out.write(reinterpret_cast<char const*>(&checksum), sizeof(checksum));
    out.write(reinterpret_cast<char const*>(records.data()), static_cast<std::streamsize>(records.size()));
}

void DownloadTask::launchWorkers() {
    auto taskId = context_->model->id;
    joinWorkers();
    nextRangeIndex_.store(0, std::memory_order_relaxed);
    activeWorkers_.store(0, std::memory_order_relaxed);
    isRunningFlag_->store(true, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);

    int workerCount = context_->model->resumable == Resumable::Yes ? context_->configure->threadCount_ : 1;
    workerCount     = std::max(workerCount, 1);
    workerCount     = std::min<int>(workerCount, static_cast<int>(std::max<size_t>(state_->ranges.size(), 1)));
    activeWorkers_.store(workerCount, std::memory_order_relaxed);

    EDM_LOG_INFO(taskId, "Launching {} worker(s) for {} range(s)", workerCount, state_->ranges.size());

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
    auto taskId = context_->model->id;
    EDM_LOG_DEBUG(taskId, "Worker started");
    DownloadWorker worker(context_->configure, outFilePath_, isRunningFlag_, taskId);

    while (isRunningFlag_->load(std::memory_order_acquire)) {
        auto idx = nextRangeIndex_.fetch_add(1, std::memory_order_relaxed);
        if (idx >= state_->ranges.size()) break;

        auto range = state_->ranges[idx];
        if (range->completed.load(std::memory_order_relaxed)) continue;

        EDM_LOG_DEBUG(taskId, "Worker picking up range {} [{}, {})", idx, range->start, range->end + 1);

        auto maxRetries = std::max(context_->configure->retryCount_, 0);
        for (int attempt = 0; attempt <= maxRetries && isRunningFlag_->load(std::memory_order_acquire); ++attempt) {
            range->attempts.store(attempt, std::memory_order_relaxed);

            auto res = context_->model->resumable == Resumable::Yes ? worker.downloadRange(range)
                                                                    : worker.downloadWholeFile(range);
            if (res) {
                EDM_LOG_DEBUG(taskId, "Range {} completed successfully", idx);
                break;
            }

            if (!isRunningFlag_->load(std::memory_order_acquire)) break;
            if (attempt == maxRetries) {
                EDM_LOG_ERROR(taskId, "Range {} failed after {} attempts: {}", idx, attempt + 1, res.error().message());
                setError(res.error().message());
                isRunningFlag_->store(false, std::memory_order_release);
            } else {
                EDM_LOG_WARN(taskId, "Range {} attempt {}/{} failed: {}, retrying...",
                             idx, attempt + 1, maxRetries + 1, res.error().message());
                std::this_thread::sleep_for(std::chrono::milliseconds(300 * (attempt + 1)));
            }
        }
    }
    EDM_LOG_DEBUG(taskId, "Worker exiting");
}

void DownloadTask::joinWorkers() {
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

Expected<> DownloadTask::start() {
    std::unique_lock lock{mutex_};
    auto             model = context_->model;
    auto             taskId = model->id;
    EDM_LOG_INFO(taskId, "Starting task, currentState={}", static_cast<int>(model->state));
    if (model->state != TaskState::Pending && model->state != TaskState::Paused && model->state != TaskState::Failed) {
        EDM_LOG_WARN(taskId, "Task is not startable from current state {}", static_cast<int>(model->state));
        return makeStringError("Task is not startable from current state");
    }

    if (auto prepared = prepareTask(); !prepared) {
        EDM_LOG_ERROR(taskId, "Task preparation failed: {}", prepared.error().message());
        return prepared;
    }

    model->lastTry = std::time(nullptr);
    setState(TaskState::Running);
    launchWorkers();
    EDM_LOG_INFO(taskId, "Task started and workers launched");
    return {};
}

void DownloadTask::finalizeTask() {
    std::unique_lock lock{mutex_};
    auto taskId = context_->model->id;
    EDM_LOG_DEBUG(taskId, "Finalizing task...");

    if (context_->model->state == TaskState::Paused || context_->model->state == TaskState::Canceled) {
        EDM_LOG_INFO(taskId, "Task was paused/canceled, saving progress");
        saveProgress();
        return;
    }

    int64_t totalDownloaded = getDownloadedBytes();
    state_->setDownloadedBytes(totalDownloaded);
    EDM_LOG_DEBUG(taskId, "Total downloaded: {} / {} bytes", totalDownloaded, context_->model->fileSize);

    if (cancelRequested_.load(std::memory_order_acquire)) {
        EDM_LOG_INFO(taskId, "Cancel requested, marking as canceled");
        setState(TaskState::Canceled);
        return;
    }

    if (!isRunningFlag_->load(std::memory_order_acquire)) {
        setState(context_->model->errorMsg.empty() ? TaskState::Paused : TaskState::Failed);
        saveProgress();
        return;
    }

    if (context_->model->fileSize >= 0 && totalDownloaded < context_->model->fileSize) {
        EDM_LOG_ERROR(taskId, "Download incomplete: {} < {}", totalDownloaded, context_->model->fileSize);
        setError("Download incomplete after retries");
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    EDM_LOG_DEBUG(taskId, "Verifying downloaded file...");
    auto verifyRes = FileVerifier::verify(
        outFilePath_,
        context_->model->fileSize,
        context_->meta ? context_->meta->md5 : std::optional<std::string>{}
    );
    if (!verifyRes) {
        EDM_LOG_ERROR(taskId, "File verification failed: {}", verifyRes.error().message());
        setError(verifyRes.error().message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }
    if (!verifyRes.value().ok) {
        EDM_LOG_ERROR(taskId, "File verification mismatch: {}", verifyRes.value().message);
        setError(verifyRes.value().message);
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    auto finalPath = uniqueFinalPath(
        std::filesystem::path(context_->configure->saveDir_) / sanitizeFileName(context_->model->fileName)
    );
    std::error_code ec;
    std::filesystem::rename(outFilePath_, finalPath, ec);
    if (ec) {
        EDM_LOG_ERROR(taskId, "Failed to move completed file: {}", ec.message());
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
    EDM_LOG_INFO(taskId, "Download completed: {}", finalPath.string());
    setState(TaskState::Finished);
}

Expected<> DownloadTask::pause() {
    {
        std::unique_lock lock{mutex_};
        if (context_->model->state != TaskState::Running) {
            EDM_LOG_WARN(context_->model->id, "Cannot pause: task is not running");
            return makeStringError("Only running tasks can be paused");
        }
        EDM_LOG_INFO(context_->model->id, "Pausing task");
        setState(TaskState::Paused);
        isRunningFlag_->store(false, std::memory_order_release);
    }
    joinWorkers();
    saveProgress();
    EDM_LOG_INFO(context_->model->id, "Task paused, progress saved");
    return {};
}

Expected<> DownloadTask::resume() { return start(); }

Expected<> DownloadTask::cancel() {
    {
        std::unique_lock lock{mutex_};
        if (context_->model->state != TaskState::Running && context_->model->state != TaskState::Paused) {
            EDM_LOG_WARN(context_->model->id, "Cannot cancel: task is not running or paused");
            return makeStringError("Only running or paused tasks can be canceled");
        }
        EDM_LOG_INFO(context_->model->id, "Canceling task");
        cancelRequested_.store(true, std::memory_order_release);
        setState(TaskState::Canceled);
        isRunningFlag_->store(false, std::memory_order_release);
    }

    joinWorkers();
    std::error_code ec;
    std::filesystem::remove(outFilePath_, ec);
    std::filesystem::remove(metaFilePath_, ec);
    EDM_LOG_INFO(context_->model->id, "Task canceled, temp files removed");
    return {};
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
    auto total           = getDownloadedBytes();
    currentSpeed_        = static_cast<double>(total - lastDownloadedBytes_);
    lastDownloadedBytes_ = total;
    state_->setDownloadedBytes(total);
    state_->setSpeed(currentSpeed_);
}

std::shared_ptr<edm::TaskContext> DownloadTask::getTaskContext() const { return context_; }
std::shared_ptr<DownloadState>    DownloadTask::getStateObject() const { return state_; }

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
