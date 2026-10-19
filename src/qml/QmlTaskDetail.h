#pragma once

#include "Global.h"

#include <QAbstractListModel>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace edm {

namespace downloader {
class DownloadState;
struct DownloadRange;
} // namespace downloader

/**
 * @brief 任务详情里的线程状态列表。
 *
 * 一行 = 一个引擎中实际存在的下载线程。引擎是工作窃取式分片池，线程与分片并非
 * 固定绑定，所以每行展示的是"该线程此刻正在处理哪个分片"，数据直接来自
 * DownloadState::workers 槽位（由对应 worker 独占写入），不再做任何分桶近似。
 * 行数 = 实际线程数；任务未启动时回落到配置线程数占位并显示 N/A。
 */
class QmlRangeModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by QmlTaskDetail")

    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)

public:
    enum Roles {
        RangeIndexRole = Qt::UserRole + 1,
        RangeStatusRole,
        RangeProgressRole,
        RangeDownloadedRole,
        RangeSizeRole,
        RangeStartRole,
        RangeEndRole,
        RangeAttemptsRole,
        RangeDownloadedStrRole,
        RangeSizeStrRole,
        RangeSpeedStrRole,
    };

    /// 线程槽位上限，必须与 DownloadState::kMaxWorkers 一致（.cc 中有 static_assert 兜底）
    static constexpr int kMaxWorkerSlots = 32;

    explicit QmlRangeModel(QObject* parent = nullptr);

    int                    rowCount(QModelIndex const& parent = {}) const override;
    QVariant               data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// 是否有可展示的实时线程数据（未启动/无实时状态时为 false）。
    bool isActive() const { return active_; }

    /// 设置占位行数（= 配置的线程数），仅在拿不到实时线程数时使用。
    void setThreadCount(int threadCount);

    /// 用最新实时状态刷新各线程行，逐行差分发 dataChanged。
    void update(downloader::DownloadState const* state);

signals:
    void activeChanged();

private:
    struct ThreadRow {
        QString status;
        double  progress{0.0};
        int64_t downloaded{0};
        int64_t sizeTotal{0};
        int64_t rangeStart{0};
        int64_t rangeEnd{0};
        int     attempts{0};
        double  speed{0.0}; // 该线程最近一次采样的速率（B/s）
    };

    int                    threadCount_{0};
    bool                   active_{false};
    std::vector<ThreadRow> rows_;

    // 每线程速率采样（按线程槽位下标，不随行增减移动）
    std::array<int64_t, kMaxWorkerSlots> prevBytes_{};
    std::array<double, kMaxWorkerSlots>  slotSpeed_{};
    qint64                               lastSampleMs_{0};
};

/**
 * @brief Live detail snapshot for a single task.
 *
 * Owned by QmlTaskListModel and returned via getTaskDetail(). QmlTaskListModel
 * refreshes all live detail objects on its 1-second timer; the Q_PROPERTY NOTIFY
 * signals cause QML to re-read.
 */
class QmlTaskDetail : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by QmlTaskListModel")

    Q_PROPERTY(int taskId READ taskId CONSTANT)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fileSizeStr READ fileSizeStr NOTIFY fileInfoChanged)
    Q_PROPERTY(QString urlStr READ urlStr NOTIFY fileInfoChanged)
    Q_PROPERTY(QString saveDir READ saveDir NOTIFY fileInfoChanged)
    Q_PROPERTY(QString referrer READ referrer NOTIFY fileInfoChanged)
    Q_PROPERTY(QString firstTryStr READ firstTryStr NOTIFY fileInfoChanged)
    Q_PROPERTY(QString lastTryStr READ lastTryStr NOTIFY fileInfoChanged)
    Q_PROPERTY(QString categoryStr READ categoryStr NOTIFY fileInfoChanged)
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY fileInfoChanged)
    Q_PROPERTY(QString errorMsg READ errorMsg NOTIFY fileInfoChanged)
    Q_PROPERTY(QString statusStr READ statusStr NOTIFY liveStateChanged)
    Q_PROPERTY(QString speedStr READ speedStr NOTIFY liveStateChanged)
    Q_PROPERTY(QString remainingTimeStr READ remainingTimeStr NOTIFY liveStateChanged)
    Q_PROPERTY(bool resumable READ resumable NOTIFY fileInfoChanged)
    Q_PROPERTY(QmlRangeModel* rangesModel READ rangesModel CONSTANT)

public:
    explicit QmlTaskDetail(QObject* parent = nullptr);
    ~QmlTaskDetail() override = default;

    int taskId() const { return taskId_; }
    void setTaskId(int id);

    QString fileName() const { return fileName_; }
    QString fileSizeStr() const { return fileSizeStr_; }
    QString urlStr() const { return urlStr_; }
    QString saveDir() const { return saveDir_; }
    QString referrer() const { return referrer_; }
    QString firstTryStr() const { return firstTryStr_; }
    QString lastTryStr() const { return lastTryStr_; }
    QString categoryStr() const { return categoryStr_; }
    QString mimeType() const { return mimeType_; }
    QString errorMsg() const { return errorMsg_; }
    bool resumable() const { return resumable_; }

    QString statusStr() const { return statusStr_; }
    QString speedStr() const { return speedStr_; }
    QString remainingTimeStr() const { return remainingTimeStr_; }

    QmlRangeModel* rangesModel() const { return rangesModel_; }

    /// Populate the static (file-info) fields from a TaskModel. Called when the
    /// detail is first created and whenever a task is reloaded.
    void setFileInfo(int taskId,
                     QString const& fileName,
                     QString const& fileSizeStr,
                     QString const& urlStr,
                     QString const& saveDir,
                     QString const& referrer,
                     QString const& firstTryStr,
                     QString const& lastTryStr,
                     QString const& categoryStr,
                     QString const& mimeType,
                     QString const& errorMsg,
                     bool resumable);

    /// Pull the live (state-dependent) fields from a DownloadState.
    /// `state` may be null (if the task is no longer running); in that case
    /// the live fields are zeroed and the status falls back to `fallbackState`
    /// （数据库里持久化的状态），否则"继续/暂停/打开文件"等菜单项会因为
    /// 状态串为空而被错误地全部禁用。
    void setLiveState(downloader::DownloadState const* state, TaskState fallbackState);

    /// 设置 rangesModel 的占位行数（= 下载线程数）。
    void setThreadCount(int threadCount);

    void setErrorMsg(QString const& msg);

signals:
    void fileInfoChanged();
    void liveStateChanged();

private:
    int taskId_{0};

    QString fileName_;
    QString fileSizeStr_;
    QString urlStr_;
    QString saveDir_;
    QString referrer_;
    QString firstTryStr_;
    QString lastTryStr_;
    QString categoryStr_;
    QString mimeType_;
    QString errorMsg_;
    bool resumable_{false};

    QString statusStr_;
    QString speedStr_;
    QString remainingTimeStr_;

    QmlRangeModel* rangesModel_{nullptr};
};

} // namespace edm
