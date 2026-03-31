#include "CurlEx.h"

#include "fmt/format.h"

namespace edm::downloader {

std::string CurlCodeError::message() const noexcept {
    return fmt::format("Curl error {}: {}", static_cast<int>(code), curl_easy_strerror(code));
}


void CurlEx::cleanup() {
    if (curl_) curl_easy_cleanup(curl_);
    if (headers_) curl_slist_free_all(headers_);
    curl_    = nullptr;
    headers_ = nullptr;
}
CurlEx::CurlEx() : curl_(curl_easy_init()) {}
CurlEx::~CurlEx() { cleanup(); }

CurlEx::CurlEx(CurlEx&& o) noexcept : curl_(o.curl_) { o.curl_ = nullptr; }

CurlEx& CurlEx::operator=(CurlEx&& o) noexcept {
    if (this != &o) {
        cleanup();
        curl_      = o.curl_;
        headers_   = o.headers_;
        o.curl_    = nullptr;
        o.headers_ = nullptr;
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

Expected<> CurlEx::perform() {
    if (!status_) return forwardError(status_.error());
    if (auto code = curl_easy_perform(curl_); code != CURLE_OK) {
        return makeError<CurlCodeError>(code);
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


} // namespace edm::downloader