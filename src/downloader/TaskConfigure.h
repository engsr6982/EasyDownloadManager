#pragma once
#include "model/TaskModel.h"


#include <optional>
#include <string>

#include <curl/curl.h>

namespace edm::downloader {

struct SCurl {
    CURL*       curl_{nullptr};
    curl_slist* headers_{nullptr};

    SCurl(CURL* c, curl_slist* h);
    ~SCurl();

    SCurl(SCurl&& o) noexcept;
    SCurl& operator=(SCurl&& o) noexcept;

    SCurl(const SCurl&)            = delete;
    SCurl& operator=(const SCurl&) = delete;

    inline CURL* get() const noexcept { return curl_; }

    inline explicit operator bool() const noexcept { return curl_ != nullptr; }

private:
    void cleanup();
};

struct TaskConfigure {
    std::string                url_;
    std::string                saveDir_;
    std::string                tempDir_;
    int                        threadCount_;
    BandWidthLimit             bandWidthLimit_{0}; // 单位：KB/s
    std::optional<std::string> userAgent_;
    std::optional<std::string> origin_;
    std::optional<std::string> referer_;
    std::optional<std::string> cookie_;
    std::optional<std::string> mimeType_;
    std::optional<std::string> proxyUrl_;

    RequestType                requestType_{RequestType::GET};
    std::optional<std::string> postBody_;

    [[nodiscard]] SCurl newCurl() const;
};

} // namespace edm::downloader