#include "TaskMetaInfoFetcher.h"

#include "TaskConfigure.h"
#include "utils/Utils.h"

#include <array>
#include <format>
#include <qdebug.h>
#include <qlogging.h>

namespace edm ::downloader {

TaskMetaInfoFetcher::TaskMetaInfoFetcher(TaskConfigure& configure) : configure_(configure) {}


FileSize TaskMetaInfoFetcher::getFileSize() const {
    auto scurl = configure_.newCurl();
    if (!scurl) {
        qDebug() << "TaskConfigure::newCurl is nullptr";
        return -1;
    }
    curl_easy_setopt(scurl.curl_, CURLOPT_NOBODY, 1L); // 不获取响应体
    curl_easy_setopt(scurl.curl_, CURLOPT_HEADER, 1L); // 获取响应头

    CURLcode code = curl_easy_perform(scurl.curl_);
    qDebug() << "curl_easy_perform return code: " << static_cast<int>(code);
    if (code != CURLE_OK) {
        return -1;
    }

    curl_off_t size = -1;
    if (curl_easy_getinfo(scurl.curl_, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size) == CURLE_OK && size >= 0) {
        return static_cast<long long>(size);
    }
    return -1;
}

std::string TaskMetaInfoFetcher::getFileSizeString() const {
    auto size = getFileSize(); // byte
    return utils::FileSize2String(size);
}

bool TaskMetaInfoFetcher::isSupportRange() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return false;

    // 设置 Range 测试：请求前 1 字节
    curl_easy_setopt(scurl.get(), CURLOPT_RANGE, "0-0"); // Range: bytes=0-0
    curl_easy_setopt(scurl.get(), CURLOPT_HEADER, 1L);   // 获取响应头
    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 0L);   // GET 请求，必须返回内容

    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return false;

    long responseCode = 0;
    curl_easy_getinfo(scurl.get(), CURLINFO_RESPONSE_CODE, &responseCode);

    // 206 Partial Content → 支持 Range
    return responseCode == 206;
}

std::string TaskMetaInfoFetcher::getContentType() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return {};

    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 1L); // HEAD 请求
    curl_easy_setopt(scurl.get(), CURLOPT_HEADER, 1L);

    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return {};

    char* contentType = nullptr;
    curl_easy_getinfo(scurl.get(), CURLINFO_CONTENT_TYPE, &contentType);
    return contentType ? std::string(contentType) : std::string{};
}

struct HeaderCollector {
    std::string etag;
    std::string lastModified;
};
std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}
static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t      total = size * nitems;
    std::string header(buffer, total);

    HeaderCollector* collector   = reinterpret_cast<HeaderCollector*>(userdata);
    std::string      lowerHeader = toLower(header);

    // 去掉前导空格
    lowerHeader.erase(0, lowerHeader.find_first_not_of(" \t\r\n"));
    header.erase(0, header.find_first_not_of(" \t\r\n"));

    qDebug() << "RawHeader: " << header;

    if (lowerHeader.rfind("etag:", 0) == 0) {
        collector->etag = header.substr(5);
        collector->etag.erase(0, collector->etag.find_first_not_of(" \t\r\n"));
        collector->etag.erase(collector->etag.find_last_not_of(" \t\r\n") + 1);
    } else if (lowerHeader.rfind("x-cos-meta-md5", 0) == 0) {
        collector->etag = header.substr(16); // fuck tencent cos non-standard behavior
        collector->etag.erase(0, collector->etag.find_first_not_of(" \t\r\n"));
        collector->etag.erase(collector->etag.find_last_not_of(" \t\r\n") + 1);
    } else if (lowerHeader.rfind("last-modified:", 0) == 0) {
        collector->lastModified = header.substr(14);
        collector->lastModified.erase(0, collector->lastModified.find_first_not_of(" \t\r\n"));
        collector->lastModified.erase(collector->lastModified.find_last_not_of(" \t\r\n") + 1);
    }
    return total;
}
std::string TaskMetaInfoFetcher::getETag() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return {};

    HeaderCollector collector;

    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 1L); // HEAD 请求
    curl_easy_setopt(scurl.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(scurl.get(), CURLOPT_HEADERDATA, &collector);

    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return {};

    return collector.etag;
}

std::string TaskMetaInfoFetcher::getLastModified() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return {};

    HeaderCollector collector;

    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 1L); // HEAD 请求
    curl_easy_setopt(scurl.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(scurl.get(), CURLOPT_HEADERDATA, &collector);

    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return {};

    return collector.lastModified;
}

std::string TaskMetaInfoFetcher::getFinalURL() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return {};

    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 1L);
    curl_easy_setopt(scurl.get(), CURLOPT_HEADER, 0L);
    curl_easy_setopt(scurl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return {};

    char* finalUrl = nullptr;
    curl_easy_getinfo(scurl.get(), CURLINFO_EFFECTIVE_URL, &finalUrl);
    return finalUrl ? std::string(finalUrl) : std::string{};
}

std::pair<long long, long long> TaskMetaInfoFetcher::getSupportedRange() const {
    auto scurl = configure_.newCurl();
    if (!scurl) return {-1, -1};

    long long fileSize = -1;
    {
        // HEAD 请求获取文件大小
        curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 1L);
        curl_easy_setopt(scurl.get(), CURLOPT_HEADER, 1L);
        CURLcode code = curl_easy_perform(scurl.get());
        if (code != CURLE_OK) return {-1, -1};
        curl_easy_getinfo(scurl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &fileSize);
    }

    // 测试是否支持 Range
    long responseCode = 0;
    curl_easy_setopt(scurl.get(), CURLOPT_NOBODY, 0L);
    curl_easy_setopt(scurl.get(), CURLOPT_RANGE, "0-0");
    curl_easy_setopt(scurl.get(), CURLOPT_HEADER, 1L);
    CURLcode code = curl_easy_perform(scurl.get());
    if (code != CURLE_OK) return {-1, -1};
    curl_easy_getinfo(scurl.get(), CURLINFO_RESPONSE_CODE, &responseCode);

    if (responseCode == 206) { // Partial Content
        return {0, fileSize - 1};
    } else if (responseCode == 200) { // 不支持 Range
        return {0, 0};
    }
    return {-1, -1};
}

bool TaskMetaInfoFetcher::canResumeDownload() const {
    auto [start, end] = getSupportedRange();
    return end > start;
}


} // namespace edm::downloader