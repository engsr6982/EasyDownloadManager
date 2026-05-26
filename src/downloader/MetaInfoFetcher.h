#pragma once
#include "expected.h"

#include <qtclasshelpermacros.h>
#include <string>

namespace edm {
struct TaskConfigure;
struct FetchedMetaInfo;
} // namespace edm

namespace edm ::downloader {


class MetaInfoFetcher final {
    std::shared_ptr<TaskConfigure> configure_;

public:
    Q_DISABLE_COPY_MOVE(MetaInfoFetcher);
    explicit MetaInfoFetcher(std::shared_ptr<TaskConfigure> configure);
    ~MetaInfoFetcher() = default;

    Expected<FetchedMetaInfo> fetchAll() const;
};

} // namespace edm::downloader