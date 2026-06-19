#pragma once
#include "expected.h"

#include <string>

namespace edm {
struct TaskConfigure;
struct FetchedMetaInfo;
} // namespace edm

namespace edm ::downloader {


class MetaInfoFetcher final {
    std::shared_ptr<TaskConfigure> configure_;

public:
    MetaInfoFetcher(const MetaInfoFetcher&)            = delete;
    MetaInfoFetcher& operator=(const MetaInfoFetcher&) = delete;
    MetaInfoFetcher(MetaInfoFetcher&&)                 = delete;
    MetaInfoFetcher& operator=(MetaInfoFetcher&&)      = delete;
    explicit MetaInfoFetcher(std::shared_ptr<TaskConfigure> configure);
    ~MetaInfoFetcher() = default;

    Expected<FetchedMetaInfo> fetchAll() const;
};

} // namespace edm::downloader
