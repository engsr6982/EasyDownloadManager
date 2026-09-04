#pragma once
#include "Global.h"
#include "expected.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>


namespace edm {

struct TaskContext;
struct ITaskObserver;

namespace downloader {
class DownloadState;
class DownloadTask;
} // namespace downloader


class Dispatcher {
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;


public:
    /**
     * @param observer observer 实例生命周期必须大于 dispatcher
     */
    explicit Dispatcher(ITaskObserver* observer);
    ~Dispatcher();

    /**
     * 获取指定ID的下载任务状态
     * @param id 任务ID
     * @return 返回共享指针指向的下载状态对象，如果任务不存在则返回nullptr
     */
    std::shared_ptr<downloader::DownloadState> getTaskState(TaskId id);

    /**
     * 获取指定ID的活动任务上下文
     * @param id 任务ID
     * @return 返回共享指针指向的任务上下文对象，如果任务不存在则返回nullptr
     */
    std::shared_ptr<TaskContext> getActiveTaskContext(TaskId id);

    void updateAllSpeeds();

    void updateSpeed(TaskId id);

    Expected<> pauseTask(TaskId id);
    Expected<> resumeTask(TaskId id);
    Expected<> cancelTask(TaskId id);

    [[nodiscard]] Expected<> createTask(std::shared_ptr<TaskContext> ctx);
};

} // namespace edm
