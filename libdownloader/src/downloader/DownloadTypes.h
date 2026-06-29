#pragma once
#include "Global.h"

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

struct TaskConfigure {
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

    explicit TaskConfigure(TaskId const id, std::string url, std::string saveDir)
    : id(id),
      url(std::move(url)),
      saveDir(std::move(saveDir)) {}
};

using SharedTaskConfigure = std::shared_ptr<TaskConfigure>;

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

// 下载任务状态
class DownloadState {
    std::atomic<TaskState> state_{TaskState::Pending};
    std::atomic<int64_t>   downloadedBytes_{0};
    std::atomic<int64_t>   totalBytes_{kInvalidFileSize};
    std::atomic<double>    speedBytesPerSecond_{0.0};

    mutable std::shared_mutex stringsMutex_;
    std::string               errorMessage_;
    std::string               outputPath_;

public:
    std::vector<std::shared_ptr<DownloadRange>> ranges;

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

    void setErrorMessage(std::string message) {
        std::unique_lock lock(stringsMutex_);
        errorMessage_ = std::move(message);
    }
    [[nodiscard]] std::string errorMessage() const {
        std::shared_lock lock(stringsMutex_);
        return errorMessage_;
    }

    void setOutputPath(std::string path) {
        std::unique_lock lock(stringsMutex_);
        outputPath_ = std::move(path);
    }
    [[nodiscard]] std::string outputPath() const {
        std::shared_lock lock(stringsMutex_);
        return outputPath_;
    }
};

} // namespace downloader

} // namespace edm