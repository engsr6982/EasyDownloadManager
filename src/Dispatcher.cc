#include "Dispatcher.h"

#include "EdmApplication.h"
#include "database/DownloadDatabase.h"
#include "downloader/DownloadTask.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"
#include "event/EventBus.h"


#include <QDebug>
#include <QRunnable>
#include <qobject.h>

namespace edm {


Dispatcher::Dispatcher() {
    // 监听新增任务的调度请求
    connect(EventBus::instance(), &EventBus::onTaskCreated, this, &Dispatcher::handleTaskCreated);
}

Dispatcher::~Dispatcher() = default;

namespace {

std::shared_ptr<TaskContext> makeContextFromModel(std::shared_ptr<TaskModel> model) {
    if (!model) return nullptr;

    auto ctx       = std::make_shared<TaskContext>();
    ctx->model     = std::move(model);
    ctx->configure = std::make_shared<TaskConfigure>(ctx->model);
    return ctx;
}

} // namespace


std::shared_ptr<downloader::DownloadState> Dispatcher::getTaskState(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto                        it = activeTasks_.find(id);
    if (it == activeTasks_.end()) return nullptr;
    return it->second->getStateObject();
}
std::shared_ptr<TaskModel> Dispatcher::getTaskModel(TaskId id) {
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);

        auto it = activeTasks_.find(id);
        if (it != activeTasks_.end()) {
            return it->second->getTaskContext()->model;
        }
    }
    return EdmApplication::getInstance().getDatabase()->getTaskById(id);
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
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->pause();
    }
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::resumeTask(TaskId id) {
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

    auto model = EdmApplication::getInstance().getDatabase()->getTaskById(id);
    if (!model) return makeStringError("Task does not exist");
    if (model->state != TaskState::Paused && model->state != TaskState::Failed && model->state != TaskState::Pending) {
        return makeStringError("Task is not resumable from current state");
    }

    auto ctx = makeContextFromModel(model);
    auto resumedTask =
        std::make_shared<downloader::DownloadTask>(ctx, [](std::shared_ptr<edm::TaskContext> const& changedCtx) {
            if (changedCtx && changedCtx->model) {
                EdmApplication::getInstance().getDatabase()->insertTask(changedCtx->model);
            }
        });

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_[id] = resumedTask;
    }

    auto started = resumedTask->resume();
    if (!started) {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_.erase(id);
    }
    return started;
}

Expected<> Dispatcher::cancelTask(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->cancel();
    }
    return makeStringError("Task is not active");
}

void Dispatcher::handleTaskCreated(std::shared_ptr<edm::TaskContext> task) {
    qDebug() << "Dispatcher received task ID:" << task->model->id;

    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.find(task->model->id) != activeTasks_.end()) {
        return; // 已经在跑了
    }

    // 从 Model 转换为 Configure
    if (!task->configure) {
        task->configure = std::make_shared<TaskConfigure>(task->model);
    }

    auto downloadTask =
        std::make_shared<downloader::DownloadTask>(task, [](std::shared_ptr<edm::TaskContext> const& ctx) {
            if (ctx && ctx->model) {
                EdmApplication::getInstance().getDatabase()->insertTask(ctx->model);
            }
        });

    activeTasks_[task->model->id] = downloadTask;
    if (auto started = downloadTask->start(); started) {
        qDebug() << "Task ID" << task->model->id << "started successfully.";
    } else {
        activeTasks_.erase(task->model->id);
        qDebug() << "Failed to start Task ID" << task->model->id << started.error().message().c_str();
    }
}


} // namespace edm
