#include "DownloadWorker.h"
#include "CurlEx.h"
#include "DownloadTypes.h"
#include "utils/TaskLogger.h"

#include <cstdio>
#include <fmt/format.h>

#ifdef _WIN32
#include <share.h>
#endif

namespace edm ::downloader {


DownloadWorker::DownloadWorker(
    std::shared_ptr<TaskConfigure>     config,
    std::string                        outFilePath,
    std::shared_ptr<std::atomic<bool>> isTaskRunning,
    TaskId                             taskId
)
: config_(std::move(config)),
  outFilePath_(std::move(outFilePath)),
  isTaskRunning_(std::move(isTaskRunning)) {}

DownloadWorker::~DownloadWorker() = default;

struct WorkerContext {
    std::FILE*         fp;
    DownloadRange*     range;
    std::atomic<bool>* isTaskRunning;
    bool               rangeValidated{false}; // 为 true 表示服务端确认返回了 Content-Range (206 响应)
};

size_t DownloadWorker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto ctx = static_cast<WorkerContext*>(userdata);

    // 如果上层调用了 pause() 或 cancel()，isRunning 变成 false
    if (!ctx->isTaskRunning->load(std::memory_order_relaxed)) {
        return 0; // 中断 libcurl 传输
    }

    // 如果尚未验证通过 Content-Range 头，说明服务端未按 Range 请求返回 206，
    // 此时 body 数据是完整文件而非分片，立即中断以防止数据写入错误的文件偏移。
    if (!ctx->rangeValidated) {
        return 0;
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

size_t DownloadWorker::headerCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto ctx = static_cast<WorkerContext*>(userdata);

    // 检查响应头中是否包含 Content-Range
    std::string_view data(ptr, size * nmemb);
    if (data.find("Content-Range:") != std::string_view::npos ||
        data.find("content-range:") != std::string_view::npos) {
        ctx->rangeValidated = true;
    }
    return size * nmemb;
}

Expected<> DownloadWorker::downloadRange(std::shared_ptr<DownloadRange> const& range) const {
    EDM_LOG_DEBUG(
        config_->id,
        "downloadRange: opening file, offset={}, range=[{}, {})",
        range->start + range->downloaded.load(std::memory_order_relaxed),
        range->start,
        range->end + 1
    );

    // 打开独立的文件句柄 (rb+ 允许随机读写，不截断文件)
    std::FILE* fp = nullptr;
#ifdef _WIN32
    fp = _fsopen(outFilePath_.c_str(), "rb+", _SH_DENYNO);
#else
    fp = std::fopen(outFilePath_.c_str(), "rb+");
#endif

    if (!fp) {
        EDM_LOG_ERROR(config_->id, "Failed to open temp file for range writing: {}", outFilePath_);
        return makeStringError("Failed to open temp file for range writing: " + outFilePath_);
    }

    // 定位到当前应该写入的偏移量
    int64_t currentOffset = range->start + range->downloaded.load(std::memory_order_relaxed);
    if (range->end >= range->start && currentOffset > range->end) {
        std::fclose(fp);
        range->completed.store(true, std::memory_order_relaxed);
        EDM_LOG_DEBUG(config_->id, "downloadRange: already completed (offset {} > end {})", currentOffset, range->end);
        return {};
    }

#ifdef _WIN32
    _fseeki64(fp, currentOffset, SEEK_SET); // Windows 支持 >2GB 文件的 fseek
#else
    fseeko(fp, currentOffset, SEEK_SET); // Linux/macOS 支持 >2GB 文件的 fseek
#endif

    // 配置 CURL
    assert(config_);
    auto curlExRes = CurlEx::fromConfigure(config_);
    if (!curlExRes) {
        std::fclose(fp);
        EDM_LOG_ERROR(config_->id, "downloadRange: failed to create CURL handle: {}", curlExRes.error().message());
        return forwardError(curlExRes.error());
    }
    CurlEx curl = std::move(curlExRes.value());

    // 告诉服务器我们要下载的精确范围
    std::string rangeStr = fmt::format("{}-{}", currentOffset, range->end);
    curl.setOpt(CURLOPT_RANGE, rangeStr.c_str());
    EDM_LOG_TRACE(config_->id, "downloadRange: requesting bytes {}", rangeStr);

    WorkerContext ctx{fp, range.get(), isTaskRunning_.get(), /*rangeValidated=*/false};
    curl.setOpt(CURLOPT_HEADERFUNCTION, headerCallback);
    curl.setOpt(CURLOPT_HEADERDATA, &ctx);
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &ctx);

    // 开始下载
    auto performRes = curl.perform();
    std::fflush(fp);
    std::fclose(fp);

    if (!performRes) {
        EDM_LOG_ERROR(config_->id, "downloadRange: CURL perform failed: {}", performRes.error().message());
        return forwardError(performRes.error());
    }

    long responseCode = 0;
    if (auto responseCodeRes = curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode); !responseCodeRes) {
        EDM_LOG_ERROR(config_->id, "downloadRange: failed to get response code: {}", responseCodeRes.error().message());
        return forwardError(responseCodeRes.error());
    }
    if (responseCode != 206) {
        EDM_LOG_ERROR(config_->id, "downloadRange: server returned HTTP {} instead of 206", responseCode);
        return makeStringError(fmt::format("Server did not honor range request, HTTP {}", responseCode));
    }

    int64_t expected = range->size();
    if (range->downloaded.load(std::memory_order_relaxed) != expected) {
        EDM_LOG_ERROR(
            config_->id,
            "downloadRange: incomplete range, downloaded {} / {} bytes",
            range->downloaded.load(std::memory_order_relaxed),
            expected
        );
        return makeStringError("Range download ended before expected byte count");
    }

    EDM_LOG_DEBUG(config_->id, "downloadRange: completed [{}, {})", range->start, range->end + 1);
    range->completed.store(true, std::memory_order_relaxed);
    return {};
}

Expected<> DownloadWorker::downloadWholeFile(std::shared_ptr<DownloadRange> const& range) const {
    EDM_LOG_DEBUG(config_->id, "downloadWholeFile: starting whole file download");

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
        EDM_LOG_ERROR(config_->id, "Failed to open temp file for writing: {}", outFilePath_);
        return makeStringError("Failed to open temp file for writing: " + outFilePath_);
    }

    auto curlExRes = CurlEx::fromConfigure(config_);
    if (!curlExRes) {
        std::fclose(fp);
        EDM_LOG_ERROR(config_->id, "downloadWholeFile: failed to create CURL handle: {}", curlExRes.error().message());
        return forwardError(curlExRes.error());
    }
    CurlEx curl = std::move(curlExRes.value());

    // downloadWholeFile 不需要范围校验（从偏移 0 写全文件是正确的）
    WorkerContext ctx{fp, range.get(), isTaskRunning_.get(), /*rangeValidated=*/true};
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &ctx);

    auto performRes = curl.perform();
    std::fflush(fp);
    std::fclose(fp);

    if (!performRes) {
        EDM_LOG_ERROR(config_->id, "downloadWholeFile: CURL perform failed: {}", performRes.error().message());
        return forwardError(performRes.error());
    }

    long responseCode = 0;
    if (auto responseCodeRes = curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode); !responseCodeRes) {
        EDM_LOG_ERROR(config_->id, "downloadWholeFile: failed to get response code: {}", responseCodeRes.error().message());
        return forwardError(responseCodeRes.error());
    }
    if (responseCode != 200 && responseCode != 206) {
        EDM_LOG_ERROR(config_->id, "downloadWholeFile: unexpected HTTP {}", responseCode);
        return makeStringError(fmt::format("Unexpected HTTP response: {}", responseCode));
    }

    EDM_LOG_INFO(
        config_->id,
        "downloadWholeFile: completed, {} bytes downloaded",
        range->downloaded.load(std::memory_order_relaxed)
    );
    range->completed.store(true, std::memory_order_relaxed);
    return {};
}

} // namespace edm::downloader
