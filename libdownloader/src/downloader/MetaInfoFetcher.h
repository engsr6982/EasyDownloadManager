#pragma once
#include "downloader/DownloadTypes.h"
#include "expected.h"
#include <memory>


namespace edm {
struct TaskContext;
} // namespace edm

namespace edm ::downloader {

class MetaInfoFetcher final {
public:
    MetaInfoFetcher() = delete;

    static Expected<FetchedMetaInfo> fetchAll(std::shared_ptr<TaskContext> const& configure);
};

} // namespace edm::downloader
