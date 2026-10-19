#include "AppService.h"

#include "Dispatcher.h"
#include "EdmConfig.h"
#include "ITaskObserver.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "database/TaskModelConversion.h"
#include "downloader/DownloadTypes.h"
#include "utils/TaskLogger.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"

#include <QFile>
#include <QMetaObject>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace edm {

namespace {

/// 终态判定（与 Dispatcher 的回收规则保持一致）
bool isTerminalState(TaskState state) {
    return state == TaskState::Finished || state == TaskState::Failed || state == TaskState::Canceled;
}

} // namespace

struct AppService::Impl : public ITaskObserver {
    AppService* owner{nullptr};

    std::unique_ptr<DownloadDatabase> database_;
    std::unique_ptr<Dispatcher>       dispatcher_;

    // ---- 后台执行器（两条车道）----
    // 引擎的 createTask/pause/resume/cancel/removeTask 都会阻塞（元信息探测、
    // join worker、整文件 MD5 校验），全部丢到后台线程执行，GUI 只读原子状态。
    //
    // 分两条车道的原因：派发/继续/重新下载会因为在做网络探测而长时间占住线程，
    // 若暂停/取消排在它后面，用户看到的就还是"任务不响应任何操作"。
    //   dispatchQueue_：派发类（可长时间阻塞）
    //   controlQueue_ ：打断类（暂停/取消/删除），必须随叫随到
    std::thread                       dispatchThread_;
    std::thread                       controlThread_;
    std::mutex                        queueMutex_;
    std::condition_variable           queueCv_;
    std::deque<std::function<void()>> dispatchQueue_;
    std::deque<std::function<void()>> controlQueue_;
    std::atomic<bool>                 stopping_{false};

    explicit Impl(AppService* owner) : owner(owner) {
        // 构造即建库、建调度器并接线本观察器（路径来自 EdmConfig）。
        database_       = std::make_unique<DownloadDatabase>();
        dispatcher_     = std::make_unique<Dispatcher>(this);
        dispatchThread_ = std::thread([this] { executorLoop(dispatchQueue_); });
        controlThread_  = std::thread([this] { executorLoop(controlQueue_); });
    }

    ~Impl() override { stopExecutor(); }

    // ---------- 后台执行器 ----------

    /// 派发类任务（可能长时间阻塞）
    void postDispatch(std::function<void()> job) { post(dispatchQueue_, std::move(job)); }

    /// 打断类任务（暂停/取消/删除）：独立车道，不会被派发任务堵住
    void postControl(std::function<void()> job) { post(controlQueue_, std::move(job)); }

    void post(std::deque<std::function<void()>>& queue, std::function<void()> job) {
        if (stopping_.load(std::memory_order_acquire)) return;
        {
            std::lock_guard lock(queueMutex_);
            if (stopping_.load(std::memory_order_relaxed)) return;
            queue.push_back(std::move(job));
        }
        queueCv_.notify_all();
    }

    void executorLoop(std::deque<std::function<void()>>& queue) {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(queueMutex_);
                queueCv_.wait(lock, [this, &queue] { return stopping_.load() || !queue.empty(); });
                if (queue.empty()) return; // 只有停止且本车道已排空才会走到这里
                job = std::move(queue.front());
                queue.pop_front();
            }
            try {
                job();
            } catch (std::exception const& e) {
                qWarning() << "AppService: background job failed:" << e.what();
            }
        }
    }

    /// 停止并排空两条车道。必须在 reset dispatcher/database 之前调用。
    void stopExecutor() {
        stopping_.store(true, std::memory_order_release);
        queueCv_.notify_all();
        if (dispatchThread_.joinable()) dispatchThread_.join();
        if (controlThread_.joinable()) controlThread_.join();
    }

    /// 回到 GUI 线程执行（对象已销毁时 Qt 自动丢弃该事件）
    void runOnGui(std::function<void()> fn) {
        QMetaObject::invokeMethod(owner, std::move(fn), Qt::QueuedConnection);
    }

    /// 后台线程上报操作失败
    void reportError(int taskId, QString const& prefix, QString const& message) {
        runOnGui([this, taskId, prefix, message] {
            if (stopping_.load(std::memory_order_acquire)) return;
            emit owner->taskError(taskId, prefix + message);
        });
    }

    // ---------- 上下文构造 ----------

    /// 用数据库记录构造引擎上下文：带上该任务持久化的 header
    /// （浏览器集成写入的 origin/referer/cookie 靠这里生效），
    /// 并按任务自身记录的 useProxy 决定是否挂代理——创建/继续/重新下载都走这里，
    /// 保证"重新下载"用的是任务当初的参数，而不是当前全局设置。
    std::shared_ptr<TaskContext> makeContext(std::shared_ptr<TaskModel> const& model) {
        std::shared_ptr<TaskHeaderModel> header;
        try {
            header = database_->getTaskHeaderById(model->id);
        } catch (std::exception const& e) {
            qWarning() << "AppService: load header for task" << model->id << "failed:" << e.what();
        }

        auto configure = toTaskContext(model, header);
        if (configure && model->useProxy) {
            // 即使用户当前没配置代理服务器也照常发起（不预先拦截），
            // 失败会作为普通下载错误上报；此时按"不使用代理"处理。
            auto proxyUrl = EdmConfig::getInstance().getProxyConfig().toProxyUrl();
            if (proxyUrl.empty()) {
                configure->proxyUrl.reset();
            } else {
                configure->proxyUrl = std::move(proxyUrl);
            }
        }
        return configure;
    }

    // ---------- 派发 ----------

    /// 把上下文交给引擎（在后台线程调用）。失败时按 dropRowOnFailure 决定是否
    /// 删除刚插入的孤儿行，并回到 GUI 线程发信号通知。
    void dispatchNow(
        int                                 rowId,
        std::shared_ptr<TaskContext> const& configure,
        QString const&                      failurePrefix,
        bool                                dropRowOnFailure
    ) {
        auto created = dispatcher_ ? dispatcher_->createTask(configure)
                                   : Expected<>{makeStringError("Download engine is not initialized")};
        if (created || stopping_.load(std::memory_order_acquire)) return;

        auto const taskId = configure->id;
        auto       msg    = QString::fromStdString(created.error().message());
        runOnGui([this, taskId, rowId, failurePrefix, msg, dropRowOnFailure] {
            if (stopping_.load(std::memory_order_acquire)) return;
            if (dropRowOnFailure) {
                try {
                    database_->deleteTask(rowId);
                } catch (std::exception const& e) {
                    qWarning() << "AppService: cleanup orphan task" << rowId << "failed:" << e.what();
                }
                emit owner->taskRemoved(taskId);
            }
            emit owner->taskError(taskId, failurePrefix + msg);
        });
    }

    // ---- ITaskObserver（由引擎 worker 线程回调） ----

    void onStateChanged(std::shared_ptr<TaskContext>& ctx, TaskState /*oldState*/) override {
        if (!ctx) return;
        persistContext(*ctx);
        emit owner->taskStateChanged(ctx->id);
        notifyIfFinished(*ctx);
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

    /// 任务进入终态时发一条通知信号（托盘状态通知消费）。
    /// 用 int 传状态，避免 TaskState 未注册元类型导致跨线程队列连接失败。
    void notifyIfFinished(TaskContext const& ctx) {
        if (!isTerminalState(ctx.state)) return;

        QString errorMsg;
        if (ctx.state == TaskState::Failed) {
            // onError 先于 setState(Failed) 执行，错误文本此时已落库
            try {
                if (auto model = database_->getTaskById(ctx.id)) {
                    errorMsg = QString::fromStdString(model->errorMsg);
                }
            } catch (std::exception const& e) {
                qWarning() << "AppService: read error message for task" << ctx.id << "failed:" << e.what();
            }
        }
        emit owner->taskFinished(
            ctx.id,
            static_cast<int>(ctx.state),
            QString::fromStdString(ctx.fileName),
            errorMsg
        );
    }

    /// 将引擎最新上下文同步回数据库（并发安全：DownloadDatabase 自带锁；
    /// 任务可能已被删除则跳过）。DB 异常就地兜底，不抛出。
    void persistContext(TaskContext const& ctx, QString const& errorMsg = {}) {
        if (!database_) return;
        try {
            auto model = database_->getTaskById(ctx.id);
            if (!model) return;
            applyContextToModel(*model, ctx);
            if (!errorMsg.isEmpty()) {
                model->errorMsg = errorMsg.toStdString();
            } else if (ctx.state != TaskState::Failed) {
                // 任务重新跑起来了（继续/重新下载）：清掉上一轮的失败信息，
                // 否则界面上会一直挂着早已过期的错误。
                model->errorMsg.clear();
            }
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
    // 先停后台线程（排空队列），再释放调度器（任务析构会落库），最后释放数据库。
    impl->stopExecutor();
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
    model->useProxy    = useProxy; // 持久化：重新下载时据此恢复
    // 从 URL 路径推导占位文件名（在引擎解析真实文件名之前）
    model->fileName = utils::resolveFileName(stdUrl);

    // 持久化到数据库以获取有效 TaskId
    try {
        impl->database_->insertTask(model);
    } catch (std::exception const& e) {
        return makeStringError("Failed to persist task: " + std::string(e.what()));
    }

    // 转换为引擎配置
    auto configure = impl->makeContext(model);

    // 行先出现、派发放后台线程：元信息探测是网络请求，不阻塞 UI 线程
    emit taskCreated(model);
    auto* implPtr = impl.get();
    impl->postDispatch([implPtr, id = model->id, configure] {
        implPtr->dispatchNow(id, configure, QStringLiteral("创建任务失败: "), /*dropRowOnFailure=*/true);
    });
    return model;
}

Expected<> AppService::redownloadTask(int taskId) {
    if (!impl->database_) return makeStringError("Database is not initialized");

    std::shared_ptr<TaskModel> model;
    try {
        model = impl->database_->getTaskById(taskId);
    } catch (std::exception const& e) {
        return makeStringError("Failed to load task: " + std::string(e.what()));
    }
    if (!model) return makeStringError("Task not found");

    auto* implPtr = impl.get();
    impl->postDispatch([implPtr, taskId, model] {
        // 1) 先摘掉引擎里的旧任务（含终态残留）。removeTask 会 join worker，
        //    必须在动临时文件之前完成，否则会和仍在写的 worker 抢文件。
        if (implPtr->dispatcher_) implPtr->dispatcher_->removeTask(taskId);

        // 2) 清掉临时文件与旧进度；最终文件保留，下载校验通过后才覆盖
        auto basePath = QString::fromStdString(model->saveDir) + u'/' + QString::fromStdString(model->fileName);
        for (auto const& suffix : {QStringLiteral(".edm.meta"), QStringLiteral(".edm")}) {
            QFile::remove(basePath + suffix);
        }

        // 3) 重置任务记录。id 保持不变：headers 行与 spdlog 日志都绑定该 id。
        model->state      = TaskState::Pending;
        model->errorMsg.clear();
        model->firstTry   = time_utils::nowSeconds();
        model->lastTry    = model->firstTry;
        model->retryCount = kRetryCount;
        try {
            implPtr->database_->insertTask(model);
        } catch (std::exception const& e) {
            implPtr->reportError(
                taskId,
                QStringLiteral("重新下载失败: "),
                QStringLiteral("Failed to persist task: ") + QString::fromStdString(e.what())
            );
            return;
        }

        // 4) 重新派发：覆盖模式（下到临时文件，完成后替换同名文件）
        //    注意 dropRowOnFailure=false：任务本来就存在，派发失败只应记为失败并弹错误，
        //    不能把用户的任务从列表里删掉。
        auto configure               = implPtr->makeContext(model);
        configure->state             = TaskState::Pending;
        configure->overwriteExisting = true;
        implPtr->dispatchNow(taskId, configure, QStringLiteral("重新下载失败: "), /*dropRowOnFailure=*/false);

        implPtr->runOnGui([implPtr, taskId] {
            if (implPtr->stopping_.load(std::memory_order_acquire)) return;
            emit implPtr->owner->taskStateChanged(taskId);
        });
    });
    return {};
}

Expected<> AppService::pauseTask(int taskId) {
    if (!impl->dispatcher_) return makeStringError("Download engine is not initialized");

    auto* implPtr = impl.get();
    impl->postControl([implPtr, taskId] {
        if (auto paused = implPtr->dispatcher_->pauseTask(taskId); !paused) {
            // 任务不在引擎中（已完成被回收 / 从未派发）时无事可做，不算错误
            EDM_LOG_DEBUG(taskId, "AppService: pause ignored: {}", paused.error().message());
        }
    });
    return {};
}

Expected<> AppService::resumeTask(int taskId) {
    if (!impl->database_) return makeStringError("Database is not initialized");

    // 任务已在调度器中（本会话刚创建/恢复过）：直接交给 Dispatcher 恢复
    if (impl->dispatcher_ && impl->dispatcher_->getTaskState(taskId)) {
        auto* implPtr = impl.get();
        impl->postControl([implPtr, taskId] {
            if (auto resumed = implPtr->dispatcher_->resumeTask(taskId); !resumed) {
                implPtr->reportError(
                    taskId,
                    QStringLiteral("继续下载失败: "),
                    QString::fromStdString(resumed.error().message())
                );
            }
        });
        return {};
    }

    // 重启后任务不在调度器中：从数据库重建并派发，从 .meta 续传。
    // 不自动续传，只有用户明确点击"继续"才会恢复下载。
    std::shared_ptr<TaskModel> model;
    try {
        model = impl->database_->getTaskById(taskId);
    } catch (std::exception const& e) {
        return makeStringError("Failed to load task: " + std::string(e.what()));
    }
    if (!model) return makeStringError("Task not found");
    if (model->state == TaskState::Finished) {
        return makeStringError("任务已完成，请使用\"重新下载\"");
    }
    if (model->state == TaskState::Running) {
        // 崩溃/异常退出残留：库里写着运行中但引擎里没有，按暂停处理
        model->state = TaskState::Paused;
    }

    auto configure   = impl->makeContext(model);
    configure->state = TaskState::Paused; // Dispatcher 只允许从非 Running 状态启动
    auto* implPtr    = impl.get();
    impl->postDispatch([implPtr, id = model->id, configure] {
        implPtr->dispatchNow(id, configure, QStringLiteral("继续下载失败: "), /*dropRowOnFailure=*/false);
    });
    return {};
}

Expected<> AppService::cancelTask(int taskId) {
    if (!impl->dispatcher_) return makeStringError("Download engine is not initialized");

    auto* implPtr = impl.get();
    impl->postControl([implPtr, taskId] {
        if (auto canceled = implPtr->dispatcher_->cancelTask(taskId); !canceled) {
            implPtr->reportError(
                taskId,
                QStringLiteral("取消失败: "),
                QString::fromStdString(canceled.error().message())
            );
        }
    });
    return {};
}

Expected<> AppService::deleteTask(int taskId, bool deleteFiles) {
    if (!impl->database_) return makeStringError("Database is not initialized");

    auto* implPtr = impl.get();
    impl->postControl([implPtr, taskId, deleteFiles] {
        // 先摘除引擎中的任务（best-effort 取消并 join worker），再动磁盘与数据库
        if (implPtr->dispatcher_) implPtr->dispatcher_->removeTask(taskId);

        if (deleteFiles) {
            try {
                auto model = implPtr->database_->getTaskById(taskId);
                if (model) {
                    auto basePath = QString::fromStdString(model->saveDir) + u'/' + QString::fromStdString(model->fileName);
                    // 删除最终文件、临时下载文件与进度元数据
                    QString const suffixes[] = {QStringLiteral(""), QStringLiteral(".edm"), QStringLiteral(".edm.meta")};
                    for (auto const& suffix : suffixes) {
                        QFile::remove(basePath + suffix);
                    }
                }
            } catch (std::exception const& e) {
                implPtr->reportError(
                    taskId,
                    QStringLiteral("删除文件失败: "),
                    QString::fromStdString(e.what())
                );
                return;
            }
        }

        bool removed{false};
        try {
            removed = implPtr->database_->deleteTask(taskId);
        } catch (std::exception const& e) {
            implPtr->reportError(
                taskId,
                QStringLiteral("删除任务失败: "),
                QString::fromStdString(e.what())
            );
            return;
        }
        if (!removed) {
            implPtr->reportError(taskId, QStringLiteral("删除任务失败: "), QStringLiteral("Task not found in database"));
            return;
        }

        implPtr->runOnGui([implPtr, taskId] {
            if (implPtr->stopping_.load(std::memory_order_acquire)) return;
            emit implPtr->owner->taskRemoved(taskId);
        });
    });
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
