#pragma once

#include "Global.h"

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace edm {

class Dispatcher;
class DownloadDatabase;
class QmlTaskDetail;
struct TaskModel;
struct TaskHeaderModel;
struct TaskContext;

namespace downloader {
class DownloadState;
} // namespace downloader

/**
 * @brief QML-facing task list model.
 *
 * Backed by the C++ Dispatcher (live state) and DownloadDatabase (persisted
 * tasks). Exposes a row per task whose roles match the TaskCard delegate's
 * property names so QML can bind directly: taskId, fileName, fileSizeStr,
 * progressValue, speedStr, statusStr, urlStr, saveDir, referrer, timestamp.
 *
 * Provides task actions (create / pause / resume / cancel / delete) as
 * Q_INVOKABLE methods. A 1-second timer calls Dispatcher::updateAllSpeeds
 * and refreshes the visible state so QML rebinds via dataChanged.
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

    // ---- Wiring (called from EdmApplication::initApp) ----
    void setDispatcher(Dispatcher* d);
    void setDatabase(DownloadDatabase* db);

    /// Reload all rows from the database (called once on init).
    Q_INVOKABLE void reloadFromDatabase();

    /// Start the periodic refresh timer.
    Q_INVOKABLE void startRefresh(int intervalMs = 1000);
    Q_INVOKABLE void stopRefresh();

    // ---- Task actions ----
    /// Build a TaskModel+TaskContext and submit to Dispatcher. Returns true on
    /// success; on failure emits createTaskError.
    Q_INVOKABLE bool createTask(QString const& url, QString const& saveDir, bool useProxy);

    Q_INVOKABLE bool pauseTask(int taskId);
    Q_INVOKABLE bool resumeTask(int taskId);
    Q_INVOKABLE bool cancelTask(int taskId);

    /// Delete the task. deleteFiles: also remove the on-disk file. (For now
    /// we only remove the row from the model; file deletion is a TODO since
    /// it needs to interact with the in-flight DownloadTask.)
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

private:
    struct Row {
        std::shared_ptr<TaskModel>                 model;
        std::shared_ptr<downloader::DownloadState> liveState; // may be null
    };

    int  findRow(int taskId) const;
    void insertRowFromModel(std::shared_ptr<TaskModel> const& model);
    void removeRow(int row);
    void refreshRow(int row);

    // QML/snapshot formatters
    static QString formatFileSize(FileSize size);
    static QString formatSpeed(double bytesPerSec);
    static QString formatStatus(TaskState s);
    static QString formatTimestamp(time_t t);
    static double  computeProgress(int64_t downloaded, int64_t total);
    static QString formatProgressBar(bool indeterminate, double progress, TaskState state);

    Dispatcher*       dispatcher_{nullptr};
    DownloadDatabase* database_{nullptr};

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
