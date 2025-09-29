#include "TaskConfigure.h"

#include "config/EdmGlobalConfig.h"

namespace edm::downloader {


SCurl::SCurl(CURL* c, curl_slist* h) : curl_(c), headers_(h) {}

SCurl::~SCurl() { cleanup(); }

SCurl::SCurl(SCurl&& o) noexcept : curl_(o.curl_), headers_(o.headers_) {
    o.curl_    = nullptr;
    o.headers_ = nullptr;
}

SCurl& SCurl::operator=(SCurl&& o) noexcept {
    if (this != &o) {
        cleanup();
        curl_      = o.curl_;
        headers_   = o.headers_;
        o.curl_    = nullptr;
        o.headers_ = nullptr;
    }
    return *this;
}

void SCurl::cleanup() {
    if (curl_) curl_easy_cleanup(curl_);
    if (headers_) curl_slist_free_all(headers_);
    curl_    = nullptr;
    headers_ = nullptr;
}


TaskConfigure::TaskConfigure(TaskModel const& model) noexcept {
    url_            = model.url;
    saveDir_        = model.saveDir;
    tempDir_        = model.tempDir;
    threadCount_    = model.threadCount;
    bandWidthLimit_ = model.bandWidthLimit;
    userAgent_      = model.userAgent;
    // origin_ = ; // TODO: impl
    // referer_ = ;
    // cookie_ = ;
    mimeType_ = model.mimeType;
    proxyUrl_ = proxy_utils::toProxyUrl(EdmGlobalConfig::instance().getProxyConfig());
}

SCurl TaskConfigure::newCurl() const {
    CURL*       curl    = curl_easy_init();
    curl_slist* headers = nullptr;
    if (!curl) {
        return {nullptr, nullptr};
    }

    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());  // 设置地址
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 跟随重定向

    if (userAgent_) curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_->c_str()); // 设置 User-Agent
    if (cookie_) curl_easy_setopt(curl, CURLOPT_COOKIE, cookie_->c_str());          // 设置 Cookie
    if (referer_) curl_easy_setopt(curl, CURLOPT_REFERER, referer_->c_str());       // 设置 Referer

    if (origin_) {
        std::string originHeader = "Origin: " + *origin_;
        headers                  = curl_slist_append(headers, originHeader.c_str());
    }

    if (requestType_ == RequestType::POST && postBody_) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody_->c_str());
    }

    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (proxyUrl_) curl_easy_setopt(curl, CURLOPT_PROXY, proxyUrl_->c_str()); // 设置代理

    if (bandWidthLimit_ > 0) {
        curl_off_t speedLimit = static_cast<curl_off_t>(bandWidthLimit_) * 1024; // KB/s 转换为 B/s
        curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);        // 设置下载速度限制
    }

    return SCurl(curl, headers);
}

} // namespace edm::downloader