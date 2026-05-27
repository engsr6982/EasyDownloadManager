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

bool Dispatcher::pauseTask(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return activeTasks_.at(id)->pause();
    }
    return false;
}

bool Dispatcher::resumeTask(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return activeTasks_.at(id)->resume();
    }
    return false;
}

bool Dispatcher::cancelTask(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return activeTasks_.at(id)->cancel();
    }
    return false;
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

    // 实例化真正的下载任务大脑
    auto downloadTask =
        std::make_shared<downloader::DownloadTask>(task, [](std::shared_ptr<edm::TaskContext> const& ctx) {
            if (ctx && ctx->model) {
                EdmApplication::getInstance().getDatabase()->insertTask(ctx->model);
            }
        });

    // 启动
    if (downloadTask->start()) {
        activeTasks_[task->model->id] = downloadTask;
        qDebug() << "Task ID" << task->model->id << "started successfully.";
    } else {
        qDebug() << "Failed to start Task ID" << task->model->id;
    }
}


} // namespace edm
