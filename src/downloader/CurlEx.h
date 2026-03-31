#pragma once
#include "expected.h"

#include "curl/curl.h"

#include <optional>

namespace edm::downloader {

struct CurlCodeError : IErrorInfo {
    CURLcode code;

    CurlCodeError(CURLcode code) : code(code) {}
    std::string message() const noexcept override;
};

class CurlEx {
    CURL* curl_{nullptr};

    curl_slist*              headers_;
    std::vector<std::string> rawHeaders_; // 持久化 const char*

    mutable Expected<> status_{};

    void cleanup();

public:
    CurlEx();
    ~CurlEx();

    CurlEx(CurlEx&& o) noexcept;
    CurlEx& operator=(CurlEx&& o) noexcept;

    CurlEx(const CurlEx&)            = delete;
    CurlEx& operator=(const CurlEx&) = delete;

    inline CURL* get() const noexcept { return curl_; }

    inline operator bool() const noexcept { return curl_ != nullptr; }

    inline Expected<> const& status() const { return status_; }

    /**
     * 设置选项
     * @see curl_easy_setopt
     */
    template <typename... Args>
    CurlEx& setOpt(CURLoption opt, Args&&... args) {
        if (!status_) return *this;
        if (auto code = curl_easy_setopt(curl_, opt, std::forward<Args>(args)...); code != CURLE_OK) {
            status_ = makeError<CurlCodeError>(code);
        }
        return *this;
    }

    /**
     * 追加请求头
     * @param header 请求头
     * @return 是否添加成功
     * @see curl_slist_append
     */
    CurlEx& append(std::string header);

    /**
     * 执行请求
     * @return 是否执行成功
     * @see curl_easy_perform
     */
    Expected<> perform();

    /**
     * 获取信息
     * @param info 信息类型
     * @see curl_easy_getinfo
     */
    template <typename... Args>
    Expected<> getInfo(CURLINFO info, Args&&... args) {
        if (!status_) return forwardError(status_.error());
        auto code = curl_easy_getinfo(curl_, info, std::forward<Args>(args)...);
        if (code != CURLE_OK) return makeError<CurlCodeError>(code);
        return {};
    }

    /**
     * 获取指定名称的响应头 (需要 libcurl 7.83.0+)
     * @param name 请求头名称 (例如 "ETag", "Content-Disposition")
     * @return 如果存在则返回对应的值，不存在则返回 nullopt
     */
    std::optional<std::string> getHeader(const std::string& name) const;
};

} // namespace edm::downloader
