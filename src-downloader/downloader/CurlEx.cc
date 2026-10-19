#include "CurlEx.h"

#include "DownloadTypes.h"
#include "Global.h"

#include "fmt/format.h"

namespace edm::downloader {

namespace {
// 传输阻塞上界：没有这两个限制时，服务器一旦卡住不回数据，写回调就不会被调用，
// 暂停/取消的 joinWorkers() 将无限期挂住调用线程。参见 DownloadTask::pause()。
constexpr long kConnectTimeoutSec = 15;  // 建连超时
constexpr long kLowSpeedLimit     = 1;   // B/s，低于此速率…
constexpr long kLowSpeedTimeSec   = 30;  // …持续该秒数即中断
} // namespace


void CurlEx::cleanup() {
    if (curl_) curl_easy_cleanup(curl_);
    if (headers_) curl_slist_free_all(headers_);
    curl_    = nullptr;
    headers_ = nullptr;
}
CurlEx::CurlEx() : curl_(curl_easy_init()) {}
CurlEx::~CurlEx() { cleanup(); }

CurlEx::CurlEx(CurlEx&& o) noexcept
: curl_(o.curl_),
  headers_(o.headers_),
  rawHeaders_(std::move(o.rawHeaders_)),
  status_(std::move(o.status_)) {
    o.curl_    = nullptr;
    o.headers_ = nullptr;
}

CurlEx& CurlEx::operator=(CurlEx&& o) noexcept {
    if (this != &o) {
        cleanup();
        curl_       = o.curl_;
        headers_    = o.headers_;
        rawHeaders_ = std::move(o.rawHeaders_);
        status_     = std::move(o.status_);
        o.curl_     = nullptr;
        o.headers_  = nullptr;
    }
    return *this;
}

CurlEx& CurlEx::append(std::string header) {
    if (!status_) return *this;
    rawHeaders_.push_back(std::move(header));
    auto newList = curl_slist_append(headers_, rawHeaders_.back().c_str());
    if (!newList) {
        status_ = makeStringError("curl_slist_append failed");
    }
    if (!headers_) {
        setOpt(CURLOPT_HTTPHEADER, newList); // first call, set headers
    }
    headers_ = newList;
    return *this;
}

CurlEx& CurlEx::setAbortFlag(std::atomic<bool> const* flag) {
    // libcurl 进度回调：返回非 0 立即中止本次传输（连接阶段也会被调用）
    static auto const kXferInfo = [](void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
        auto const* abortFlag = static_cast<std::atomic<bool> const*>(clientp);
        return (abortFlag && !abortFlag->load(std::memory_order_relaxed)) ? 1 : 0;
    };

    setOpt(CURLOPT_NOPROGRESS, 0L);
    setOpt(CURLOPT_XFERINFOFUNCTION, +kXferInfo);
    setOpt(CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(flag));
    return *this;
}

Expected<> CurlEx::perform() {
    if (!status_) return forwardError(status_.error());
    if (auto code = curl_easy_perform(curl_); code != CURLE_OK) {
        return makeCurlError(code);
    }
    return {};
}

std::optional<std::string> CurlEx::getHeader(const std::string& name) const {
    if (!status_) return std::nullopt;

    curl_header* hout = nullptr;
    if (curl_easy_header(curl_, name.c_str(), 0, CURLH_HEADER, -1, &hout) == CURLHE_OK) {
        if (hout && hout->value) {
            return std::string(hout->value);
        }
    }
    return std::nullopt;
}

Expected<downloader::CurlEx> CurlEx::fromContext(std::shared_ptr<TaskContext> const& cfg) {
    auto curl = downloader::CurlEx{};

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
        curl.setOpt(CURLOPT_COOKIE, cfg->cookie->c_str()); // 设置 Cookie
    }

    if (cfg->referer) {
        curl.setOpt(CURLOPT_REFERER, cfg->referer->c_str()); // 设置 Referer
    }

    if (cfg->origin) {
        curl.append("Origin: " + *cfg->origin);
    }

    if (cfg->proxyUrl) {
        curl.setOpt(CURLOPT_PROXY, cfg->proxyUrl->c_str()); // 设置代理
    }

    if (cfg->bandLimit > kInvalidBandLimit) {
        auto speedLimit = static_cast<curl_off_t>(cfg->bandLimit) * 1024; // KB/s 转换为 B/s
        curl.setOpt(CURLOPT_MAX_RECV_SPEED_LARGE, speedLimit);            // 设置下载速度限制
    }

    if (!cfg->headers.empty()) {
        for (auto const& header : cfg->headers) {
            curl.append(header);
        }
    }

    if (!curl.status()) return makeStringError(curl.status().error().message());
    return curl;
}

} // namespace edm::downloader
