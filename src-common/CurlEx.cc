#include "CurlEx.h"

namespace edm::curl {

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

std::optional<std::string> CurlEx::getHeader(std::string const& name) const {
    if (!status_) return std::nullopt;

    curl_header* hout = nullptr;
    if (curl_easy_header(curl_, name.c_str(), 0, CURLH_HEADER, -1, &hout) == CURLHE_OK) {
        if (hout && hout->value) {
            return std::string(hout->value);
        }
    }
    return std::nullopt;
}


} // namespace edm::curl
