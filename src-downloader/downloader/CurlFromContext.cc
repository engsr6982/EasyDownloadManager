#include "CurlFromContext.h"

#include "DownloadTypes.h"
#include "Global.h"

namespace edm::downloader {

namespace {
// 传输阻塞上界：没有这两个限制时，服务器一旦卡住不回数据，写回调就不会被调用，
// 暂停/取消的 joinWorkers() 将无限期挂住调用线程。参见 DownloadTask::pause()。
constexpr long kConnectTimeoutSec = 15; // 建连超时
constexpr long kLowSpeedLimit     = 1;  // B/s，速率下限
constexpr long kLowSpeedTimeSec   = 30; // 低于下限持续该秒数即中断
} // namespace

void applyContextOptions(curl::CurlEx& curl, std::shared_ptr<TaskContext> const& cfg) {
    curl.setOpt(CURLOPT_URL, cfg->url.c_str()) // 设置地址
        .setOpt(CURLOPT_FOLLOWLOCATION, 1L)    // 跟随重定向
        .setOpt(CURLOPT_NOSIGNAL, 1L)
        .setOpt(CURLOPT_FAILONERROR, 1L)
        .setOpt(CURLOPT_NOPROXY, "localhost,127.0.0.1,::1") // 本地回环地址忽略代理
        .setOpt(CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec)
        .setOpt(CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimit)
        .setOpt(CURLOPT_LOW_SPEED_TIME, kLowSpeedTimeSec);

    curl.setOpt(CURLOPT_USERAGENT, cfg->userAgent.c_str()); // 设置 User-Agent

    if (cfg->cookie) {
        curl.setOpt(CURLOPT_COOKIE, cfg->cookie->c_str());
    }

    if (cfg->referer) {
        curl.setOpt(CURLOPT_REFERER, cfg->referer->c_str());
    }

    if (cfg->origin) {
        curl.append("Origin: " + *cfg->origin);
    }

    if (cfg->proxyUrl) {
        curl.setOpt(CURLOPT_PROXY, cfg->proxyUrl->c_str());
    }

    if (cfg->bandLimit > kInvalidBandLimit) {
        auto speedLimit = static_cast<curl_off_t>(cfg->bandLimit) * 1024; // KB/s 转换为 B/s
        curl.setOpt(CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);
    }

    if (!cfg->headers.empty()) {
        for (auto const& header : cfg->headers) {
            curl.append(header);
        }
    }
}

Expected<curl::CurlEx> makeCurlFromContext(std::shared_ptr<TaskContext> const& cfg) {
    auto curl = curl::CurlEx{};
    applyContextOptions(curl, cfg);
    if (!curl.status()) return makeStringError(curl.status().error().message());
    return curl;
}

} // namespace edm::downloader
