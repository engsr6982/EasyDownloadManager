#include "Dispatcher.h"

#include "Global.h"
#include "downloader/DownloadTask.h"
#include "downloader/DownloadTypes.h"
#include "utils/TaskLogger.h"

#include <cassert>
#include <utility>

namespace edm {

struct Dispatcher::Impl {
    std::unordered_map<TaskId, std::shared_ptr<downloader::DownloadTask>> activeTasks_{};
    std::mutex                                                            tasksMutex_{};
    ITaskObserver*                                                        observer{nullptr};

    /// 终态任务不再占用调度槽位：可以被重新调度（继续/重新下载）。
    static bool isTerminal(TaskState state) {
        return state == TaskState::Finished || state == TaskState::Failed || state == TaskState::Canceled;
    }

    /// 回收所有终态任务。用原子状态判定，避免持 tasksMutex_ 时去抢任务锁。
    /// 被回收对象的析构统一放在锁外。
    void reapTerminalTasks() {
        std::vector<std::shared_ptr<downloader::DownloadTask>> doomed;
        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            for (auto it = activeTasks_.begin(); it != activeTasks_.end();) {
                if (isTerminal(it->second->getStateObject()->state())) {
                    doomed.push_back(it->second);
                    it = activeTasks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    /// 摘除并取消指定任务（可能阻塞，须在后台线程调用）。
    void removeTask(TaskId id) {
        std::shared_ptr<downloader::DownloadTask> task;
        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            auto                        it = activeTasks_.find(id);
            if (it == activeTasks_.end()) return;
            task = it->second;
            activeTasks_.erase(it);
        }
        // 锁外 best-effort 取消：会 join worker 并删除临时文件，可能耗时
        (void)task->cancel();
    }

    [[nodiscard]] Expected<> scheduleTask(std::shared_ptr<TaskContext> const& task) {
        auto taskId = task->id;
        EDM_LOG_DEBUG(taskId, "Dispatcher: scheduling task");

        std::shared_ptr<downloader::DownloadTask> replaced; // 锁外析构
        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            auto                        it = activeTasks_.find(taskId);
            if (it != activeTasks_.end()) {
                if (!isTerminal(it->second->getStateObject()->state())) {
                    EDM_LOG_DEBUG(taskId, "Dispatcher: task already active, skipping");
                    return {}; // 已经在跑了
                }
                EDM_LOG_DEBUG(taskId, "Dispatcher: replacing terminal task");
                replaced = it->second;
                activeTasks_.erase(it);
            }
        }

        auto downloadTask = std::make_shared<downloader::DownloadTask>(task, observer);
        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            if (activeTasks_.find(taskId) != activeTasks_.end()) {
                return {};
            }
            activeTasks_[taskId] = downloadTask;
        }

        if (auto started = downloadTask->start(); !started) {
            EDM_LOG_ERROR(taskId, "Dispatcher: failed to start task: {}", started.error().message());
            std::lock_guard<std::mutex> lock(tasksMutex_);
            auto                        it = activeTasks_.find(taskId);
            if (it != activeTasks_.end() && it->second == downloadTask) {
                activeTasks_.erase(it);
            }
            return forwardError(started.error());
        }
        EDM_LOG_INFO(taskId, "Dispatcher: task scheduled and started");
        return {};
    }
};

Dispatcher::Dispatcher(ITaskObserver* observer) : impl_(std::make_unique<Impl>()) {
    assert(observer != nullptr);
    impl_->observer = observer;
}
Dispatcher::~Dispatcher() = default;


std::shared_ptr<downloader::DownloadState> Dispatcher::getTaskState(TaskId id) {
    std::lock_guard<std::mutex> lock(impl_->tasksMutex_);
    auto                        it = impl_->activeTasks_.find(id);
    if (it == impl_->activeTasks_.end()) return nullptr;
    return it->second->getStateObject();
}


std::shared_ptr<TaskContext> Dispatcher::getActiveTaskContext(TaskId id) {
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

        auto it = impl_->activeTasks_.find(id);
        if (it != impl_->activeTasks_.end()) {
            return it->second->getContext();
        }
    }
    return nullptr;
}

void Dispatcher::updateAllSpeeds() {
    // 先在锁内快照，出锁再逐个更新：updateSpeed() 会短暂抢任务的 mutex_，
    // 不能把调用方（GUI 刷新定时器）堵在 tasksMutex_ 上。
    std::vector<std::shared_ptr<downloader::DownloadTask>> snapshot;
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);
        snapshot.reserve(impl_->activeTasks_.size());
        for (auto& [id, task] : impl_->activeTasks_) {
            snapshot.push_back(task);
        }
    }
    for (auto& task : snapshot) {
        task->updateSpeed();
    }

    // 顺手回收终态任务，使其可被重新调度（继续/重新下载）
    impl_->reapTerminalTasks();
}

void Dispatcher::updateSpeed(TaskId id) {
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);
        auto                        iter = impl_->activeTasks_.find(id);
        if (iter != impl_->activeTasks_.end()) {
            task = iter->second;
        }
    }
    if (task) task->updateSpeed();
}

void Dispatcher::removeTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: removeTask requested");
    impl_->removeTask(id);
}

Expected<> Dispatcher::pauseTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: pauseTask requested");
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

        auto iter = impl_->activeTasks_.find(id);
        if (iter != impl_->activeTasks_.end()) {
            task = iter->second;
        }
    }

    if (task) {
        // 不要在持有 tasksMutex_ 时调用任务方法：pause 会同步触发 observer 回调，
        // 回调可能再次进入 Dispatcher（如 getTaskState），同线程重复加锁会抛异常。
        return task->pause();
    }

    EDM_LOG_WARN(id, "Dispatcher: pauseTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::resumeTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: resumeTask requested");
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

        auto iter = impl_->activeTasks_.find(id);
        if (iter != impl_->activeTasks_.end()) {
            task = iter->second;
        }
    }

    if (task) {
        return task->resume();
    }

    EDM_LOG_WARN(id, "Dispatcher: resumeTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::cancelTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: cancelTask requested");
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

        auto iter = impl_->activeTasks_.find(id);
        if (iter != impl_->activeTasks_.end()) {
            task = iter->second;
        }
    }

    if (task) {
        // 与 pauseTask 同理：解锁后再调用任务方法，避免同步回调重入 Dispatcher。
        return task->cancel();
    }

    EDM_LOG_WARN(id, "Dispatcher: cancelTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::createTask(std::shared_ptr<TaskContext> ctx) {
    auto const id = ctx->id;
    assert(id != kInvalidTaskID);

    if (auto scheduled = impl_->scheduleTask(std::move(ctx)); !scheduled) {
        return forwardError(scheduled.error());
    }
    EDM_LOG_INFO(id, "Dispatcher: task created successfully");
    return {};
}


} // namespace edm
