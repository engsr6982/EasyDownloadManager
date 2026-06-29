#include "Dispatcher.h"

#include "Global.h"
#include "downloader/DownloadTask.h"
#include "downloader/DownloadTypes.h"
#include "utils/TaskLogger.h"

#include <cassert>
#include <utility>

namespace edm {


Dispatcher::Dispatcher(Options options)
: taskLoader_(std::move(options.taskLoader)),
  onTaskChanged_(std::move(options.onTaskChanged)) {
    EDM_LOG_INFO(kInvalidTaskID, "Dispatcher initialized");
}

Dispatcher::~Dispatcher() = default;

void Dispatcher::persistTask(std::shared_ptr<TaskConfigure> const& ctx) const {
    if (onTaskChanged_) {
        onTaskChanged_(ctx);
    }
}


std::shared_ptr<downloader::DownloadState> Dispatcher::getTaskState(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto                        it = activeTasks_.find(id);
    if (it == activeTasks_.end()) return nullptr;
    return it->second->getStateObject();
}
std::shared_ptr<TaskConfigure> Dispatcher::getTaskConfigure(TaskId id) {
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);

        auto it = activeTasks_.find(id);
        if (it != activeTasks_.end()) {
            return it->second->getConfigure();
        }
    }
    return taskLoader_ ? taskLoader_(id) : nullptr;
}

void Dispatcher::updateAllSpeeds() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    for (auto& [id, task] : activeTasks_) {
        task->updateSpeed();
    }
}

void Dispatcher::updateSpeed(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        iter->second->updateSpeed();
    }
}

Expected<> Dispatcher::pauseTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: pauseTask requested");
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->pause();
    }
    EDM_LOG_WARN(id, "Dispatcher: pauseTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::resumeTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: resumeTask requested");
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);

        auto iter = activeTasks_.find(id);
        if (iter != activeTasks_.end()) {
            task = iter->second;
        }
    }

    if (task) {
        return task->resume();
    }

    auto cfg = taskLoader_ ? taskLoader_(id) : nullptr;
    if (!cfg) {
        EDM_LOG_ERROR(id, "Dispatcher: resumeTask failed, task does not exist");
        return makeStringError("Task does not exist");
    }

    // if (cfg->state != TaskState::Paused && cfg->state != TaskState::Failed && cfg->state != TaskState::Pending) {
    //     EDM_LOG_WARN(
    //         id,
    //         "Dispatcher: resumeTask failed, task is not resumable from state {}",
    //         static_cast<int>(cfg->state)
    //     );
    //     return makeStringError("Task is not resumable from current state");
    // }

    auto resumedTask =
        std::make_shared<downloader::DownloadTask>(cfg, [this](std::shared_ptr<edm::TaskConfigure> const& changedCtx) {
            persistTask(changedCtx);
        });

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_[id] = resumedTask;
    }

    auto started = resumedTask->resume();
    if (!started) {
        EDM_LOG_ERROR(id, "Dispatcher: resumeTask start failed: {}", started.error().message());
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_.erase(id);
    }
    return started;
}

Expected<> Dispatcher::cancelTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: cancelTask requested");
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->cancel();
    }
    EDM_LOG_WARN(id, "Dispatcher: cancelTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::scheduleTask(std::shared_ptr<edm::TaskConfigure> task) {
    // if (!task || !task->model) {
    //     return makeStringError("Task context is missing");
    // }

    auto taskId = task->id;
    EDM_LOG_DEBUG(taskId, "Dispatcher: scheduling task");

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        if (activeTasks_.find(taskId) != activeTasks_.end()) {
            EDM_LOG_DEBUG(taskId, "Dispatcher: task already active, skipping");
            return {}; // 已经在跑了
        }
    }

    auto downloadTask =
        std::make_shared<downloader::DownloadTask>(task, [this](std::shared_ptr<edm::TaskConfigure> const& ctx) {
            persistTask(ctx);
        });

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

Expected<> Dispatcher::createTask(std::shared_ptr<TaskConfigure> configure) {
    assert(configure->id != kInvalidTaskID);

    persistTask(configure);

    if (auto scheduled = scheduleTask(configure); !scheduled) {
        return forwardError(scheduled.error());
    }
    EDM_LOG_INFO(configure->id, "Dispatcher: task created successfully");
    return {};
}


} // namespace edm
