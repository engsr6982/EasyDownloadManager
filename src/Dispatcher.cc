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
        return TaskSnapshot{task->getProgress(), task->getSpeed(), task->getState()};
    }
    return std::nullopt;
}
void Dispatcher::updateAllSpeeds() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    for (auto& [id, task] : activeTasks_) {
        task->updateSpeed();
    }
}

void Dispatcher::handleDispatchTask(edm::TaskModel const& task) {
    qDebug() << "Dispatcher received task ID:" << task.id;

    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (activeTasks_.find(task.id) != activeTasks_.end()) {
        return; // 已经在跑了
    }

    // 从 Model 转换为 Configure
    downloader::TaskConfigure config;
    config.url_            = task.url;
    config.saveDir_        = task.saveDir;
    config.threadCount_    = task.threadCount;
    config.bandWidthLimit_ = task.bandWidthLimit;
    config.userAgent_      = task.userAgent;

    // 实例化真正的下载任务大脑
    auto downloadTask = std::make_shared<downloader::DownloadTask>(task, config);

    // 启动
    if (downloadTask->start()) {
        activeTasks_[task.id] = downloadTask;
        qDebug() << "Task ID" << task.id << "started successfully.";
    } else {
        qDebug() << "Failed to start Task ID" << task.id;
    }
}


} // namespace edm