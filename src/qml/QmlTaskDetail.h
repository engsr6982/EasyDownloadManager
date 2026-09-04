#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace edm {

namespace downloader {
class DownloadState;
struct DownloadRange;
} // namespace downloader

/**
 * @brief Live per-range status for one task, exposed to QML as a list model.
 *
 * Roles match what TaskDetailDrawer's thread list needs.
 */
class QmlRangeModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by QmlTaskDetail")

public:
    enum Roles {
        RangeIndexRole = Qt::UserRole + 1,
        RangeStartRole,
        RangeEndRole,
        RangeDownloadedRole,
        RangeSizeRole,
        RangeProgressRole,
        RangeStatusRole,
        RangeAttemptsRole,
    };

    explicit QmlRangeModel(QObject* parent = nullptr);

    int rowCount(QModelIndex const& parent = {}) const override;
    QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(std::vector<std::shared_ptr<downloader::DownloadRange>> const& ranges);

private:
    std::vector<std::shared_ptr<downloader::DownloadRange>> ranges_;
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
    /// the live fields are zeroed and we keep the last status string.
    void setLiveState(downloader::DownloadState const* state);

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
