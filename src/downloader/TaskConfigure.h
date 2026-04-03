#pragma once
#include "Global.h"
#include "expected.h"

#include <optional>
#include <string>

namespace edm {

namespace downloader {
class CurlEx;
}
struct TaskModel;

struct TaskConfigure {
    std::string                url_;
    std::string                saveDir_;
    int                        threadCount_;
    BandWidthLimit             bandWidthLimit_{0}; // 单位：KB/s
    std::optional<std::string> userAgent_;
    std::optional<std::string> origin_;
    std::optional<std::string> referer_;
    std::optional<std::string> cookie_;
    std::optional<std::string> mimeType_;
    std::optional<std::string> proxyUrl_;

    TaskConfigure() = default;
    explicit TaskConfigure(std::shared_ptr<edm::TaskModel> model) noexcept;

    [[nodiscard]] Expected<downloader::CurlEx> newCurl() const;

    [[nodiscard]] static std::shared_ptr<TaskConfigure>
    fromUrl(std::string const& url, std::string const& saveDir, bool useProxy);
};

} // namespace edm