#include "MetaInfoFetcher.h"
#include "CurlEx.h"
#include "FetchedMetaInfo.h"
#include "TaskConfigure.h"
#include "event/EventBus.h"
#include "event/Events.h"
#include "utils/Utils.h"

#include <QRunnable>
#include <QThreadPool>
#include <format>
#include <qdebug.h>
#include <qlogging.h>

namespace edm ::downloader {

class FetchRunnable : public QRunnable {
    TaskConfigure config_;

public:
    explicit FetchRunnable(TaskConfigure const& configure) : config_(configure) {}

    void run() override {
        MetaInfoResultEvent result;

        auto expected = MetaInfoFetcher{config_}.fetchAll();
        if (expected) {
            result.result = std::move(expected.value());
        } else {
            result.result = expected.error().message();
        }

        // 异步请求完成，通过 Qt 信号槽机制安全扔回主线程
        emit EventBus::instance() -> onTaskMetaInfoFetched(result);
    }
};

MetaInfoFetcher::MetaInfoFetcher(TaskConfigure& configure) : configure_(configure) {}

void MetaInfoFetcher::fetchAsync(TaskConfigure configure) {
    QThreadPool::globalInstance()->start(new FetchRunnable(std::move(configure)));
}

Expected<FetchedMetaInfo> MetaInfoFetcher::fetchAll() const {
    auto ex_res = configure_.newCurl();
    if (!ex_res) return forwardError(ex_res.error());

    CurlEx curl = std::move(ex_res.value());

    curl.setOpt(CURLOPT_NOBODY, 1L);
    curl.setOpt(CURLOPT_HEADER, 1L);
    curl.setOpt(CURLOPT_RANGE, "0-0");

    if (auto res = curl.perform(); !res) {
        return forwardError(res.error());
    }

    FetchedMetaInfo info;

    // 获取最终重定向的 URL
    char* fUrl = nullptr;
    if (curl.getInfo(CURLINFO_EFFECTIVE_URL, &fUrl) && fUrl) {
        info.finalUrl = fUrl;
    } else {
        info.finalUrl = configure_.url_;
    }

    // 是否支持 Range 及真实大小
    long responseCode = 0;
    curl.getInfo(CURLINFO_RESPONSE_CODE, &responseCode);

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