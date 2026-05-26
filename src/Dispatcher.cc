#include "Dispatcher.h"

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


std::optional<Dispatcher::TaskSnapshot> Dispatcher::getTaskSnapshot(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto                        it = activeTasks_.find(id);
    if (it != activeTasks_.end()) {
        auto task = it->second;
        return TaskSnapshot{
            task->getTaskContext()->model,
            task->getState(),
            task->getDownloadedBytes(),
            task->getSpeed(),
            task->getRanges()
        };
    }
    return std::nullopt;
}
void Dispatcher::updateAllSpeeds() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    for (auto& [id, task] : activeTasks_) {
        task->updateSpeed();
    }
}

void Dispatcher::pauseTask(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.contains(id)) activeTasks_.at(id)->pause();
}

void Dispatcher::resumeTask(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.contains(id)) activeTasks_.at(id)->resume();
}

void Dispatcher::cancelTask(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.contains(id)) activeTasks_.at(id)->cancel();
}

void Dispatcher::handleTaskCreated(std::shared_ptr<edm::TaskContext> task) {
    qDebug() << "Dispatcher received task ID:" << task->model->id;

    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.find(task->model->id) != activeTasks_.end()) {
        return; // 已经在跑了
    }

    // 从 Model 转换为 Configure
    auto config = std::make_shared<TaskConfigure>(task->model);

    // 实例化真正的下载任务大脑
    auto downloadTask = std::make_shared<downloader::DownloadTask>(task);

    // 启动
    if (downloadTask->start()) {
        activeTasks_[task->model->id] = downloadTask;
        qDebug() << "Task ID" << task->model->id << "started successfully.";
    } else {
        qDebug() << "Failed to start Task ID" << task->model->id;
    }
}


} // namespace edm