#include "Dispatcher.h"

#include "downloader/DownloadTask.h"
#include "downloader/TaskConfigure.h"
#include "event/EventBus.h"

#include <QDebug>
#include <qobject.h>

namespace edm {


Dispatcher::Dispatcher() {
    // 监听新增任务的调度请求
    connect(EventBus::instance(), &EventBus::onRequestDispatchTask, this, &Dispatcher::handleDispatchTask);
}

Dispatcher::~Dispatcher() = default;


std::optional<Dispatcher::TaskSnapshot> Dispatcher::getTaskSnapshot(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto                        it = activeTasks_.find(id);
    if (it != activeTasks_.end()) {
        auto task = it->second;
        return TaskSnapshot{
            task->getModel(),
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
    if (activeTasks_.count(id)) activeTasks_[id]->pause();
}

void Dispatcher::resumeTask(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.count(id)) activeTasks_[id]->resume();
}

void Dispatcher::cancelTask(int id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.count(id)) activeTasks_[id]->cancel();
}

void Dispatcher::handleDispatchTask(std::shared_ptr<edm::TaskModel> task) {
    qDebug() << "Dispatcher received task ID:" << task->id;

    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.find(task->id) != activeTasks_.end()) {
        return; // 已经在跑了
    }

    // 从 Model 转换为 Configure
    auto config = std::make_shared<TaskConfigure>(task);

    // 实例化真正的下载任务大脑
    auto downloadTask = std::make_shared<downloader::DownloadTask>(task, config);

    // 启动
    if (downloadTask->start()) {
        activeTasks_[task->id] = downloadTask;
        qDebug() << "Task ID" << task->id << "started successfully.";
    } else {
        qDebug() << "Failed to start Task ID" << task->id;
    }
}


} // namespace edm