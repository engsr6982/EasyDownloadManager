#include "TaskConfigure.h"

#include "CurlEx.h"
#include "EdmConfig.h"

namespace edm::downloader {


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
    proxyUrl_ = EdmConfig::getInstance().getProxyConfig().toProxyUrl();
}

Expected<CurlEx> TaskConfigure::newCurl() const {
    auto curl = CurlEx{};

    curl.setOpt(CURLOPT_URL, url_.c_str())   // 设置地址
        .setOpt(CURLOPT_FOLLOWLOCATION, 1L); // 跟随重定向

    if (userAgent_) {
        curl.setOpt(CURLOPT_USERAGENT, userAgent_->c_str()); // 设置 User-Agent
    }

    if (cookie_) {
        curl.setOpt(CURLOPT_COOKIE, cookie_->c_str()); // 设置 Cookie
    }

    if (referer_) {
        curl.setOpt(CURLOPT_REFERER, referer_->c_str()); // 设置 Referer
    }

    if (origin_) {
        curl.append("Origin: " + *origin_);
    }

    if (requestType_ == RequestType::POST && postBody_) { // 设置请求类型
        curl.setOpt(CURLOPT_POST, 1L);
        curl.setOpt(CURLOPT_POSTFIELDS, postBody_->c_str());
    }

    if (proxyUrl_) {
        curl.setOpt(CURLOPT_PROXY, proxyUrl_->c_str()); // 设置代理
    }

    if (bandWidthLimit_ > 0) {
        auto speedLimit = static_cast<curl_off_t>(bandWidthLimit_) * 1024; // KB/s 转换为 B/s
        curl.setOpt(CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);             // 设置下载速度限制
    }

    if (!curl.status()) return makeStringError(curl.status().error().message());
    return curl;
}

TaskConfigure TaskConfigure::fromUrl(std::string const& url, std::string const& saveDir, bool useProxy) {
    TaskConfigure configure;
    configure.url_     = url;
    configure.saveDir_ = saveDir;

    auto& conf = EdmConfig::getInstance();

    configure.tempDir_        = conf.getTempDir().toStdString();
    configure.threadCount_    = conf.getThreadCount();
    configure.userAgent_      = conf.getUserAgent().toStdString();
    configure.bandWidthLimit_ = conf.getBandwidthLimit();
    if (auto proxy = conf.getProxyConfig(); useProxy && !proxy.isNone()) {
        configure.proxyUrl_ = proxy.toProxyUrl();
    }

    return configure;
}

} // namespace edm::downloader