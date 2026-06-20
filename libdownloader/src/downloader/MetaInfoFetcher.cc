#include "MetaInfoFetcher.h"
#include "CurlEx.h"
#include "FetchedMetaInfo.h"
#include "TaskConfigure.h"

namespace edm ::downloader {

namespace {
size_t discardCallback(char*, size_t size, size_t nmemb, void*) { return size * nmemb; }
} // namespace


MetaInfoFetcher::MetaInfoFetcher(std::shared_ptr<TaskConfigure> configure) : configure_(std::move(configure)) {}


Expected<FetchedMetaInfo> MetaInfoFetcher::fetchAll() const {
    auto ex_res = configure_->newCurl();
    if (!ex_res) return forwardError(ex_res.error());

    CurlEx curl = std::move(ex_res.value());

    // 显式将当前探测请求的 FAILONERROR 关闭（设为 0L）。
    // 因为 Meta 探测阶段的职责是“观察者”，无论服务器返回 200、206 还是 403，
    // 都是极其重要的业务信号，必须放行 perform()，让状态码能安全流转到后方的 getInfo 中。
    curl.setOpt(CURLOPT_FAILONERROR, 0L);

    curl.setOpt(CURLOPT_HEADER, 1L);
    curl.setOpt(CURLOPT_RANGE, "0-0");
    curl.setOpt(CURLOPT_WRITEFUNCTION, discardCallback);

    if (auto res = curl.perform(); !res) {
        return forwardError(res.error());
    }

    FetchedMetaInfo info;

    // 获取最终重定向的 URL
    char* fUrl = nullptr;
    if (curl.getInfo(CURLINFO_EFFECTIVE_URL, &fUrl) && fUrl) {
        info.finalUrl = fUrl;
    } else {
        info.finalUrl = configure_->url_;
    }

    // 是否支持 Range 及真实大小
    long responseCode = 0;
    if (auto responseCodeRes = curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode); !responseCodeRes) {
        return forwardError(responseCodeRes.error());
    }

    if (responseCode == 206) {
        // 206 Partial Content: 支持断点续传
        info.supportRange = true;
        // 此时 Content-Length 是 1 (因为只请求了 0-0 共 1 byte)
        // 真实大小藏在 Content-Range 中: "bytes 0-0/1234567"
        auto contentRange = curl.getHeader("Content-Range");
        if (contentRange) {
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

    return info;
}

} // namespace edm::downloader
