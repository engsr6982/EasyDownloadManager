#include "TaskConfigure.h"

#include "CurlEx.h"
#include "EdmConfig.h"
#include "model/TaskModel.h"

namespace edm {


TaskConfigure::TaskConfigure(std::shared_ptr<edm::TaskModel> model) noexcept {
    url_            = model->url;
    saveDir_        = model->saveDir;
    threadCount_    = model->threadCount;
    bandLimit_ = model->bandLimit;
    userAgent_      = model->userAgent;
    // origin_ = ; // TODO: impl
    // referer_ = ;
    // cookie_ = ;
    mimeType_ = model->mimeType;
    proxyUrl_ = EdmConfig::getInstance().getProxyConfig().toProxyUrl();
}

Expected<downloader::CurlEx> TaskConfigure::newCurl() const {
    auto curl = downloader::CurlEx{};

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

    if (proxyUrl_) {
        curl.setOpt(CURLOPT_PROXY, proxyUrl_->c_str()); // 设置代理
    }

    if (bandLimit_ > 0) {
        auto speedLimit = static_cast<curl_off_t>(bandLimit_) * 1024; // KB/s 转换为 B/s
        curl.setOpt(CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);             // 设置下载速度限制
    }

    if (!curl.status()) return makeStringError(curl.status().error().message());
    return curl;
}

std::shared_ptr<TaskConfigure>
TaskConfigure::fromUrl(std::string const& url, std::string const& saveDir, bool useProxy) {
    auto configure      = std::make_shared<TaskConfigure>();
    configure->url_     = url;
    configure->saveDir_ = saveDir;

    auto& conf = EdmConfig::getInstance();

    configure->threadCount_    = conf.getThreadCount();
    configure->userAgent_      = conf.getUserAgent().toStdString();
    configure->bandLimit_ = conf.getBandwidthLimit();
    if (auto proxy = conf.getProxyConfig(); useProxy && !proxy.isNone()) {
        configure->proxyUrl_ = proxy.toProxyUrl();
    }

    return configure;
}

} // namespace edm