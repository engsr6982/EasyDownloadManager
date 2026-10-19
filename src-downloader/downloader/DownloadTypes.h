#pragma once
#include "Global.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

namespace edm {

struct FetchedMetaInfo {
    std::string                finalUrl;            // 最终请求的 URL (经过重定向后)
    bool                       supportRange{false}; // 是否支持断点续传
    std::optional<FileSize>    fileSize;            // 文件大小 (未知则为空)
    std::optional<std::string> contentType;         // MIME 类型
    std::optional<std::string> etag;                // ETag (校验文件是否被修改)
    std::optional<std::string> lastModified;        // 最后修改时间
    std::optional<std::string> contentDisposition;  // 服务端的建议文件名
    std::optional<std::string> md5;                 // 文件完整性校验 MD5 (Content-MD5 或 x-cos-meta-md5)
};

struct TaskContext {
    TaskId const               id{kInvalidTaskID};           // 任务唯一标识符
    std::string const          url;                          // 要下载的文件地址
    std::string const          saveDir;                      // 文件保存目录
    AvailableThreads           threads{kDefaultThreadCount}; // 下载线程数
    BandLimit                  bandLimit{kDefaultBandLimit}; // 下载带宽限制
    std::string                userAgent{kDefaultUserAgent}; // 下载时使用的User-Agent
    std::optional<std::string> origin;                       // 下载时使用的Origin
    std::optional<std::string> referer;                      // 下载时使用的Referer
    std::optional<std::string> cookie;                       // 下载时使用的Cookie
    std::optional<std::string> proxyUrl;                     // 代理服务器地址
    std::vector<std::string>   headers;                      // 下载时使用的自定义HTTP头

    std::optional<FetchedMetaInfo> metaInfo; // 下载前获取到的文件元信息

    /// 解析后的最终文件名（来自 Content-Disposition 或 URL）。
    /// 由 DownloadTask::prepareTask() 在获取 meta 之后填入，供引擎写盘和 UI 持久化使用。
    std::string fileName;

    /// 当前任务状态，由 DownloadTask::setState() 实时更新，供 onTaskChanged 持久化到 DB。
    TaskState state{TaskState::Pending};

    /// 完成后是否覆盖同名文件（"重新下载"语义）。
    /// 为 false 时沿用旧行为：目标已存在则另存为 file(1).ext。
    /// 下载始终先写 <name>.edm 临时文件，校验通过后才改名，覆盖不会损坏原文件。
    bool overwriteExisting{false};

    explicit TaskContext(TaskId const id, std::string url, std::string saveDir)
    : id(id),
      url(std::move(url)),
      saveDir(std::move(saveDir)) {}
};

using SharedTaskContext = std::shared_ptr<TaskContext>;

namespace downloader {

// 下载分片信息
struct DownloadRange {
    int64_t              start{0};
    int64_t              end{0};
    std::atomic<int64_t> downloaded{0};
    std::atomic<int>     attempts{0};
    std::atomic<bool>    completed{false};

    DownloadRange(int64_t s, int64_t e) : start(s), end(e) {}

    [[nodiscard]] int64_t size() const noexcept { return end >= start ? end - start + 1 : 0; }
};

// 单个下载线程所处的阶段
enum class WorkerState {
    Idle        = 0, // 已启动但还没领到分片
    Requesting  = 1, // 已领到分片，正在建立连接 / 等待响应
    Downloading = 2, // 正在接收数据（收到首个数据块后）
    Retrying    = 3, // 当前分片失败后的退避重试
    Finished    = 4, // 分片取完，正常退出
    Stopped     = 5  // 因暂停 / 取消 / 失败而中途退出
};

/// 一个下载线程的实时状态。由对应的 worker 独占写入，UI 只读。
/// 分片与线程并非固定绑定（引擎是工作窃取式分片池），所以这里记录的是
/// 该线程"此刻正在处理哪个分片"，而不是"负责哪些分片"。
struct WorkerSlot {
    std::atomic<int>     state{static_cast<int>(WorkerState::Idle)};
    std::atomic<size_t>  rangeIndex{SIZE_MAX}; // 当前分片下标；SIZE_MAX 表示无
    std::atomic<int>     attempt{0};           // 当前分片第几次尝试
    std::atomic<int64_t> downloaded{0};        // 该线程累计传输字节（UI 据此算每线程速率）
};

// 下载任务状态
class DownloadState {
    std::atomic<TaskState> state_{TaskState::Pending};
    std::atomic<int64_t>   downloadedBytes_{0};
    std::atomic<int64_t>   totalBytes_{kInvalidFileSize};
    std::atomic<double>    speedBytesPerSecond_{0.0};

public:
    /// 线程槽位上限，等于 AvailableThreads 的最大值。
    static constexpr size_t kMaxWorkers = 32;

    std::vector<std::shared_ptr<DownloadRange>> ranges;

    /// 固定容量，避免读端（UI 线程）看到 vector 重新分配。
    /// 仅前 workerCount 个槽位有效。
    std::array<WorkerSlot, kMaxWorkers> workers{};
    std::atomic<int>                    workerCount{0};

    void                    setState(TaskState state) noexcept { state_.store(state, std::memory_order_release); }
    [[nodiscard]] TaskState state() const noexcept {
        return static_cast<TaskState>(state_.load(std::memory_order_acquire));
    }

    void setDownloadedBytes(int64_t bytes) noexcept { downloadedBytes_.store(bytes, std::memory_order_relaxed); }
    [[nodiscard]] int64_t downloadedBytes() const noexcept { return downloadedBytes_.load(std::memory_order_relaxed); }

    void                  setTotalBytes(int64_t bytes) noexcept { totalBytes_.store(bytes, std::memory_order_relaxed); }
    [[nodiscard]] int64_t totalBytes() const noexcept { return totalBytes_.load(std::memory_order_relaxed); }

    void setSpeed(double bytesPerSecond) noexcept {
        speedBytesPerSecond_.store(bytesPerSecond, std::memory_order_relaxed);
    }
    [[nodiscard]] double speed() const noexcept { return speedBytesPerSecond_.load(std::memory_order_relaxed); }
};

} // namespace downloader

} // namespace edm