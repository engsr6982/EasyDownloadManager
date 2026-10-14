#pragma once

#include "Global.h"

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace edm {

class AppService;
class QmlTaskDetail;
struct TaskModel;

namespace downloader {
class DownloadState;
} // namespace downloader

/**
 * @brief QML-facing task list model.
 *
 * A thin view model over `AppService`: all task operations (create / pause /
 * resume / cancel / delete) are delegated to the service, and row structure is
 * driven by the service's signals (taskCreated / taskRemoved / taskStateChanged...).
 * A 1-second timer calls AppService::updateSpeeds and refreshes the visible
 * state so QML rebinds via dataChanged.
 */
class QmlTaskListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        TaskIdRole = Qt::UserRole + 1,
        FileNameRole,
        FileSizeStrRole,
        ProgressValueRole,
        SpeedStrRole,
        StatusStrRole,
        UrlStrRole,
        SaveDirRole,
        ReferrerRole,
        TimestampRole,
        CategoryRole,
        FirstTryRole,
        LastTryRole,
    };
    Q_ENUM(Roles)

    explicit QmlTaskListModel(QObject* parent = nullptr);
    ~QmlTaskListModel() override;

    // ---- QAbstractListModel ----
    int                    rowCount(QModelIndex const& parent = {}) const override;
    QVariant               data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool                   canFetchMore(QModelIndex const& parent) const override;
    void                   fetchMore(QModelIndex const& parent) override;

    // ---- Wiring (called from EdmApplication) ----
    /// 注入服务并建立信号连接（taskCreated/taskRemoved/taskStateChanged/...）。
    void setService(AppService* service);

    /// Reload all rows from the database (called once on init).
    Q_INVOKABLE void reloadFromDatabase();

    /// Start the periodic refresh timer.
    Q_INVOKABLE void startRefresh(int intervalMs = 1000);
    Q_INVOKABLE void stopRefresh();

    // ---- Task actions（均委托给 AppService；失败通过信号携带错误详情） ----
    /// 创建任务，成功返回 true；失败返回 false 并发出 createTaskError。
    Q_INVOKABLE bool createTask(QString const& url, QString const& saveDir, bool useProxy);

    Q_INVOKABLE bool pauseTask(int taskId);
    Q_INVOKABLE bool resumeTask(int taskId);
    Q_INVOKABLE bool cancelTask(int taskId);

    /// 删除任务；deleteFiles 为 true 时一并清理磁盘文件（含 .edm/.edm.meta）。
    /// 成功后对应行由 AppService::taskRemoved 信号移除。
    Q_INVOKABLE bool deleteTask(int taskId, bool deleteFiles);

    /// Returns a live QmlTaskDetail for the drawer. May return nullptr if the
    /// task isn't tracked. The QObject is owned by this model.
    Q_INVOKABLE QmlTaskDetail* getTaskDetail(int taskId);

signals:
    void countChanged();
    void createTaskError(QString const& url, QString const& message);
    void taskActionError(QString const& action, int taskId, QString const& message);

private slots:
    void onRefreshTick();
    void onServiceTaskCreated(std::shared_ptr<TaskModel> const& model);
    void onServiceTaskRemoved(int taskId);
    void onServiceTaskStateChanged(int taskId);
    void onServiceTaskError(int taskId, QString const& message);
    void onServiceTaskFileNameChanged(int taskId);

private:
    struct Row {
        std::shared_ptr<TaskModel>                 model;
        std::shared_ptr<downloader::DownloadState> liveState; // may be null
    };

    int  findRow(int taskId) const;
    void insertRowFromModel(std::shared_ptr<TaskModel> const& model);
    void removeRow(int row);
    void refreshRow(int row);
    /// 从数据库重读某行模型（用于错误/文件名变更后刷新显示）。
    void reloadRowFromService(int taskId);

    // QML/snapshot formatters
    static QString formatFileSize(FileSize size);
    static QString formatSpeed(double bytesPerSec);
    static QString formatStatus(TaskState s);
    static QString formatTimestamp(time_t t);
    static double  computeProgress(int64_t downloaded, int64_t total);
    static QString formatProgressBar(bool indeterminate, double progress, TaskState state);

    AppService* service_{nullptr};

    QTimer* refreshTimer_{nullptr};

    std::vector<Row>                rows_;    // index = row
    std::unordered_map<int, size_t> idToRow_; // taskId -> index in rows_

    static constexpr int kBatchSize = 50; // 每次从数据库加载的行数
    int                  totalCount_{0};  // 数据库中的任务总数

    /// 从数据库加载下一批任务并追加到 rows_
    void loadNextBatch();

    // Cached detail objects, lazily created on getTaskDetail()
    std::unordered_map<int, QPointer<QmlTaskDetail>> details_;
};

} // namespace edm
