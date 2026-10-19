#pragma once

#include "expected.h"

#include <QObject>
#include <QString>

#include <memory>

namespace edm {

class Dispatcher;
class DownloadDatabase;
struct TaskModel;
struct TaskContext;

namespace downloader {
class DownloadState;
} // namespace downloader

/**
 * @brief 应用层服务（非 QML 单例），由 `EdmApplication` 持有。
 *
 * 拥有 `DownloadDatabase`、`Dispatcher` 及 `ITaskObserver` 实现，并对外提供
 * 全部任务操作：
 * - 任务操作（create/pause/resume/cancel/delete）统一在此实现，以 `Expected<>`
 *   返回，错误信息原样上抛，视图层直接展示 `.error().message()`；
 * - 观察器由引擎 worker 线程回调：把状态/错误/文件名变化写回数据库，并通过
 *   Qt 信号通知界面（跨线程 emit 自动走 QueuedConnection）。
 *
 * 销毁前须先调用 `shutdown()`（`EdmApplication::shutdown()` 中已完成该步骤）。
 */
class AppService : public QObject {
    Q_OBJECT
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    explicit AppService(QObject* parent = nullptr);
    ~AppService() override;
    Q_DISABLE_COPY_MOVE(AppService);

    [[nodiscard]] Dispatcher*       dispatcher() const;
    [[nodiscard]] DownloadDatabase* database() const;

    /// 停止调度器并释放数据库（会触发现有任务进入 Paused 并持久化）。
    void shutdown();

    // ---- 任务操作 ----
    // 以下操作的返回值只表示"已受理"；真正的引擎调用在后台线程串行执行
    // （引擎的 start/pause/cancel 会做网络探测、join worker、整文件校验，都是秒级阻塞），
    // 失败通过 taskError / taskActionError 异步回传。
    /// 从配置构造 TaskModel、落库取有效 id、转 TaskContext。返回 model 并同步发出
    /// taskCreated；实际派发放到后台线程（元信息探测是网络请求），避免卡住 UI 线程。
    Expected<std::shared_ptr<TaskModel>> createTask(QString const& url, QString const& saveDir, bool useProxy);

    Expected<> pauseTask(int taskId);
    Expected<> resumeTask(int taskId);
    Expected<> cancelTask(int taskId);

    /// 取消（best-effort）→ 可选删除磁盘文件 → 删除数据库记录 → 发出 taskRemoved。
    Expected<> deleteTask(int taskId, bool deleteFiles);

    /// 重新下载：摘除旧任务、清除进度文件后从零再下一次（用于已完成/失败任务）。
    /// 下载期间旧文件保持不动，校验通过后才覆盖同名文件。
    Expected<> redownloadTask(int taskId);

    // ---- 供视图模型轮询 ----
    std::shared_ptr<downloader::DownloadState> taskState(int taskId) const;
    void                                       updateSpeeds();

signals:
    /// 任务创建成功，视图模型据此插入或刷新对应行。
    void taskCreated(std::shared_ptr<TaskModel> model);
    /// 任务已从数据库删除，视图应移除对应行。
    void taskRemoved(int taskId);
    /// 引擎状态变化（可能由 worker 线程发出，跨线程自动排队）。
    void taskStateChanged(int taskId);
    /// 引擎解析出最终文件名（如来自 Content-Disposition）。
    void taskFileNameChanged(int taskId);
    /// 引擎回传的错误（已持久化到数据库 errorMsg）。
    void taskError(int taskId, QString const& message);
    /// 任务进入终态（完成/失败/取消），供托盘状态通知等消费。
    /// state 用 int 传，避免 TaskState 未注册元类型导致跨线程队列连接失败。
    void taskFinished(int taskId, int state, QString const& fileName, QString const& errorMsg);
};

} // namespace edm