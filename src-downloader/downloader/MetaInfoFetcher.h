#pragma once
#include "downloader/DownloadTypes.h"
#include "expected.h"
#include <atomic>
#include <memory>


namespace edm {
struct TaskContext;
} // namespace edm

namespace edm ::downloader {

class MetaInfoFetcher final {
public:
    MetaInfoFetcher() = delete;

    /// 探测文件元信息（一次 GET + Range: 0-0）。
    /// @param abortFlag 任务的可中断标志（DownloadTask 的 isRunningFlag_）：置为 false 时
    ///                  立即中断探测，供暂停/取消使用。可为空。
    static Expected<FetchedMetaInfo> fetchAll(
        std::shared_ptr<TaskContext> const&       configure,
        std::shared_ptr<std::atomic<bool>> const& abortFlag = nullptr
    );
};

} // namespace edm::downloader
