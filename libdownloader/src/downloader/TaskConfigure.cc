#include "TaskConfigure.h"

#include "CurlEx.h"
#include "model/TaskModel.h"

#include <regex>
#include <utility>

namespace edm {

namespace {
std::optional<std::string> originFromUrl(std::string const& url) {
    static const std::regex re(R"re(^([a-zA-Z][a-zA-Z0-9+.-]*://[^/]+))re");
    std::smatch             match;
    if (std::regex_search(url, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return std::nullopt;
}
} // namespace


TaskConfigure::TaskConfigure(std::shared_ptr<edm::TaskModel> model) noexcept {
    url_         = model->url;
    saveDir_     = model->saveDir.empty() ? "." : model->saveDir;
    threadCount_ = model->threadCount > 0 ? model->threadCount : static_cast<int>(GlobalDefaults::kDefaultThreadCount);
    bandLimit_   = model->bandLimit >= 0 ? model->bandLimit : GlobalDefaults::kDefaultBandwidthLimit;
    retryCount_  = model->retryCount >= 0 ? model->retryCount : kRetryCount;
    userAgent_   = model->userAgent.empty() ? GlobalDefaults::kDefaultUserAgent : model->userAgent;
    origin_      = originFromUrl(model->url);
    if (!model->pageUrl.empty()) referer_ = model->pageUrl;
    // cookie_ = ; // TODO: fix
    mimeType_ = model->mimeType;
}

Expected<downloader::CurlEx> TaskConfigure::newCurl() const {
    auto curl = downloader::CurlEx{};

    curl.setOpt(CURLOPT_URL, url_.c_str())  // 设置地址
        .setOpt(CURLOPT_FOLLOWLOCATION, 1L) // 跟随重定向
        .setOpt(CURLOPT_NOSIGNAL, 1L)
        .setOpt(CURLOPT_FAILONERROR, 1L)
        .setOpt(CURLOPT_NOPROXY, "localhost,127.0.0.1,::1"); // 本地回环地址忽略代理

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
        curl.setOpt(CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);        // 设置下载速度限制
    }

    if (!curl.status()) return makeStringError(curl.status().error().message());
    return curl;
}

std::shared_ptr<TaskConfigure>
TaskConfigure::fromUrl(std::string const& url, std::string const& saveDir, TaskConfigureDefaults defaults) {
    auto configure      = std::make_shared<TaskConfigure>();
    configure->url_     = url;
    configure->saveDir_ = saveDir;
    configure->threadCount_ =
        defaults.threadCount > 0 ? defaults.threadCount : static_cast<int>(GlobalDefaults::kDefaultThreadCount);
    configure->bandLimit_  = defaults.bandLimit >= 0 ? defaults.bandLimit : GlobalDefaults::kDefaultBandwidthLimit;
    configure->retryCount_ = defaults.retryCount >= 0 ? defaults.retryCount : kRetryCount;
    configure->userAgent_  = std::move(defaults.userAgent);
    configure->origin_     = originFromUrl(url);
    configure->proxyUrl_   = std::move(defaults.proxyUrl);

    return configure;
}

} // namespace edm
