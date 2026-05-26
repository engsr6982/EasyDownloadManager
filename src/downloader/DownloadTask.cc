#include "DownloadTask.h"

#include "DownloadWorker.h"
#include "TaskConfigure.h"
#include "dto/TaskContext.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThreadPool>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>

namespace edm ::downloader {

constexpr auto TMP_OUT_FILE_EXT = ".edm";

DownloadTask::DownloadTask(std::shared_ptr<edm::TaskContext> ctx)
: context_(std::move(ctx)),
  isRunningFlag_(std::make_shared<std::atomic<bool>>(false)) {
    outFilePath_ = fmt::format("{}/{}{}", context_->configure->saveDir_, context_->model->fileName, TMP_OUT_FILE_EXT);
    context_->model->state = TaskState::Pending;
}

DownloadTask::~DownloadTask() {
    std::unique_lock<std::shared_mutex> lock{mutex_}; // 占用锁进行析构任务
    if (context_->model->state == TaskState::Running) {
        qDebug() << "[DownloadTask::~DownloadTask()]" << "Warning: task is still running, force to cancel";
    }
    // TODO: impl
}

bool DownloadTask::start() {
    std::unique_lock<std::shared_mutex> lock{mutex_};

    auto model = context_->model;
    assert(model != nullptr);
    if (model->state != TaskState::Pending && model->state != TaskState::Paused && model->state != TaskState::Failed) {
        return false;
    }

    if (!context_->meta) {
        // todo: fetch meta info
    }

    // 预分配磁盘文件
    std::filesystem::path p(outFilePath_);
    if (!std::filesystem::exists(p)) {
        // 创建空文件
        std::ofstream ofs(p, std::ios::binary);
        ofs.close();
        // 预分配到目标大小
        std::error_code ec;
        std::filesystem::resize_file(p, model->fileSize, ec);
        if (ec) {
            model->errorMsg = "Failed to pre-allocate disk space: " + ec.message();
            return false;
        }
    }

    // 切分 Range (如果尚未切分)
    if (ranges_.empty()) {
        int threads = model->resumable == Resumable::Yes ? context_->configure->threadCount_ : 1;
        if (threads <= 0) threads = 1;

        qint64 partSize = model->fileSize / threads;
        for (int i = 0; i < threads; ++i) {
            qint64 start = i * partSize;
            qint64 end   = (i == threads - 1) ? (model->fileSize - 1) : (start + partSize - 1);
            ranges_.push_back(std::make_shared<DownloadRange>(start, end));
        }
    }

    model->state = TaskState::Running;
    isRunningFlag_->store(true, std::memory_order_relaxed);

    // 更新数据库状态
    // model_->state = context_->model->state;
    // EdmApplication::getInstance().getDatabase()->insertTask(model_);

    // 将所有未完成的块派发到线程池
    for (auto& range : ranges_) {
        if (range->downloaded.load() < (range->end - range->start + 1)) {
            activeWorkers_.fetch_add(1); // 增加活跃计数

            // 使用 weak_ptr 防止毁坏性回调
            std::weak_ptr<DownloadTask> weakSelf = shared_from_this();

            auto worker = new DownloadWorker(
                context_->configure,
                outFilePath_,
                range,
                isRunningFlag_,
                [weakSelf](bool /* success */) {
                    if (auto self = weakSelf.lock()) {
                        // 一个线程结束，计数减1。如果是最后一个线程，触发收尾
                        if (self->activeWorkers_.fetch_sub(1) == 1) {
                            self->finalizeTask();
                        }
                    }
                }
            );
            QThreadPool::globalInstance()->start(worker);
        }
    }

    return true;
}

void DownloadTask::finalizeTask() {
    std::unique_lock<std::shared_mutex> lock{mutex_};

    // 检查是否是被用户手动暂停或取消引起的退出
    if (context_->model->state == TaskState::Paused || context_->model->state == TaskState::Canceled) {
        return;
    }

    // 检查是否真正全部下载完成
    qint64 totalDownloaded = 0;
    for (const auto& r : ranges_) {
        totalDownloaded += r->downloaded.load(std::memory_order_relaxed);
    }

    if (totalDownloaded < context_->model->fileSize) {
        context_->model->state    = TaskState::Failed;
        context_->model->errorMsg = "Download incomplete, network error.";
        context_->model->state    = context_->model->state;
        // EdmApplication::getInstance().getDatabase()->insertTask(context_->model);
        return;
    }

    // =========== 下载完成，执行校验和移动逻辑 ===========

    // TODO: 可以在这里读取 tempFilePath_ 的文件流计算 MD5，并和 model 里的 MD5 比较

    // 处理最终路径
    QString finalPath = QString::fromStdString(outFilePath_);

    auto idx = outFilePath_.rfind(TMP_OUT_FILE_EXT);
    if (idx != std::string::npos) {
        finalPath = QString::fromStdString(outFilePath_.substr(0, idx));
    }

    // 如果文件已存在，自动重命名 (比如 file(1).zip)


    int       counter = 1;
    QFileInfo fileInfo(finalPath);
    QDir      dir      = fileInfo.dir();
    QString   baseName = fileInfo.completeBaseName();
    QString   suffix   = fileInfo.suffix();
    while (QFile::exists(finalPath)) {
        QString newName = QString("%1(%2).%3").arg(baseName).arg(counter).arg(suffix);
        if (suffix.isEmpty()) newName = QString("%1(%2)").arg(baseName).arg(counter);
        finalPath = dir.filePath(newName);
        counter++;
    }

    if (QFile::rename(QString::fromStdString(outFilePath_), finalPath)) {
        qDebug() << "File successfully moved to" << finalPath;
        context_->model->state    = TaskState::Finished;
        context_->model->fileName = QFileInfo(finalPath).fileName().toStdString(); // 更新为可能重命名后的最终文件名
        currentSpeed_             = 0.0;
    } else {
        qDebug() << "Failed to rename file!";
        context_->model->state    = TaskState::Failed;
        context_->model->errorMsg = "Failed to rename file!";
    }

    // 更新数据库并保存最终状态
    context_->model->state = context_->model->state;
    // EdmApplication::getInstance().getDatabase()->insertTask(context_->model);
}

bool DownloadTask::pause() {
    std::unique_lock<std::shared_mutex> lock{mutex_};
    if (context_->model->state != TaskState::Running) return false;

    context_->model->state = TaskState::Paused;
    // 这一行会让所有该任务的 Worker 退出下载循环
    isRunningFlag_->store(false, std::memory_order_relaxed);

    // TODO: 通知数据库更新进度和状态
    return true;
}

bool DownloadTask::resume() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (context_->model->state != TaskState::Paused) {
        return false;
    }
    context_->model->state = TaskState::Running;
    // TODO: impl
}

bool DownloadTask::cancel() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (context_->model->state != TaskState::Running) {
        return false;
    }
    context_->model->state = TaskState::Canceled;
    // TODO: impl
}

bool DownloadTask::isFinished() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->state == TaskState::Finished;
}

bool DownloadTask::isPaused() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->state == TaskState::Paused;
}

bool DownloadTask::isCanceled() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->state == TaskState::Canceled;
}

bool DownloadTask::isRunning() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->state == TaskState::Running;
}

double DownloadTask::getProgress() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (context_->model->fileSize <= 0) return 0.0;

    qint64 totalDownloaded = 0;
    for (const auto& r : ranges_) {
        totalDownloaded += r->downloaded.load(std::memory_order_relaxed);
    }
    return static_cast<double>(totalDownloaded) / context_->model->fileSize;
}

void DownloadTask::updateSpeed() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    if (context_->model->state != TaskState::Running) {
        currentSpeed_ = 0.0;
        return;
    }
    qint64 total = 0;
    for (const auto& r : ranges_) {
        total += r->downloaded.load(std::memory_order_relaxed);
    }
    currentSpeed_        = static_cast<double>(total - lastDownloadedBytes_); // 因为是 1 秒调用一次，差值就是 speed
    lastDownloadedBytes_ = total;
}

std::shared_ptr<edm::TaskContext> DownloadTask::getTaskContext() const { return context_; }

qint64 DownloadTask::getDownloadedBytes() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    qint64                              total = 0;
    for (const auto& r : ranges_) {
        total += r->downloaded.load(std::memory_order_relaxed);
    }
    return total;
}

double DownloadTask::getSpeed() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return currentSpeed_;
}

std::optional<std::string> DownloadTask::getLastError() {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->errorMsg;
}

TaskState DownloadTask::getState() const {
    std::shared_lock<std::shared_mutex> lock{mutex_};
    return context_->model->state;
}


} // namespace edm::downloader