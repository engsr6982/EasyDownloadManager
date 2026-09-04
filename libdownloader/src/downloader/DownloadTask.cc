#include "DownloadTask.h"

#include "DownloadTypes.h"
#include "DownloadWorker.h"
#include "FileVerifier.h"
#include "Global.h"
#include "ITaskObserver.h"
#include "MetaInfoFetcher.h"
#include "schema/ChunkMetaFile.h"
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


/**
 * @brief 对文件名进行安全化处理，替换不合法字符
 * @param value 原始文件名
 * @return 安全化处理后的文件名，如果处理结果为空则返回无效文件名常量
 */
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

/**
 * 从Content-Disposition头部中提取文件名
 * @param value Content-Disposition头部的字符串值
 * @return 包含文件名的std::optional对象，如果未找到文件名则返回std::nullopt
 */
std::optional<std::string> fileNameFromContentDisposition(std::string const& value) {
    static const std::regex filenameStarRe(R"(filename\*=UTF-8''([^;]+))", std::regex::icase);
    static const std::regex filenameRe(R"re(filename="?([^";]+)"?)re", std::regex::icase);

    std::smatch match;
    // 首先尝试匹配RFC 5987编码的文件名
    if (std::regex_search(value, match, filenameStarRe) && match.size() > 1) {
        return sanitizeFileName(match[1].str());
    }
    // 如果未找到编码的文件名，尝试匹配常规文件名
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
        auto suffixWithCount = fmt::format("{}({}){}", stem, counter, ext);
        auto candidate       = dir / suffixWithCount;
        if (!std::filesystem::exists(candidate)) return candidate;
    }
}

} // namespace

DownloadTask::DownloadTask(std::shared_ptr<edm::TaskContext> configure, ITaskObserver* observer)
: configure_(std::move(configure)),
  state_(std::make_shared<DownloadState>()),
  observer_(observer),
  isRunningFlag_(std::make_shared<std::atomic<bool>>(false)) {
    assert(configure_);
    state_->setState(TaskState::Pending);
    EDM_LOG_INFO(configure_->id, "DownloadTask created, url={}", configure_->url);
}

DownloadTask::~DownloadTask() {
    assert(configure_);
    assert(state_);
    EDM_LOG_DEBUG(configure_->id, "DownloadTask destroying");

    isRunningFlag_->store(false, std::memory_order_release);
    joinWorkers();

    if (state_->state() == TaskState::Running) {
        setState(TaskState::Paused);
        saveProgress();
    }

    EDM_LOG_DEBUG(configure_->id, "DownloadTask destroyed, releasing logger");
    TaskLoggerManager::remove(configure_->id);
}

void DownloadTask::setState(TaskState state) {
    auto old = state_->state();

    EDM_LOG_DEBUG(configure_->id, "State transition: {} -> {}", static_cast<int>(old), static_cast<int>(state));

    state_->setState(state);
    configure_->state = state; // 同步到配置以持久化
    observer_->onStateChanged(configure_, old);
}

void DownloadTask::notifyError(std::string const& message) {
    EDM_LOG_ERROR(configure_->id, "Error: {}", message);
    observer_->onError(configure_, message);
}


Expected<> DownloadTask::prepareTask() {
    auto taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Preparing task...");

    if (!configure_->metaInfo) {
        EDM_LOG_DEBUG(taskId, "Fetching meta info from server");

        auto metaRes = edm::downloader::MetaInfoFetcher::fetchAll(configure_);
        if (!metaRes) {
            auto err = metaRes.error().message();

            EDM_LOG_ERROR(taskId, "Meta info fetch failed: {}", err);
            notifyError(err);
            setState(TaskState::Failed);
            return makeStringError(err);
        }
        configure_->metaInfo = std::move(metaRes.value());
        EDM_LOG_INFO(
            taskId,
            "Meta info fetched, fileSize={}, supportRange={}",
            configure_->metaInfo->fileSize.value_or(-1),
            configure_->metaInfo->supportRange
        );
    }

    auto const& meta = *configure_->metaInfo;

    auto const fileSize = meta.fileSize.value_or(kInvalidFileSize);

    // --- 解析最终文件名 ---
    // 优先使用 Content-Disposition (服务端返回的真实文件名)，其次回退到 URL 解析。
    if (meta.contentDisposition) {
        auto cdName = fileNameFromContentDisposition(*meta.contentDisposition);
        if (cdName && *cdName != kInvalidFileName) {
            configure_->fileName = std::move(*cdName);
        }
    }
    if (configure_->fileName.empty()) {
        configure_->fileName = fileNameFromUrl(configure_->url);
    }
    // 防呆：确保文件名不为空
    if (configure_->fileName.empty() || configure_->fileName == kInvalidFileName) {
        configure_->fileName = kInvalidFileName;
    }
    EDM_LOG_INFO(taskId, "Resolved filename: {}", configure_->fileName);

    state_->setTotalBytes(fileSize);
    std::error_code dirEc;
    std::filesystem::create_directories(configure_->saveDir, dirEc);
    if (dirEc) {
        auto err = fmt::format("Failed to create save directory: {}", dirEc.message());
        EDM_LOG_ERROR(taskId, err);
        notifyError(err);
        setState(TaskState::Failed);
        return makeStringError(err);
    }
    outFilePath_ =
        (std::filesystem::path(configure_->saveDir) / (sanitizeFileName(configure_->fileName) + kTempFileExt)).string();
    metaFilePath_ = outFilePath_ + kMetaFileExt;
    EDM_LOG_DEBUG(taskId, "Output path: {}, Meta path: {}", outFilePath_, metaFilePath_);

    // TODO: 文件名解析完成后立即通知宿主持久化，确保 UI/DB 中的文件名与磁盘一致。

    if (meta.fileSize < 0) {
        EDM_LOG_WARN(taskId, "Unknown file size, falling back to single-thread download");
        // TODO: 非标准服务端返回数据, 转为单线程下载
        // model->resumable = Resumable::No;
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
    std::filesystem::resize_file(tmpPath, fileSize, ec); // TODO
    if (ec) {
        auto err = fmt::format("Failed to pre-allocate disk space: {}", ec.message());
        EDM_LOG_ERROR(taskId, err);
        notifyError(err);
        setState(TaskState::Failed);
        return makeStringError(err);
    }
    EDM_LOG_DEBUG(taskId, "Pre-allocated {} bytes on disk", fileSize);

    if (!meta.supportRange || !loadProgress()) {
        rebuildRanges();
    }
    EDM_LOG_INFO(taskId, "Task prepared with {} range(s)", state_->ranges.size());

    return {};
}

void DownloadTask::rebuildRanges() {
    state_->ranges.clear();

    int threads = configure_->metaInfo->supportRange ? static_cast<int>(configure_->threads) : 1;
    if (threads <= 0) threads = 1;

    if (!configure_->metaInfo->supportRange) {
        state_->ranges.push_back(
            std::make_shared<DownloadRange>(0, configure_->metaInfo->fileSize.value_or(kInvalidFileSize) - 1)
        );
        EDM_LOG_DEBUG(
            configure_->id,
            "Non-resumable: single range [0, {})",
            configure_->metaInfo->fileSize.value_or(kInvalidFileSize)
        );
        return;
    }

    auto fileSize = configure_->metaInfo->fileSize.value_or(kInvalidFileSize);

    auto targetChunks = std::max<int64_t>(threads * kChunkFanout, 1);
    auto chunkSize    = std::clamp(fileSize / targetChunks, kMinChunkSize, kMaxChunkSize);
    for (int64_t start = 0; start < fileSize; start += chunkSize) {
        auto end = std::min<int64_t>(start + chunkSize - 1, fileSize - 1);
        state_->ranges.push_back(std::make_shared<DownloadRange>(start, end));
    }
    EDM_LOG_DEBUG(
        configure_->id,
        "Built {} ranges with chunkSize={}, threads={}",
        state_->ranges.size(),
        chunkSize,
        threads
    );
}

bool DownloadTask::loadProgress() {
    auto taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Attempting to load progress from {}", metaFilePath_);

    auto reader = schema::EnvelopeReader{kProgressMagic, kProgressVersion, metaFilePath_};

    auto data = reader.read<schema::MetaPaylad>();
    if (!data) {
        EDM_LOG_ERROR(taskId, "Failed to read progress: {}", data.error().message());
        return false;
    }

    // 文件大小不匹配，可能服务端下发了新的文件 URL
    if (data->fileSize != configure_->metaInfo->fileSize) {
        EDM_LOG_WARN(
            taskId,
            "Progress file invalid: fileSize={}, expected={}",
            data->fileSize,
            configure_->metaInfo->fileSize.value_or(-1)
        );
        return false;
    }

    state_->ranges.clear();
    state_->ranges.reserve(data->rangeCount);
    for (auto const& range : data->ranges) {
        auto r = std::make_shared<DownloadRange>(range.start, range.end);
        r->downloaded.store(range.downloaded, std::memory_order_relaxed);
        r->completed.store(range.completed, std::memory_order_relaxed);
        state_->ranges.push_back(r);
    }

    EDM_LOG_INFO(taskId, "Loaded progress: {} range(s) from previous session", data->rangeCount);
    return !state_->ranges.empty();
}

void DownloadTask::saveProgress() const {
    if (metaFilePath_.empty() || configure_->metaInfo->fileSize < 0) return;
    auto taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Saving progress to {}", metaFilePath_);

    {
        auto writer = schema::EnvelopeWriter<schema::MetaPaylad>{kProgressMagic, kProgressVersion, metaFilePath_};

        writer->fileSize   = configure_->metaInfo->fileSize.value();
        writer->rangeCount = state_->ranges.size();

        auto& ranges = writer->ranges;
        ranges.reserve(state_->ranges.size());
        for (auto const& range : state_->ranges) {
            ranges.emplace_back(
                range->start,
                range->end,
                range->downloaded.load(std::memory_order_relaxed),
                range->completed.load(std::memory_order_relaxed)
            );
        }

        if (auto exc = writer.flush(); !exc) {
            EDM_LOG_ERROR(taskId, "Failed to save progress: {}", exc.error().message());
        }
    }
}

void DownloadTask::launchWorkers() {
    auto taskId = configure_->id;
    joinWorkers();
    nextRangeIndex_.store(0, std::memory_order_relaxed);
    activeWorkers_.store(0, std::memory_order_relaxed);
    isRunningFlag_->store(true, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);

    int workerCount = configure_->metaInfo->supportRange ? static_cast<int>(configure_->threads) : 1;
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
    auto taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Worker started");
    DownloadWorker worker(configure_, outFilePath_, isRunningFlag_);

    while (isRunningFlag_->load(std::memory_order_acquire)) {
        auto idx = nextRangeIndex_.fetch_add(1, std::memory_order_relaxed);
        if (idx >= state_->ranges.size()) break;

        auto range = state_->ranges[idx];
        if (range->completed.load(std::memory_order_relaxed)) continue;

        EDM_LOG_DEBUG(taskId, "Worker picking up range {} [{}, {})", idx, range->start, range->end + 1);

        for (int attempt = 0; attempt <= kRetryCount && isRunningFlag_->load(std::memory_order_acquire); ++attempt) {
            range->attempts.store(attempt, std::memory_order_relaxed);

            auto res =
                configure_->metaInfo->supportRange ? worker.downloadRange(range) : worker.downloadWholeFile(range);
            if (res) {
                EDM_LOG_DEBUG(taskId, "Range {} completed successfully", idx);
                break;
            }

            if (!isRunningFlag_->load(std::memory_order_acquire)) break;
            if (attempt == kRetryCount) {
                EDM_LOG_ERROR(taskId, "Range {} failed after {} attempts: {}", idx, attempt + 1, res.error().message());
                notifyError(res.error().message());
                isRunningFlag_->store(false, std::memory_order_release);
            } else {
                EDM_LOG_WARN(
                    taskId,
                    "Range {} attempt {}/{} failed: {}, retrying...",
                    idx,
                    attempt + 1,
                    kRetryCount + 1,
                    res.error().message()
                );
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

    auto taskId = configure_->id;
    auto state  = state_->state();

    EDM_LOG_INFO(taskId, "Starting task, currentState={}", static_cast<int>(state));
    if (state != TaskState::Pending && state != TaskState::Paused && state != TaskState::Failed) {
        EDM_LOG_WARN(taskId, "Task is not startable from current state {}", static_cast<int>(state));
        return makeStringError("Task is not startable from current state");
    }

    if (auto prepared = prepareTask(); !prepared) {
        EDM_LOG_ERROR(taskId, "Task preparation failed: {}", prepared.error().message());
        return prepared;
    }

    setState(TaskState::Running);
    launchWorkers();
    EDM_LOG_INFO(taskId, "Task started and workers launched");
    return {};
}

void DownloadTask::finalizeTask() {
    std::unique_lock lock{mutex_};
    auto             taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Finalizing task...");

    if (state_->state() == TaskState::Paused || state_->state() == TaskState::Canceled) {
        EDM_LOG_INFO(taskId, "Task was paused/canceled, saving progress");
        saveProgress();
        return;
    }

    auto const fileSize = configure_->metaInfo->fileSize.value_or(kInvalidFileSize);

    int64_t totalDownloaded = getDownloadedBytes();
    state_->setDownloadedBytes(totalDownloaded);
    EDM_LOG_DEBUG(taskId, "Total downloaded: {} / {} bytes", totalDownloaded, fileSize);

    if (cancelRequested_.load(std::memory_order_acquire)) {
        EDM_LOG_INFO(taskId, "Cancel requested, marking as canceled");
        setState(TaskState::Canceled);
        return;
    }

    if (fileSize >= 0 && totalDownloaded < fileSize) {
        EDM_LOG_ERROR(taskId, "Download incomplete: {} < {}", totalDownloaded, fileSize);
        notifyError("Download incomplete after retries");
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    EDM_LOG_DEBUG(taskId, "Verifying downloaded file...");
    auto verifyRes = FileVerifier::verify(
        outFilePath_,
        fileSize,
        configure_->metaInfo ? configure_->metaInfo->md5 : std::optional<std::string>{}
    );
    if (!verifyRes) {
        EDM_LOG_ERROR(taskId, "File verification failed: {}", verifyRes.error().message());
        notifyError(verifyRes.error().message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }
    if (!verifyRes.value().ok) {
        EDM_LOG_ERROR(taskId, "File verification mismatch: {}", verifyRes.value().message);
        notifyError(verifyRes.value().message);
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    auto finalName = configure_->fileName.empty() ? fileNameFromUrl(configure_->url) : configure_->fileName;
    auto finalPath = uniqueFinalPath(std::filesystem::path(configure_->saveDir) / sanitizeFileName(finalName));
    std::error_code ec;
    std::filesystem::rename(outFilePath_, finalPath, ec);
    if (ec) {
        EDM_LOG_ERROR(taskId, "Failed to move completed file: {}", ec.message());
        notifyError("Failed to move completed file: " + ec.message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    std::filesystem::remove(metaFilePath_, ec);
    currentSpeed_ = 0.0;
    state_->setSpeed(0.0);
    EDM_LOG_INFO(taskId, "Download completed: {}", finalPath.string());
    setState(TaskState::Finished);
}

Expected<> DownloadTask::pause() {
    {
        std::unique_lock lock{mutex_};
        if (state_->state() != TaskState::Running) {
            EDM_LOG_WARN(configure_->id, "Cannot pause: task is not running");
            return makeStringError("Only running tasks can be paused");
        }
        EDM_LOG_INFO(configure_->id, "Pausing task");
        setState(TaskState::Paused);
        isRunningFlag_->store(false, std::memory_order_release);
    }
    joinWorkers();
    saveProgress();
    EDM_LOG_INFO(configure_->id, "Task paused, progress saved");
    return {};
}

Expected<> DownloadTask::resume() { return start(); }

Expected<> DownloadTask::cancel() {
    {
        std::unique_lock lock{mutex_};
        if (state_->state() != TaskState::Running && state_->state() != TaskState::Paused) {
            EDM_LOG_WARN(configure_->id, "Cannot cancel: task is not running or paused");
            return makeStringError("Only running or paused tasks can be canceled");
        }
        EDM_LOG_INFO(configure_->id, "Canceling task");
        cancelRequested_.store(true, std::memory_order_release);
        setState(TaskState::Canceled);
        isRunningFlag_->store(false, std::memory_order_release);
    }

    joinWorkers();
    std::error_code ec;
    std::filesystem::remove(outFilePath_, ec);
    std::filesystem::remove(metaFilePath_, ec);
    EDM_LOG_INFO(configure_->id, "Task canceled, temp files removed");
    return {};
}

bool DownloadTask::isFinished() const {
    std::shared_lock lock{mutex_};
    return state_->state() == TaskState::Finished;
}

bool DownloadTask::isPaused() const {
    std::shared_lock lock{mutex_};
    return state_->state() == TaskState::Paused;
}

bool DownloadTask::isCanceled() const {
    std::shared_lock lock{mutex_};
    return state_->state() == TaskState::Canceled;
}

bool DownloadTask::isRunning() const {
    std::shared_lock lock{mutex_};
    return state_->state() == TaskState::Running;
}

double DownloadTask::getProgress() {
    std::shared_lock lock{mutex_};
    auto             fileSize = configure_->metaInfo->fileSize.value_or(kInvalidFileSize);
    if (fileSize <= 0) return 0.0;
    return static_cast<double>(getDownloadedBytes()) / static_cast<double>(fileSize);
}

void DownloadTask::updateSpeed() {
    std::shared_lock lock{mutex_};
    if (state_->state() != TaskState::Running) {
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

std::shared_ptr<edm::TaskContext> DownloadTask::getContext() const { return configure_; }
std::shared_ptr<DownloadState>    DownloadTask::getStateObject() const { return state_; }

int64_t DownloadTask::getDownloadedBytes() const {
    int64_t total = 0;
    for (const auto& r : state_->ranges) {
        total += r->downloaded.load(std::memory_order_relaxed);
    }
    return total;
}

std::vector<std::shared_ptr<DownloadRange>> DownloadTask::getRanges() const { return state_->ranges; }

double DownloadTask::getSpeed() const {
    std::shared_lock lock{mutex_};
    return currentSpeed_;
}

TaskState DownloadTask::getState() const {
    std::shared_lock lock{mutex_};
    return state_->state();
}

} // namespace edm::downloader
