#pragma once
#include "expected.h"
#include "Global.h"

#include <string>

namespace edm {
struct TaskConfigure;
struct FetchedMetaInfo;
} // namespace edm

namespace edm ::downloader {


class MetaInfoFetcher final {
    std::shared_ptr<TaskConfigure> configure_;
    TaskId                         taskId_;

public:
    MetaInfoFetcher(const MetaInfoFetcher&)            = delete;
    MetaInfoFetcher& operator=(const MetaInfoFetcher&) = delete;
    MetaInfoFetcher(MetaInfoFetcher&&)                 = delete;
    MetaInfoFetcher& operator=(MetaInfoFetcher&&)      = delete;
    explicit MetaInfoFetcher(std::shared_ptr<TaskConfigure> configure, TaskId taskId = kInvalidTaskID);
    ~MetaInfoFetcher() = default;

    Expected<FetchedMetaInfo> fetchAll() const;
};

} // namespace edm::downloader
