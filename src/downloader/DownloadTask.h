#pragma once
#include "MultiThreadDownloader.h"
#include "TaskConfigure.h"

#include <memory>
#include <optional>
#include <qtclasshelpermacros.h>
#include <shared_mutex>
#include <string>


namespace edm ::downloader {

class TaskMetaInfoFetcher;

class DownloadTask final {
    TaskState                  state_{TaskState::Pending};
    TaskConfigure              configure_;
    MultiThreadDownloader      downloader_;
    mutable std::shared_mutex  mutex_; // 读写锁
    std::optional<std::string> lastError_{std::nullopt};


public:
    Q_DISABLE_COPY_MOVE(DownloadTask);
    explicit DownloadTask(TaskConfigure configure);
    ~DownloadTask();

    /**
     * 启动任务
     */
    [[nodiscard]] bool start();

    /**
     * 暂停任务
     * @note 暂停任务，并抛弃当前内存中未完成的残缺分片(磁盘中的分片不受影响)
     */
    [[nodiscard]] bool pause();

    /**
     * 恢复任务
     * @note 恢复中断的任务(即任务被暂停过)，将会从上次暂停的位置继续下载
     */
    [[nodiscard]] bool resume();

    /**
     * 取消任务
     * @note 取消任务后，将会清理分段文件，无法恢复(仅能重新启动)
     */
    [[nodiscard]] bool cancel();

    /**
     * 判断任务是否完成
     * @return 任务是否完成
     */
    [[nodiscard]] bool isFinished() const;

    /**
     * 判断任务是否暂停
     * @return 任务是否暂停
     */
    [[nodiscard]] bool isPaused() const;

    /**
     * 判断任务是否取消
     * @return 任务是否取消
     */
    [[nodiscard]] bool isCanceled() const;

    /**
     * 判断任务是否正在运行
     * @return 任务是否正在运行
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * 获取任务进度
     * @return 任务进度，范围[0, 1]
     */
    [[nodiscard]] double getProgress();

    /**
     * 获取最新的异常信息
     */
    [[nodiscard]] std::optional<std::string> const& getLastError();

    /**
     * 获取任务状态
     */
    [[nodiscard]] TaskState getState() const;
};

} // namespace edm::downloader
