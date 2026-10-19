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

    /**
     * 从活动集合中摘除任务（best-effort 取消，会 join worker 并删除临时文件）。
     * 供"删除任务""重新下载"使用：摘除后同 id 可以立即重新入队。
     * 注意：会阻塞，须在非 GUI 线程调用。
     */
    void removeTask(TaskId id);

    [[nodiscard]] Expected<> createTask(std::shared_ptr<TaskContext> ctx);
};

} // namespace edm
