#include "QmlTaskDetail.h"

#include "Global.h"
#include "database/TaskModel.h"
#include "downloader/DownloadTypes.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"


#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <limits>
#include <string>

namespace edm {

// ---------- QmlRangeModel ----------

QmlRangeModel::QmlRangeModel(QObject* parent) : QAbstractListModel(parent) {}

int QmlRangeModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(ranges_.size());
}

QVariant QmlRangeModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(ranges_.size())) {
        return {};
    }
    auto const& r = ranges_[index.row()];
    if (!r) return {};
    switch (role) {
    case RangeIndexRole:
        return index.row() + 1;
    case RangeStartRole:
        return static_cast<qlonglong>(r->start);
    case RangeEndRole:
        return static_cast<qlonglong>(r->end);
    case RangeDownloadedRole:
        return static_cast<qlonglong>(r->downloaded.load(std::memory_order_relaxed));
    case RangeSizeRole:
        return static_cast<qlonglong>(r->size());
    case RangeProgressRole: {
        int64_t size = r->size();
        if (size <= 0) return 0.0;
        double progress =
            static_cast<double>(r->downloaded.load(std::memory_order_relaxed)) / static_cast<double>(size);
        return std::clamp(progress, 0.0, 1.0);
    }
    case RangeStatusRole: {
        if (r->completed.load(std::memory_order_relaxed)) {
            return QStringLiteral("已完成");
        }
        int64_t downloaded = r->downloaded.load(std::memory_order_relaxed);
        if (downloaded > 0) {
            return QStringLiteral("接收中...");
        }
        return QStringLiteral("等待中");
    }
    case RangeAttemptsRole:
        return r->attempts.load(std::memory_order_relaxed);
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlRangeModel::roleNames() const {
    return {
        {     RangeIndexRole,      "rangeIndex"},
        {     RangeStartRole,      "rangeStart"},
        {       RangeEndRole,        "rangeEnd"},
        {RangeDownloadedRole, "rangeDownloaded"},
        {      RangeSizeRole,       "rangeSize"},
        {  RangeProgressRole,   "rangeProgress"},
        {    RangeStatusRole,     "rangeStatus"},
        {  RangeAttemptsRole,   "rangeAttempts"},
    };
}

void QmlRangeModel::update(std::vector<std::shared_ptr<downloader::DownloadRange>> const& ranges) {
    beginResetModel();
    ranges_ = ranges;
    endResetModel();
}

// ---------- QmlTaskDetail ----------

QmlTaskDetail::QmlTaskDetail(QObject* parent) : QObject(parent), rangesModel_(new QmlRangeModel(this)) {}

void QmlTaskDetail::setTaskId(int id) { taskId_ = id; }

void QmlTaskDetail::setFileInfo(
    int            taskId,
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
    bool           resumable
) {
    taskId_      = taskId;
    fileName_    = fileName;
    fileSizeStr_ = fileSizeStr;
    urlStr_      = urlStr;
    saveDir_     = saveDir;
    referrer_    = referrer;
    firstTryStr_ = firstTryStr;
    lastTryStr_  = lastTryStr;
    categoryStr_ = categoryStr;
    mimeType_    = mimeType;
    errorMsg_    = errorMsg;
    resumable_   = resumable;
    emit fileInfoChanged();
}

void QmlTaskDetail::setLiveState(downloader::DownloadState const* state) {
    if (!state) {
        // No live state (e.g. task isn't running). Show placeholders, keep last status.
        bool       changed = false;
        auto const kNA     = QStringLiteral("N/A");
        auto const kDash   = QStringLiteral("---");
        if (speedStr_ != kNA) {
            speedStr_ = kNA;
            changed   = true;
        }
        if (remainingTimeStr_ != kDash) {
            remainingTimeStr_ = kDash;
            changed           = true;
        }
        if (changed) emit liveStateChanged();
        return;
    }

    auto    stateEnum = state->state();
    auto    name      = magic_enum::enum_name(stateEnum);
    QString newStatus = name.empty() ? QString{} : QString::fromStdString(std::string(name));

    QString newSpeed;
    if (stateEnum == TaskState::Running) {
        double spd = state->speed();
        if (spd > 0.0) {
            newSpeed =
                QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(spd))) + QStringLiteral("/s");
        }
    }
    if (newSpeed.isEmpty()) {
        newSpeed = QStringLiteral("N/A");
    }

    QString newRemaining;
    int64_t total      = state->totalBytes();
    int64_t downloaded = state->downloadedBytes();
    double  speed      = state->speed();
    if (stateEnum == TaskState::Running && total > 0 && speed > 0.0) {
        int64_t remaining = std::max<int64_t>(0, total - downloaded);
        int64_t etaSec    = static_cast<int64_t>(static_cast<double>(remaining) / speed);
        newRemaining      = QString::fromStdString(time_utils::formatDuration(static_cast<time_t>(etaSec)));
    }
    if (newRemaining.isEmpty()) {
        newRemaining = QStringLiteral("---");
    }

    // 同步线程分片状态到 QML
    rangesModel_->update(state->ranges);

    bool liveChanged = (newStatus != statusStr_) || (newSpeed != speedStr_) || (newRemaining != remainingTimeStr_);

    statusStr_        = newStatus;
    speedStr_         = newSpeed;
    remainingTimeStr_ = newRemaining;

    if (liveChanged) emit liveStateChanged();
}

void QmlTaskDetail::setErrorMsg(QString const& msg) {
    if (errorMsg_ != msg) {
        errorMsg_ = msg;
        emit fileInfoChanged();
    }
}

} // namespace edm
