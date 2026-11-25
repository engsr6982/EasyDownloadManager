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

constexpr auto    kTempFileExt  = ".edm";
constexpr auto    kMetaFileExt  = ".meta";
constexpr int64_t kMinChunkSize = 1024 * 1024;
constexpr int64_t kMaxChunkSize = 8 * 1024 * 1024;
constexpr int64_t kChunkFanout  = 8;

using schema::kProgressMagic;
using schema::kProgressVersion;


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
    static std::regex const filenameStarRe(R"(filename\*=UTF-8''([^;]+))", std::regex::icase);
    static std::regex const filenameRe(R"re(filename="?([^";]+)"?)re", std::regex::icase);

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

/// 拆分文件名主干与扩展名；.tar.gz 这类复合扩展名整体处理，
/// 序号插在主干与复合扩展名之间（name(1).tar.gz），不拆散原扩展名
std::pair<std::string, std::string> splitNameAndExtension(std::filesystem::path const& path) {
    auto ext     = path.extension().string();
    auto stem    = path.stem().string();
    auto toLower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };
    auto const lowerExt = toLower(ext);
    if ((lowerExt == ".gz" || lowerExt == ".bz2" || lowerExt == ".xz" || lowerExt == ".zst") && stem.size() > 4
        && toLower(stem).ends_with(".tar")) {
        return {stem.substr(0, stem.size() - 4), ".tar" + ext};
    }
    return {stem, ext};
}

std::filesystem::path uniqueFinalPath(std::filesystem::path path) {
    if (!std::filesystem::exists(path)) return path;

    auto dir         = path.parent_path();
    auto [stem, ext] = splitNameAndExtension(path);
    for (int counter = 1;; ++counter) {
        auto suffixWithCount = fmt::format("{}({}){}", stem, counter, ext);
        auto candidate       = dir / suffixWithCount;
        if (!std::filesystem::exists(candidate)) return candidate;
    }
}

/// 临时文件归属判定：读取 .meta 的 taskId，匹配则属于本任务（重启续传接管）
bool tempPathOwnedByTask(std::filesystem::path const& metaPath, TaskId taskId) {
    auto reader = schema::EnvelopeReader{kProgressMagic, kProgressVersion, metaPath};
    auto data   = reader.read<schema::MetaPaylad>();
    return data && data->taskId == taskId;
}

/// 临时文件路径：<name>.edm，冲突时按 name(n).ext.edm 递增；已存在但 .meta
/// 归属本任务的路径直接复用（重启/恢复续传），并发同名任务互不误占
std::filesystem::path uniqueTempPath(std::filesystem::path const& dir, std::string const& fileName, TaskId taskId) {
    auto [stem, ext] = splitNameAndExtension(std::filesystem::path(fileName));
    auto owned = [&](std::filesystem::path const& p) { return tempPathOwnedByTask(p.string() + kMetaFileExt, taskId); };

    auto first = dir / (fileName + kTempFileExt);
    if (!std::filesystem::exists(first) || owned(first)) return first;
    for (int counter = 1;; ++counter) {
        auto candidate = dir / fmt::format("{}({}){}{}", stem, counter, ext, kTempFileExt);
        if (!std::filesystem::exists(candidate) || owned(candidate)) return candidate;
    }
}

} // namespace

void removeTaskTempFiles(std::string const& saveDir, std::string const& fileName, TaskId taskId) {
    namespace fs = std::filesystem;
    if (fileName.empty()) return;
    auto const      dir = fs::path{saveDir};
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    auto [stem, ext] = splitNameAndExtension(fs::path{fileName});

    // 候选形态：<fileName>.edm 与 <stem>(<n>)<ext>.edm
    auto isTempOfName = [&](std::string const& name) {
        if (name.size() <= 4 || !name.ends_with(kTempFileExt)) return false;
        auto base = name.substr(0, name.size() - 4);
        if (base == fileName) return true;
        if (base.size() <= stem.size() + ext.size()) return false;
        if (!base.starts_with(stem) || !base.ends_with(ext)) return false;
        auto mid     = base.substr(stem.size(), base.size() - stem.size() - ext.size());
        auto isDigit = [](unsigned char c) { return std::isdigit(c) != 0; };
        return mid.size() >= 3 && mid.front() == '(' && mid.back() == ')'
            && std::all_of(mid.begin() + 1, mid.end() - 1, isDigit);
    };

    // .meta 归属其他任务的不动；缺失/损坏视为孤儿一并清理
    auto owned = [taskId](fs::path const& metaPath) {
        auto reader = schema::EnvelopeReader{kProgressMagic, kProgressVersion, metaPath};
        auto data   = reader.read<schema::MetaPaylad>();
        return !data || data->taskId == taskId;
    };

    for (auto const& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        auto const& path = entry.path();
        if (!isTempOfName(path.filename().string())) continue;
        if (!owned(path.string() + kMetaFileExt)) continue;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + kMetaFileExt, ec);
    }
}

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

    {
        std::lock_guard lock{wakeMutex_};
        exitWorkers_.store(true, std::memory_order_release);
    }
    wakeCv_.notify_all();
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

    // --- 脚本钩子: beforeFetchMeta ---
    // 元信息探测前，脚本可改写 url / userAgent / headers / proxy 等，或取消任务。
    // （继续/恢复下载会再次进入 prepareTask，钩子会重复触发，脚本应保持幂等。）
    if (auto hook = observer_->onBeforeFetchMeta(configure_); hook.cancelled) {
        auto const reason = hook.reason.empty() ? std::string{"canceled by script (beforeFetchMeta)"} : hook.reason;
        EDM_LOG_INFO(taskId, "Task rejected by script hook: beforeFetchMeta, reason={}", reason);
        setState(TaskState::Canceled);
        notifyError(reason);
        return makeStringError(reason);
    }

    if (!configure_->metaInfo) {
        EDM_LOG_DEBUG(taskId, "Fetching meta info from server");

        auto metaRes = edm::downloader::MetaInfoFetcher::fetchAll(configure_, isRunningFlag_);
        if (!metaRes) {
            // 暂停/取消引发的中断不是错误：不能写进数据库，也不该显示给用户
            if (isAbortRequested()) {
                EDM_LOG_INFO(taskId, "Meta info fetch aborted by pause/cancel request");
                return makeStringError("aborted by pause/cancel");
            }

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

    // --- 脚本钩子: afterFetchMeta ---
    // 元信息与文件名都已确定、磁盘预分配前：脚本可改写 fileName，或取消任务。
    if (auto hook = observer_->onAfterFetchMeta(configure_, meta); hook.cancelled) {
        auto const reason = hook.reason.empty() ? std::string{"canceled by script (afterFetchMeta)"} : hook.reason;
        EDM_LOG_INFO(taskId, "Task rejected by script hook: afterFetchMeta, reason={}", reason);
        setState(TaskState::Canceled);
        notifyError(reason);
        return makeStringError(reason);
    }

    // 文件名解析（含脚本改写）完成后立即通知宿主持久化，确保 UI/DB 中的文件名与磁盘一致。
    observer_->onFileNameChanged(configure_);

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
    // 临时名与 .meta 同前缀：.meta 记录 taskId，供重启续传接管与并发同名任务区分归属
    outFilePath_  = uniqueTempPath(configure_->saveDir, sanitizeFileName(configure_->fileName), taskId).string();
    metaFilePath_ = outFilePath_ + kMetaFileExt;
    EDM_LOG_DEBUG(taskId, "Output path: {}, Meta path: {}", outFilePath_, metaFilePath_);

    // 唯一化得到的临时名主干即任务对外文件名：回写 configure_->fileName 并通知宿主，
    // 使 UI/DB 显示名与磁盘上的 .edm 临时文件一致
    if (auto uniqueBase = std::filesystem::path(outFilePath_).filename().string();
        uniqueBase.size() > 4 && uniqueBase.ends_with(kTempFileExt)) {
        uniqueBase.resize(uniqueBase.size() - 4);
        if (uniqueBase != configure_->fileName) {
            configure_->fileName = std::move(uniqueBase);
            observer_->onFileNameChanged(configure_);
        }
    }

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

    // 归属不匹配：临时文件属于其他同名任务，不动
    if (data->taskId != configure_->id) {
        EDM_LOG_WARN(taskId, "Progress file invalid: taskId={}, expected={}", data->taskId, configure_->id);
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
        writer->taskId     = configure_->id;
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

void DownloadTask::rearmWorkers() {
    auto taskId = configure_->id;
    nextRangeIndex_.store(0, std::memory_order_relaxed);
    finalizeDone_.store(false, std::memory_order_release);
    exitWorkers_.store(false, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);
    isRunningFlag_->store(true, std::memory_order_release);

    int workerCount = configure_->metaInfo->supportRange ? static_cast<int>(configure_->threads) : 1;
    workerCount     = std::max(workerCount, 1);
    workerCount     = std::min<int>(workerCount, static_cast<int>(std::max<size_t>(state_->ranges.size(), 1)));
    workerCount     = std::min<int>(workerCount, static_cast<int>(DownloadState::kMaxWorkers));
    workerTarget_.store(workerCount, std::memory_order_relaxed);

    // 重置线程槽位（世代重启：状态/分片/累计字节全部归零，此后 UI 的速率
    // 差分基线从零开始）。此时 worker 已全部驻留或退出，写槽位无竞争。
    for (auto& slot : state_->workers) {
        slot.state.store(static_cast<int>(WorkerState::Idle), std::memory_order_relaxed);
        slot.rangeIndex.store(SIZE_MAX, std::memory_order_relaxed);
        slot.attempt.store(0, std::memory_order_relaxed);
        slot.downloaded.store(0, std::memory_order_relaxed);
    }
    state_->workerCount.store(workerCount, std::memory_order_release);

    // 线程随任务常驻：仅在首次启动时创建，此后驻留待命（pause 不退出），
    // start/resume 递增世代号唤醒驻留线程重新领取分片。
    if (!workersSpawned_.load(std::memory_order_acquire)) {
        workers_.reserve(static_cast<size_t>(workerCount));
        auto self = shared_from_this();
        for (int i = 0; i < workerCount; ++i) {
            workers_.emplace_back([self, slot = static_cast<size_t>(i)]() { self->workerLoop(slot); });
        }
        workersSpawned_.store(true, std::memory_order_release);
        workerLaunchCount_.fetch_add(1, std::memory_order_relaxed);
        EDM_LOG_INFO(taskId, "Spawned {} persistent worker(s) for {} range(s)", workerCount, state_->ranges.size());
    } else {
        EDM_LOG_INFO(taskId, "Rearming {} persistent worker(s) for {} range(s)", workerCount, state_->ranges.size());
    }

    {
        // 世代号自增须持 wakeMutex_：驻留 worker 的谓词检查与休眠在锁内原子完成，
        // 不持锁自增会与它们产生丢失唤醒窗口
        std::lock_guard lock{wakeMutex_};
        workGeneration_.fetch_add(1, std::memory_order_release);
    }
    wakeCv_.notify_all();
}

void DownloadTask::workerLoop(size_t slot) {
    auto taskId = configure_->id;
    EDM_LOG_DEBUG(taskId, "Worker {} spawned", slot);
    auto&          slotRef = state_->workers[slot];
    DownloadWorker worker(configure_, outFilePath_, isRunningFlag_, &slotRef);
    uint64_t       myGen = workGeneration_.load(std::memory_order_acquire);

    for (;;) {
        // 处理当前世代的全部分片（工作窃取式：原子游标领片）
        while (isRunningFlag_->load(std::memory_order_acquire)
               && myGen == workGeneration_.load(std::memory_order_acquire)) {
            auto idx = nextRangeIndex_.fetch_add(1, std::memory_order_relaxed);
            if (idx >= state_->ranges.size()) break;

            auto range = state_->ranges[idx];
            if (range->completed.load(std::memory_order_relaxed)) continue;

            EDM_LOG_DEBUG(taskId, "Worker {} picking up range {} [{}, {})", slot, idx, range->start, range->end + 1);
            // 领到分片进入请求中；收到首个数据块后由写回调切到"接收中"
            slotRef.state.store(static_cast<int>(WorkerState::Requesting), std::memory_order_relaxed);
            slotRef.rangeIndex.store(idx, std::memory_order_relaxed);
            slotRef.attempt.store(0, std::memory_order_relaxed);

            for (int attempt = 0; attempt <= kRetryCount && isRunningFlag_->load(std::memory_order_acquire);
                 ++attempt) {
                range->attempts.store(attempt, std::memory_order_relaxed);
                slotRef.attempt.store(attempt, std::memory_order_relaxed);

                auto res =
                    configure_->metaInfo->supportRange ? worker.downloadRange(range) : worker.downloadWholeFile(range);
                if (res) {
                    EDM_LOG_DEBUG(taskId, "Range {} completed successfully", idx);
                    break;
                }

                if (!isRunningFlag_->load(std::memory_order_acquire)) break;
                if (attempt == kRetryCount) {
                    EDM_LOG_ERROR(
                        taskId,
                        "Range {} failed after {} attempts: {}",
                        idx,
                        attempt + 1,
                        res.error().message()
                    );
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
                    slotRef.state.store(static_cast<int>(WorkerState::Retrying), std::memory_order_relaxed);
                    std::this_thread::sleep_for(std::chrono::milliseconds(300 * (attempt + 1)));
                    slotRef.state.store(static_cast<int>(WorkerState::Downloading), std::memory_order_relaxed);
                }
            }
        }
        slotRef.rangeIndex.store(SIZE_MAX, std::memory_order_relaxed);

        // pause/失败/世代完成 - 驻留待命；cancel/析构 - 退出线程
        if (exitWorkers_.load(std::memory_order_acquire)) {
            slotRef.state.store(static_cast<int>(WorkerState::Stopped), std::memory_order_relaxed);
            EDM_LOG_DEBUG(taskId, "Worker {} exiting (requested)", slot);
            return;
        }

        // 分片取完 = 已完成；被暂停/失败打断 = 已停止（失败的任务不该显示"已完成"）
        bool const interrupted = !isRunningFlag_->load(std::memory_order_acquire);
        slotRef.state.store(
            static_cast<int>(interrupted ? WorkerState::Stopped : WorkerState::Finished),
            std::memory_order_relaxed
        );

        // 驻留等待期不持有连接：TCP 立即归还，resume 后传输前按需重建
        worker.disconnect();

        {
            // 驻留登记须持 wakeMutex_：waitAllParked 的谓词检查与休眠在锁内原子完成
            std::lock_guard lock{wakeMutex_};
            parkedCount_.fetch_add(1, std::memory_order_relaxed);
        }
        // 最后一个驻留者负责世代收尾（暂停落盘 / 完成校验改名 / 失败标记）
        if (parkedCount_.load(std::memory_order_acquire) == workerTarget_.load(std::memory_order_relaxed)) {
            if (!finalizeDone_.exchange(true, std::memory_order_acq_rel)) {
                finalizeTask();
            }
            wakeCv_.notify_all();
        }
        EDM_LOG_DEBUG(taskId, "Worker {} parked ({})", slot, interrupted ? "stopped" : "finished");

        std::unique_lock lock{wakeMutex_};
        wakeCv_.wait(lock, [&] {
            return exitWorkers_.load(std::memory_order_acquire)
                || myGen != workGeneration_.load(std::memory_order_acquire);
        });
        parkedCount_.fetch_sub(1, std::memory_order_relaxed);
        if (exitWorkers_.load(std::memory_order_acquire)) {
            slotRef.state.store(static_cast<int>(WorkerState::Stopped), std::memory_order_relaxed);
            EDM_LOG_DEBUG(taskId, "Worker {} exiting (requested while parked)", slot);
            return;
        }
        myGen = workGeneration_.load(std::memory_order_acquire);
        EDM_LOG_DEBUG(taskId, "Worker {} rearmed for generation {}", slot, myGen);
    }
}

void DownloadTask::waitAllParked() {
    std::unique_lock lock{wakeMutex_};
    wakeCv_.wait(lock, [&] {
        return parkedCount_.load(std::memory_order_acquire) == workerTarget_.load(std::memory_order_relaxed);
    });
}

void DownloadTask::joinWorkers() {
    // 终态任务被 Dispatcher 回收后，最后一个 worker 释放 shared_ptr 会在它自己的
    // 线程上触发 ~DownloadTask；此时 join 自身会抛 std::system_error，必须跳过并 detach。
    auto const self = std::this_thread::get_id();
    for (auto& worker : workers_) {
        if (!worker.joinable()) continue;
        if (worker.get_id() == self) {
            worker.detach();
            continue;
        }
        worker.join();
    }
    workers_.clear();
    workersSpawned_.store(false, std::memory_order_release);
}

Expected<> DownloadTask::start() {
    // start/resume 只会在全体 worker 驻留（pause）或退出（cancel）后进入，
    // 此时重建 ranges 不会与任何 worker 竞争。
    if (workersSpawned_.load(std::memory_order_acquire)) {
        waitAllParked();
    }

    auto const taskId = configure_->id;

    {
        std::unique_lock lock{mutex_};

        auto state = state_->state();
        EDM_LOG_INFO(taskId, "Starting task, currentState={}", static_cast<int>(state));
        // 准备阶段没有持锁，可能被重复 start（例如 UI 把"准备中"当成可继续的任务）。
        // 重复进入会并发执行 prepareTask，必须挡掉。
        if (preparing_.load(std::memory_order_acquire)) {
            EDM_LOG_WARN(taskId, "Task is already preparing, ignoring duplicate start");
            return makeStringError("Task is already preparing");
        }
        if (state != TaskState::Pending && state != TaskState::Paused && state != TaskState::Failed
            && state != TaskState::Canceled) {
            EDM_LOG_WARN(taskId, "Task is not startable from current state {}", static_cast<int>(state));
            return makeStringError("Task is not startable from current state");
        }

        cancelRequested_.store(false, std::memory_order_release);
        prepareAbort_.store(false, std::memory_order_release);
        // 探测阶段也要能被中断：isRunningFlag_ 同时是 pause/cancel 的中止信号
        isRunningFlag_->store(true, std::memory_order_release);
        preparing_.store(true, std::memory_order_release);
        // 准备中要能被 UI 识别（"等待中"会被误当成可继续的任务，导致重复 start）
        setState(TaskState::Preparing);
    }

    // 准备阶段（网络探测 + 磁盘预分配）**不持锁**执行：
    // 否则 pause()/cancel() 会一直等这把锁，用户看到的就是"任务不响应任何操作"。
    // 此时任务状态仍是 Pending/Paused，UI 不会去读 ranges，无并发读取风险。
    auto prepared = prepareTask();

    std::unique_lock lock{mutex_};
    preparing_.store(false, std::memory_order_release);

    if (cancelRequested_.load(std::memory_order_acquire)) {
        EDM_LOG_INFO(taskId, "Task canceled during preparation");
        removeTempFiles();
        setState(TaskState::Canceled);
        return {};
    }
    if (prepareAbort_.load(std::memory_order_acquire) || !isRunningFlag_->load(std::memory_order_acquire)) {
        EDM_LOG_INFO(taskId, "Task paused during preparation");
        setState(TaskState::Paused);
        return {};
    }
    if (!prepared) {
        EDM_LOG_ERROR(taskId, "Task preparation failed: {}", prepared.error().message());
        return prepared; // prepareTask 内部已 notifyError + 置 Failed
    }

    setState(TaskState::Running);
    rearmWorkers();
    EDM_LOG_INFO(taskId, "Task started, workers rearmed");
    return {};
}

void DownloadTask::removeTempFiles() {
    std::error_code ec;
    if (!outFilePath_.empty()) std::filesystem::remove(outFilePath_, ec);
    if (!metaFilePath_.empty()) std::filesystem::remove(metaFilePath_, ec);
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

    auto finalName  = configure_->fileName.empty() ? fileNameFromUrl(configure_->url) : configure_->fileName;
    auto targetPath = std::filesystem::path(configure_->saveDir) / sanitizeFileName(finalName);
    // 覆盖同名文件时全程写 .edm 临时文件、校验通过后才改名，原文件不会被写坏；
    // 不覆盖时另存为 name(1).ext
    auto            finalPath = configure_->overwriteExisting ? targetPath : uniqueFinalPath(targetPath);
    std::error_code ec;
    std::filesystem::rename(outFilePath_, finalPath, ec);
    if (ec && configure_->overwriteExisting) {
        // 平台差异兜底：rename 不支持覆盖时，先删目标再改名
        std::error_code removeEc;
        std::filesystem::remove(finalPath, removeEc);
        ec.clear();
        std::filesystem::rename(outFilePath_, finalPath, ec);
    }
    if (ec) {
        EDM_LOG_ERROR(taskId, "Failed to move completed file: {}", ec.message());
        notifyError("Failed to move completed file: " + ec.message());
        setState(TaskState::Failed);
        saveProgress();
        return;
    }

    // 完成重命名后再次唯一化的最终名同样回写并通知，UI/DB 与落盘文件名保持一致
    if (auto finalBase = finalPath.filename().string(); finalBase != configure_->fileName) {
        configure_->fileName = std::move(finalBase);
        observer_->onFileNameChanged(configure_);
    }

    std::filesystem::remove(metaFilePath_, ec);
    currentSpeed_.store(0.0, std::memory_order_relaxed);
    state_->setSpeed(0.0);
    EDM_LOG_INFO(taskId, "Download completed: {}", finalPath.string());
    setState(TaskState::Finished);
}

Expected<> DownloadTask::pause() {
    // 准备阶段（元信息探测/磁盘预分配）start() 不持锁，这里也必须只用原子量请求中止，
    // 否则用户点了暂停却要等整个探测结束，表现为"任务不响应任何操作"。
    if (preparing_.load(std::memory_order_acquire)) {
        EDM_LOG_INFO(configure_->id, "Pausing task during preparation");
        prepareAbort_.store(true, std::memory_order_release);
        isRunningFlag_->store(false, std::memory_order_release);
        return {}; // 最终落定为 Paused 由 start() 在探测返回后完成
    }

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
    waitAllParked();
    saveProgress();
    EDM_LOG_INFO(configure_->id, "Task paused, progress saved");
    return {};
}

Expected<> DownloadTask::resume() { return start(); }

Expected<> DownloadTask::cancel() {
    if (preparing_.load(std::memory_order_acquire)) {
        EDM_LOG_INFO(configure_->id, "Canceling task during preparation");
        cancelRequested_.store(true, std::memory_order_release);
        prepareAbort_.store(true, std::memory_order_release);
        isRunningFlag_->store(false, std::memory_order_release);
        return {}; // 清理与置 Canceled 由 start() 在探测返回后完成
    }

    {
        std::unique_lock lock{mutex_};
        auto             state = state_->state();
        if (state == TaskState::Canceled) return {}; // 幂等
        if (state == TaskState::Finished) {
            EDM_LOG_WARN(configure_->id, "Cannot cancel: task already finished");
            return makeStringError("Finished tasks cannot be canceled");
        }
        EDM_LOG_INFO(configure_->id, "Canceling task");
        cancelRequested_.store(true, std::memory_order_release);
        setState(TaskState::Canceled);
        isRunningFlag_->store(false, std::memory_order_release);
    }
    {
        std::lock_guard lock{wakeMutex_};
        exitWorkers_.store(true, std::memory_order_release);
    }
    wakeCv_.notify_all();
    joinWorkers();
    removeTempFiles();
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
    // 完全不碰 mutex_：prepareTask()/finalizeTask() 会长时间独占它（网络元信息探测、
    // 整文件 MD5），而本函数由 UI 的 1s 刷新定时器调用，一旦抢锁就会卡住界面。
    // 这里只读原子状态与结构稳定的 state_->ranges（仅在 Running 期间遍历；
    // 结构性重建只发生在 Pending 阶段的 prepareTask 里）。
    if (state_->state() != TaskState::Running) {
        currentSpeed_.store(0.0, std::memory_order_relaxed);
        state_->setSpeed(0.0);
        return;
    }

    auto const total = getDownloadedBytes();
    auto const last  = lastDownloadedBytes_.exchange(total, std::memory_order_relaxed);
    auto const speed = static_cast<double>(total - last);
    currentSpeed_.store(speed, std::memory_order_relaxed);
    state_->setDownloadedBytes(total);
    state_->setSpeed(speed);
}

std::shared_ptr<edm::TaskContext> DownloadTask::getContext() const { return configure_; }
std::shared_ptr<DownloadState>    DownloadTask::getStateObject() const { return state_; }

int64_t DownloadTask::getDownloadedBytes() const {
    int64_t total = 0;
    for (auto const& r : state_->ranges) {
        total += r->downloaded.load(std::memory_order_relaxed);
    }
    return total;
}

std::vector<std::shared_ptr<DownloadRange>> DownloadTask::getRanges() const { return state_->ranges; }

double DownloadTask::getSpeed() const { return currentSpeed_.load(std::memory_order_relaxed); }

uint32_t DownloadTask::workerLaunchCount() const { return workerLaunchCount_.load(std::memory_order_relaxed); }

TaskState DownloadTask::getState() const {
    std::shared_lock lock{mutex_};
    return state_->state();
}

} // namespace edm::downloader
