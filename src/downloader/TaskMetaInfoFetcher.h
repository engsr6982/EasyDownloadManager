#pragma once
#include "model/Definition.h"

#include <qtclasshelpermacros.h>
#include <string>

namespace edm ::downloader {

struct TaskConfigure;

class TaskMetaInfoFetcher final {
    TaskConfigure& configure_;

public:
    Q_DISABLE_COPY_MOVE(TaskMetaInfoFetcher);
    explicit TaskMetaInfoFetcher(TaskConfigure& configure);
    ~TaskMetaInfoFetcher() = default;

    /**
     * 异步获取任务元信息
     * @param configure 任务配置
     * @note 任务完成后发出 `onTaskMetaInfoFetched` 信号
     */
    static void fetchAsync(TaskConfigure const& configure);

    /**
     * 获取文件大小
     * @return 文件大小，单位为字节(byte), -1 表示获取失败
     */
    [[nodiscard]] FileSize getFileSize() const;

    /**
     * 获取文件大小 (自适应单位)
     */
    [[nodiscard]] std::string getFileSizeString() const;

    /**
     * 是否支持 Range 请求
     */
    bool isSupportRange() const;

    /**
     * 获取 Content-Type
     */
    std::string getContentType() const;

    /**
     * 获取 ETag
     */
    std::string getETag() const;

    /**
     * 获取 Last-Modified
     */
    std::string getLastModified() const;

    /**
     * 获取最终请求的 URL
     */
    std::string getFinalURL() const;

    /**
     * 获取支持的 Range 范围
     */
    std::pair<long long, long long> getSupportedRange() const;

    /**
     * 是否支持断点续传
     */
    bool canResumeDownload() const;
};

} // namespace edm::downloader