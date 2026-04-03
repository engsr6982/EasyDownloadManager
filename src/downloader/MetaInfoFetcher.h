#pragma once
#include "expected.h"

#include <qtclasshelpermacros.h>
#include <string>

namespace edm {
struct TaskConfigure;
struct FetchedMetaInfo;
}

namespace edm ::downloader {


class MetaInfoFetcher final {
    std::shared_ptr<TaskConfigure> configure_;

public:
    Q_DISABLE_COPY_MOVE(MetaInfoFetcher);
    explicit MetaInfoFetcher(std::shared_ptr<TaskConfigure> configure);
    ~MetaInfoFetcher() = default;

    /**
     * 异步获取任务元信息
     * @param configure 任务配置
     * @note 任务完成后发出 `onTaskMetaInfoFetched` 信号
     */
    static void fetchAsync(std::shared_ptr<TaskConfigure> configure);


    Expected<FetchedMetaInfo> fetchAll() const;
};

} // namespace edm::downloader