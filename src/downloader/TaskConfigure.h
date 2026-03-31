#pragma once
#include "expected.h"
#include "model/TaskModel.h"

#include <optional>
#include <string>

namespace edm::downloader {

class CurlEx;

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

    TaskConfigure() = default;
    explicit TaskConfigure(TaskModel const& model) noexcept;

    [[nodiscard]] Expected<CurlEx> newCurl() const;

    [[nodiscard]] static TaskConfigure fromUrl(std::string const& url, std::string const& saveDir, bool useProxy);
};

} // namespace edm::downloader