#include "AppService.h"

#include "Dispatcher.h"
#include "EdmConfig.h"
#include "ITaskObserver.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "database/TaskModelConversion.h"
#include "downloader/DownloadTypes.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"

#include <QFile>

#include <exception>
#include <string>

namespace edm {

struct AppService::Impl : public ITaskObserver {
    AppService* owner{nullptr};

    std::unique_ptr<DownloadDatabase> database_;
    std::unique_ptr<Dispatcher>       dispatcher_;

    explicit Impl(AppService* owner) : owner(owner) {
        // 构造即建库、建调度器并接线本观察器（路径来自 EdmConfig）。
        database_   = std::make_unique<DownloadDatabase>();
        dispatcher_ = std::make_unique<Dispatcher>(this);
    }

    // ---- ITaskObserver（由引擎 worker 线程回调） ----

    void onStateChanged(std::shared_ptr<TaskContext>& ctx, TaskState /*oldState*/) override {
        if (!ctx) return;
        persistContext(*ctx);
        emit owner->taskStateChanged(ctx->id);
    }

    void onError(std::shared_ptr<TaskContext>& ctx, std::string const& error) override {
        if (!ctx) return;
        persistContext(*ctx, QString::fromStdString(error));
        emit owner->taskError(ctx->id, QString::fromStdString(error));
    }

    void onFileNameChanged(std::shared_ptr<TaskContext>& ctx) override {
        if (!ctx) return;
        persistContext(*ctx);
        emit owner->taskFileNameChanged(ctx->id);
    }

    /// 将引擎最新上下文同步回数据库（并发安全：DownloadDatabase 自带锁；
    /// 任务可能已被删除则跳过）。DB 异常就地兜底，不抛出。
    void persistContext(TaskContext const& ctx, QString const& errorMsg = {}) {
        if (!database_) return;
        try {
            auto model = database_->getTaskById(ctx.id);
            if (!model) return;
            applyContextToModel(*model, ctx);
            if (!errorMsg.isEmpty()) model->errorMsg = errorMsg.toStdString();
            model->lastTry = time_utils::nowSeconds();
            database_->insertTask(model);
        } catch (std::exception const& e) {
            qWarning() << "AppService: persist task" << ctx.id << "failed:" << e.what();
        }
    }
};

AppService::AppService(QObject* parent) : QObject(parent), impl(std::make_unique<Impl>(this)) {}
AppService::~AppService() = default;

Dispatcher*       AppService::dispatcher() const { return impl->dispatcher_.get(); }
DownloadDatabase* AppService::database() const { return impl->database_.get(); }

void AppService::shutdown() {
    impl->dispatcher_.reset();
    impl->database_.reset();
}

Expected<std::shared_ptr<TaskModel>> AppService::createTask(QString const& url, QString const& saveDir, bool useProxy) {
    auto stdUrl     = url.toStdString();
    auto stdSaveDir = saveDir.toStdString();
    if (stdUrl.empty()) return makeStringError("URL is empty");

    auto& conf = EdmConfig::getInstance();

    auto model         = TaskModel::make();
    model->url         = stdUrl;
    model->bandLimit   = conf.getBandwidthLimit();
    model->threadCount = conf.getThreadCount();
    model->retryCount  = kRetryCount;
    model->firstTry    = time_utils::nowSeconds();
    model->lastTry     = model->firstTry;
    model->userAgent   = conf.getUserAgent().toStdString();
    model->saveDir     = stdSaveDir;
    // 从 URL 路径推导占位文件名（在引擎解析真实文件名之前）
    model->fileName = utils::resolveFileName(stdUrl);

    // 持久化到数据库以获取有效 TaskId
    try {
        impl->database_->insertTask(model);
    } catch (std::exception const& e) {
        return makeStringError("Failed to persist task: " + std::string(e.what()));
    }

    // 转换为引擎配置
    auto configure = toTaskContext(model);
    if (useProxy) {
        if (auto proxy = conf.getProxyConfig(); !proxy.isEnabled()) {
            configure->proxyUrl = proxy.toProxyUrl();
        }
    }

    if (auto created = impl->dispatcher_->createTask(configure); !created) {
        // 派发失败即删除刚插入的孤儿行，避免重启后残留未派发的 Pending 任务。
        try {
            impl->database_->deleteTask(model->id);
        } catch (std::exception const& e) {
            qWarning() << "AppService: cleanup orphan task" << model->id << "failed:" << e.what();
        }
        return forwardError(created.error());
    }

    emit taskCreated(model);
    return model;
}

Expected<> AppService::pauseTask(int taskId) {
    if (!impl->dispatcher_) return makeStringError("Download engine is not initialized");
    return impl->dispatcher_->pauseTask(taskId);
}

Expected<> AppService::resumeTask(int taskId) {
    if (!impl->dispatcher_) return makeStringError("Download engine is not initialized");
    return impl->dispatcher_->resumeTask(taskId);
}

Expected<> AppService::cancelTask(int taskId) {
    if (!impl->dispatcher_) return makeStringError("Download engine is not initialized");
    return impl->dispatcher_->cancelTask(taskId);
}

Expected<> AppService::deleteTask(int taskId, bool deleteFiles) {
    // best-effort 取消；任务可能已处于终态，忽略取消错误
    if (impl->dispatcher_) (void)impl->dispatcher_->cancelTask(taskId);

    if (deleteFiles && impl->database_) {
        try {
            auto model = impl->database_->getTaskById(taskId);
            if (model) {
                auto basePath = QString::fromStdString(model->saveDir) + u'/' + QString::fromStdString(model->fileName);
                // 删除最终文件、临时下载文件与进度元数据
                QString const suffixes[] = {QStringLiteral(""), QStringLiteral(".edm"), QStringLiteral(".edm.meta")};
                for (auto const& suffix : suffixes) {
                    QFile::remove(basePath + suffix);
                }
            }
        } catch (std::exception const& e) {
            return makeStringError("Failed to delete files: " + std::string(e.what()));
        }
    }

    bool removed{false};
    try {
        removed = impl->database_->deleteTask(taskId);
    } catch (std::exception const& e) {
        return makeStringError("Failed to delete task: " + std::string(e.what()));
    }
    if (!removed) return makeStringError("Task not found in database");

    emit taskRemoved(taskId);
    return {};
}

std::shared_ptr<downloader::DownloadState> AppService::taskState(int taskId) const {
    if (!impl->dispatcher_) return nullptr;
    return impl->dispatcher_->getTaskState(taskId);
}

void AppService::updateSpeeds() {
    if (impl->dispatcher_) impl->dispatcher_->updateAllSpeeds();
}

} // namespace edm