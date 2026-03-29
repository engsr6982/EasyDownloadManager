#include "DownloadTask.h"

#include <qdebug.h>
#include <qlogging.h>

namespace edm ::downloader {


DownloadTask::DownloadTask(TaskConfigure configure) : configure_(std::move(configure)) {
    std::unique_lock<std::shared_mutex> lock{mutex_}; // 占用锁进行初始化任务
    state_ = TaskState::Pending;
}

DownloadTask::~DownloadTask() {
    std::unique_lock<std::shared_mutex> lock{mutex_}; // 占用锁进行析构任务
    if (state_ == TaskState::Running) {
        qDebug() << "[DownloadTask::~DownloadTask()]" << "Warning: task is still running, force to cancel";
    }
    // TODO: impl
}

bool DownloadTask::start() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (state_ != TaskState::Pending) {
        return false;
    }
    state_ = TaskState::Running;
    // TODO: impl
}

bool DownloadTask::pause() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (state_ != TaskState::Running) {
        return false;
    }
    state_ = TaskState::Paused;
    // TODO: impl
}

bool DownloadTask::resume() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (state_ != TaskState::Paused) {
        return false;
    }
    state_ = TaskState::Running;
    // TODO: impl
}

bool DownloadTask::cancel() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (state_ != TaskState::Running) {
        return false;
    }
    state_ = TaskState::Canceled;
    // TODO: impl
}

bool DownloadTask::isFinished() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return state_ == TaskState::Finished;
}

bool DownloadTask::isPaused() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return state_ == TaskState::Paused;
}

bool DownloadTask::isCanceled() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return state_ == TaskState::Canceled;
}

bool DownloadTask::isRunning() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return state_ == TaskState::Running;
}

double DownloadTask::getProgress() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return 0; // TODO: impl
}

std::optional<std::string> const& DownloadTask::getLastError() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return lastError_;
}

TaskState DownloadTask::getState() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return state_;
}


} // namespace edm::downloader