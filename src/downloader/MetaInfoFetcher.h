#pragma once
#include "expected.h"

#include <qtclasshelpermacros.h>
#include <string>

namespace edm ::downloader {

struct TaskConfigure;
struct FetchedMetaInfo;

class MetaInfoFetcher final {
    TaskConfigure& configure_;

public:
    Q_DISABLE_COPY_MOVE(MetaInfoFetcher);
    explicit MetaInfoFetcher(TaskConfigure& configure);
    ~MetaInfoFetcher() = default;

    /**
     * 异步获取任务元信息
     * @param configure 任务配置
     * @note 任务完成后发出 `onTaskMetaInfoFetched` 信号
     */
    static void fetchAsync(TaskConfigure configure);


    Expected<FetchedMetaInfo> fetchAll() const;
};

} // namespace edm::downloader