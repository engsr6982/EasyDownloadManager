#include "DownloadWorker.h"
#include "CurlEx.h"
#include "DownloadState.h"
#include "TaskConfigure.h"

#include <algorithm>
#include <cstdio>
#include <fmt/format.h>
#ifdef _WIN32
#include <share.h>
#endif

namespace edm ::downloader {


DownloadWorker::DownloadWorker(
    std::shared_ptr<TaskConfigure>     config,
    std::string                        outFilePath,
    std::shared_ptr<std::atomic<bool>> isTaskRunning
)
: config_(std::move(config)),
  outFilePath_(std::move(outFilePath)),
  isTaskRunning_(std::move(isTaskRunning)) {}
DownloadWorker::~DownloadWorker() = default;

struct WorkerContext {
    std::FILE*         fp;
    DownloadRange*     range;
    std::atomic<bool>* isTaskRunning;
};

size_t DownloadWorker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto ctx = static_cast<WorkerContext*>(userdata);

    // 如果上层调用了 pause() 或 cancel()，isRunning 变成 false
    if (!ctx->isTaskRunning->load(std::memory_order_relaxed)) {
        return 0; // 中断 libcurl 传输
    }

    size_t totalBytes = size * nmemb;

    // 写入文件 (不需要加锁，因为每个 worker 有自己独立的 fp 和互不重叠的文件区块)
    if (std::fwrite(ptr, size, nmemb, ctx->fp) != nmemb) {
        return 0; // 写入磁盘失败，中断传输
    }

    // 更新进度条用的原子变量
    ctx->range->downloaded.fetch_add(totalBytes, std::memory_order_relaxed);

    return totalBytes;
}

Expected<> DownloadWorker::downloadRange(std::shared_ptr<DownloadRange> const& range) const {
    // 打开独立的文件句柄 (rb+ 允许随机读写，不截断文件)
    std::FILE* fp = nullptr;
#ifdef _WIN32
    fp = _fsopen(outFilePath_.c_str(), "rb+", _SH_DENYNO);
#else
    fp = std::fopen(outFilePath_.c_str(), "rb+");
#endif

    if (!fp) {
        return makeStringError("Failed to open temp file for range writing: " + outFilePath_);
    }

    // 定位到当前应该写入的偏移量
    int64_t currentOffset = range->start + range->downloaded.load(std::memory_order_relaxed);
    if (range->end >= range->start && currentOffset > range->end) {
        std::fclose(fp);
        range->completed.store(true, std::memory_order_relaxed);
        return {};
    }

#ifdef _WIN32
    _fseeki64(fp, currentOffset, SEEK_SET); // Windows 支持 >2GB 文件的 fseek
#else
    fseeko(fp, currentOffset, SEEK_SET); // Linux/macOS 支持 >2GB 文件的 fseek
#endif

    // 配置 CURL
    assert(config_);
    auto curlExRes = config_->newCurl();
    if (!curlExRes) {
        std::fclose(fp);
        return forwardError(curlExRes.error());
    }
    CurlEx curl = std::move(curlExRes.value());

    // 告诉服务器我们要下载的精确范围
    std::string rangeStr = fmt::format("{}-{}", currentOffset, range->end);
    curl.setOpt(CURLOPT_RANGE, rangeStr.c_str());

    // 设置写入回调
    WorkerContext ctx{fp, range.get(), isTaskRunning_.get()};
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &ctx);

    // 开始下载
    auto performRes = curl.perform();
    std::fflush(fp);
    std::fclose(fp);

    if (!performRes) {
        return forwardError(performRes.error());
    }

    long responseCode = 0;
    if (auto responseCodeRes = curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode); !responseCodeRes) {
        return forwardError(responseCodeRes.error());
    }
    if (responseCode != 206) {
        return makeStringError(fmt::format("Server did not honor range request, HTTP {}", responseCode));
    }

    int64_t expected = range->size();
    if (range->downloaded.load(std::memory_order_relaxed) != expected) {
        return makeStringError("Range download ended before expected byte count");
    }

    range->completed.store(true, std::memory_order_relaxed);
    return {};
}

Expected<> DownloadWorker::downloadWholeFile(std::shared_ptr<DownloadRange> const& range) const {
    std::FILE* fp = nullptr;
#ifdef _WIN32
    // NOTE: "rb+" is used instead of "wb" to preserve the pre-allocated file content.
    // "wb" would truncate the file to zero length, destroying the allocated space.
    // _fsopen with _SH_DENYNO additionally allows other processes to access the file concurrently.
    fp = _fsopen(outFilePath_.c_str(), "rb+", _SH_DENYNO);
#else
    fp = std::fopen(outFilePath_.c_str(), "rb+");
#endif

    if (!fp) {
        return makeStringError("Failed to open temp file for writing: " + outFilePath_);
    }

    auto curlExRes = config_->newCurl();
    if (!curlExRes) {
        std::fclose(fp);
        return forwardError(curlExRes.error());
    }
    CurlEx curl = std::move(curlExRes.value());

    WorkerContext ctx{fp, range.get(), isTaskRunning_.get()};
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &ctx);

    auto performRes = curl.perform();
    std::fflush(fp);
    std::fclose(fp);

    if (!performRes) {
        return forwardError(performRes.error());
    }

    range->completed.store(true, std::memory_order_relaxed);
    return {};
}

} // namespace edm::downloader
