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

    [[nodiscard]] Expected<> scheduleTask(std::shared_ptr<TaskContext> const& task) {
        auto taskId = task->id;
        EDM_LOG_DEBUG(taskId, "Dispatcher: scheduling task");

        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            if (activeTasks_.find(taskId) != activeTasks_.end()) {
                EDM_LOG_DEBUG(taskId, "Dispatcher: task already active, skipping");
                return {}; // 已经在跑了
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
    std::lock_guard<std::mutex> lock(impl_->tasksMutex_);
    for (auto& [id, task] : impl_->activeTasks_) {
        task->updateSpeed();
    }
}

void Dispatcher::updateSpeed(TaskId id) {
    std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

    auto iter = impl_->activeTasks_.find(id);
    if (iter != impl_->activeTasks_.end()) {
        iter->second->updateSpeed();
    }
}

Expected<> Dispatcher::pauseTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: pauseTask requested");
    std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

    auto iter = impl_->activeTasks_.find(id);
    if (iter != impl_->activeTasks_.end()) {
        return iter->second->pause();
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
    std::lock_guard<std::mutex> lock(impl_->tasksMutex_);

    auto iter = impl_->activeTasks_.find(id);
    if (iter != impl_->activeTasks_.end()) {
        return iter->second->cancel();
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
