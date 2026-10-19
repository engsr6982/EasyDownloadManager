#include "MetaInfoFetcher.h"
#include "CurlEx.h"
#include "DownloadTypes.h"
#include "Global.h"
#include "utils/TaskLogger.h"

#include <chrono>

namespace edm ::downloader {

namespace {

/// 探测阶段允许多丢弃的响应体上限。服务器若无视 Range: 0-0 直接返回 200，
/// 不设上限就会把整个文件白白下载一遍（大文件能让"新建任务"卡住几分钟）。
constexpr int64_t kMaxDiscardBytes = 64 * 1024;

struct DiscardCtx {
    std::atomic<bool> const* abortFlag{nullptr}; // 为 false 表示请求暂停/取消
    int64_t                  received{0};
};

size_t discardCallback(char* /*ptr*/, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<DiscardCtx*>(userdata);

    // 上层请求暂停/取消 → 立刻中断探测
    if (ctx->abortFlag && !ctx->abortFlag->load(std::memory_order_relaxed)) return 0;

    ctx->received += static_cast<int64_t>(size * nmemb);
    // 已经拿到响应头，body 没必要继续丢；返回 0 让 curl 以写错误收尾，
    // 调用方凭响应码继续解析元信息。
    if (ctx->received > kMaxDiscardBytes) return 0;

    return size * nmemb;
}

} // namespace

Expected<FetchedMetaInfo> MetaInfoFetcher::fetchAll(
    std::shared_ptr<TaskContext> const&       configure,
    std::shared_ptr<std::atomic<bool>> const& abortFlag
) {
    if (!configure) {
        EDM_LOG_ERROR(kInvalidTaskID, "MetaInfoFetcher: configure is null");
        return makeStringError("configure is null");
    }

    EDM_LOG_DEBUG(configure->id, "MetaInfoFetcher: fetching meta from {}", configure->url);

    auto ex_res = CurlEx::fromContext(configure);
    if (!ex_res) {
        EDM_LOG_ERROR(configure->id, "MetaInfoFetcher: failed to create CURL handle: {}", ex_res.error().message());
        return forwardError(ex_res.error());
    }

    CurlEx curl = std::move(ex_res.value());

    // 显式将当前探测请求的 FAILONERROR 关闭（设为 0L）。
    // 因为 Meta 探测阶段的职责是"观察者"，无论服务器返回 200、206 还是 403，
    // 都是极其重要的业务信号，必须放行 perform()，让状态码能安全流转到后方的 getInfo 中。
    curl.setOpt(CURLOPT_FAILONERROR, 0L);

    curl.setOpt(CURLOPT_HEADER, 1L);
    curl.setOpt(CURLOPT_RANGE, "0-0");
    DiscardCtx discardCtx{abortFlag ? abortFlag.get() : nullptr, 0};
    curl.setOpt(CURLOPT_WRITEFUNCTION, discardCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &discardCtx);
    if (abortFlag) {
        // 连接阶段也能被暂停/取消打断（不必等连接超时）
        curl.setAbortFlag(abortFlag.get());
    }

    EDM_LOG_TRACE(configure->id, "MetaInfoFetcher: performing HEAD-like request with Range: 0-0");

    auto const startedAt = std::chrono::steady_clock::now();
    auto       performRes = curl.perform();

    // 探测阶段的 body 是丢弃的，写回调主动中断（服务器不支持 Range 时）会以
    // CURLE_WRITE_ERROR 收尾。只要拿到了响应码，就仍可正常解析元信息。
    long responseCode = 0;
    auto codeRes      = curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode);
    if (!performRes && !(codeRes && responseCode > 0)) {
        EDM_LOG_ERROR(configure->id, "MetaInfoFetcher: CURL perform failed: {}", performRes.error().message());
        return forwardError(performRes.error());
    }
    if (abortFlag && !abortFlag->load(std::memory_order_relaxed)) {
        EDM_LOG_INFO(configure->id, "MetaInfoFetcher: probe aborted by pause/cancel request");
        return makeStringError("aborted by pause/cancel");
    }
    auto const elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
    EDM_LOG_INFO(configure->id, "MetaInfoFetcher: probe finished in {} ms", elapsedMs);

    FetchedMetaInfo info;

    // 获取最终重定向的 URL
    char* fUrl = nullptr;
    if (curl.getInfo(CURLINFO_EFFECTIVE_URL, &fUrl) && fUrl) {
        info.finalUrl = fUrl;
    } else {
        info.finalUrl = configure->url;
    }
    EDM_LOG_DEBUG(configure->id, "MetaInfoFetcher: effective URL = {}", info.finalUrl);

    // 是否支持 Range 及真实大小
    EDM_LOG_DEBUG(configure->id, "MetaInfoFetcher: HTTP response code = {}", responseCode);

    if (responseCode >= 400) {
        EDM_LOG_ERROR(configure->id, "MetaInfoFetcher: HTTP error {}", responseCode);
        return makeStringError("HTTP error: " + std::to_string(responseCode));
    }

    if (responseCode == 206) {
        // 206 Partial Content: 支持断点续传
        info.supportRange = true;
        // 此时 Content-Length 是 1 (因为只请求了 0-0 共 1 byte)
        // 真实大小藏在 Content-Range 中: "bytes 0-0/1234567"
        auto contentRange = curl.getHeader("Content-Range");
        if (contentRange) {
            EDM_LOG_DEBUG(configure->id, "MetaInfoFetcher: Content-Range = {}", *contentRange);
            auto pos = contentRange->find('/');
            if (pos != std::string::npos) {
                try {
                    info.fileSize = std::stoll(contentRange->substr(pos + 1));
                } catch (...) {} // 解析失败
            }
        }
    } else if (responseCode == 200) {
        // 200 OK: 服务器忽略了 Range 头，直接返回整个文件，不支持断点续传
        info.supportRange = false;
        // 此时 Content-Length 就是真实文件大小
        curl_off_t size = -1;
        if (curl.getInfo(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size) && size >= 0) {
            info.fileSize = static_cast<FileSize>(size);
        }
        EDM_LOG_DEBUG(
            configure->id,
            "MetaInfoFetcher: server does not support Range, Content-Length={}",
            info.fileSize.value_or(-1)
        );
    }

    // 获取 Content-Type
    char* cType = nullptr;
    if (curl.getInfo(CURLINFO_CONTENT_TYPE, &cType) && cType) {
        info.contentType = cType;
    }

    info.etag               = curl.getHeader("ETag");
    info.lastModified       = curl.getHeader("Last-Modified");
    info.contentDisposition = curl.getHeader("Content-Disposition");

    // 兼容各家云存储的 MD5 哈希头
    info.md5 = curl.getHeader("Content-MD5");
    if (!info.md5) info.md5 = curl.getHeader("x-cos-meta-md5"); // 腾讯云 COS 特殊头

    EDM_LOG_DEBUG(
        configure->id,
        "MetaInfoFetcher: fileSize={}, supportRange={}, contentType={}, md5={}",
        info.fileSize.value_or(-1),
        info.supportRange,
        info.contentType.value_or("n/a"),
        info.md5.value_or("n/a")
    );

    return info;
}

} // namespace edm::downloader
