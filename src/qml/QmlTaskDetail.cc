#include "QmlTaskDetail.h"

#include "Global.h"
#include "database/TaskModel.h"
#include "downloader/DownloadTypes.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"

#include <QDateTime>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <limits>
#include <string>

namespace edm {

// 与 DownloadState::kMaxWorkers 保持同步（改一处必须改另一处）
static_assert(
    QmlRangeModel::kMaxWorkerSlots == static_cast<int>(downloader::DownloadState::kMaxWorkers),
    "QmlRangeModel::kMaxWorkerSlots must match DownloadState::kMaxWorkers"
);

// ---------- QmlRangeModel ----------

QmlRangeModel::QmlRangeModel(QObject* parent) : QAbstractListModel(parent) {}

int QmlRangeModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant QmlRangeModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    auto const& row = rows_[index.row()];
    switch (role) {
    case RangeIndexRole:
        return index.row() + 1;
    case RangeStatusRole:
        return row.status;
    case RangeProgressRole:
        return row.progress;
    case RangeDownloadedRole:
        return static_cast<qlonglong>(row.downloaded);
    case RangeSizeRole:
        return static_cast<qlonglong>(row.sizeTotal);
    case RangeStartRole:
        return static_cast<qlonglong>(row.rangeStart);
    case RangeEndRole:
        return static_cast<qlonglong>(row.rangeEnd);
    case RangeAttemptsRole:
        return row.attempts;
    case RangeDownloadedStrRole:
        return QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(row.downloaded)));
    case RangeSizeStrRole:
        return QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(row.sizeTotal)));
    case RangeSpeedStrRole:
        if (row.speed <= 0.0) return QStringLiteral("—");
        return QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(row.speed))) + QStringLiteral("/s");
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlRangeModel::roleNames() const {
    return {
        {      RangeIndexRole,      "rangeIndex"},
        {     RangeStatusRole,     "rangeStatus"},
        {   RangeProgressRole,   "rangeProgress"},
        {RangeDownloadedRole, "rangeDownloaded"},
        {      RangeSizeRole,       "rangeSize"},
        {    RangeStartRole,     "rangeStart"},
        {      RangeEndRole,       "rangeEnd"},
        { RangeAttemptsRole,  "rangeAttempts"},
        {RangeDownloadedStrRole, "rangeDownloadedStr"},
        {RangeSizeStrRole,  "rangeSizeStr"},
        { RangeSpeedStrRole,  "rangeSpeedStr"},
    };
}

void QmlRangeModel::setThreadCount(int threadCount) {
    int const n = std::max(threadCount, 0);
    if (threadCount_ == n) return;
    threadCount_ = n;
    // 只影响占位行数：有实时线程数据时行数由引擎决定，这里不重置模型
    if (active_) return;
    beginResetModel();
    rows_.assign(static_cast<size_t>(n), ThreadRow{});
    endResetModel();
}

void QmlRangeModel::update(downloader::DownloadState const* state) {
    // Pending/Paused 阶段后台线程可能正在 start() 里重建 ranges，而本方法读取
    // state->ranges 时不持有引擎锁，此时不能去解引用分片；线程槽位本身是定长
    // 原子数组，任何阶段读取都安全。
    bool const rangesReadable =
        state != nullptr && state->state() != TaskState::Pending && state->state() != TaskState::Paused;

    int const liveWorkers =
        rangesReadable ? state->workerCount.load(std::memory_order_acquire) : 0;
    int const desired = liveWorkers > 0 ? liveWorkers : std::max(threadCount_, 0);

    bool const newActive = liveWorkers > 0;
    if (newActive != active_) {
        active_ = newActive;
        emit activeChanged();
        // 线程列表从无到有时重置速率采样基线，避免跨任务算出离谱的速率
        prevBytes_.fill(0);
        slotSpeed_.fill(0.0);
        lastSampleMs_ = 0;
    }

    // 每线程速率：用槽位的累计字节数按真实采样间隔求差分
    qint64 const nowMs   = QDateTime::currentMSecsSinceEpoch();
    double const elapsed = lastSampleMs_ > 0 ? static_cast<double>(nowMs - lastSampleMs_) / 1000.0 : 0.0;
    bool const   sample  = elapsed >= 0.05;

    std::vector<ThreadRow> next(static_cast<size_t>(desired));
    for (int i = 0; i < liveWorkers; ++i) {
        auto const& slot = state->workers[static_cast<size_t>(i)];
        auto&       row  = next[static_cast<size_t>(i)];
        auto const  ws   = static_cast<downloader::WorkerState>(slot.state.load(std::memory_order_relaxed));
        auto const  idx  = slot.rangeIndex.load(std::memory_order_relaxed);
        row.attempts     = slot.attempt.load(std::memory_order_relaxed);

        switch (ws) {
        case downloader::WorkerState::Requesting:
            row.status = QStringLiteral("请求中");
            break;
        case downloader::WorkerState::Downloading:
            row.status = QStringLiteral("接收中");
            break;
        case downloader::WorkerState::Retrying:
            row.status = QStringLiteral("重试中");
            break;
        case downloader::WorkerState::Finished:
            row.status = QStringLiteral("已完成");
            break;
        case downloader::WorkerState::Stopped:
            // 暂停/取消/失败导致的退出：不能显示成"已完成"
            row.status = QStringLiteral("已停止");
            break;
        case downloader::WorkerState::Idle:
        default:
            row.status = QStringLiteral("空闲");
            break;
        }

        // 该线程速率：差分槽位累计字节
        auto const total = slot.downloaded.load(std::memory_order_relaxed);
        if (sample) {
            auto const delta = total - prevBytes_[static_cast<size_t>(i)];
            slotSpeed_[static_cast<size_t>(i)] = delta > 0 ? static_cast<double>(delta) / elapsed : 0.0;
        }
        // 基线无论是否采样都要推进（首次调用只建立基线，不产出速率）
        prevBytes_[static_cast<size_t>(i)] = total;
        row.speed                         = slotSpeed_[static_cast<size_t>(i)];

        // 该线程当前正在处理的分片（分片与线程不是固定绑定关系）
        if (idx < state->ranges.size()) {
            auto const& range = state->ranges[idx];
            if (range) {
                row.rangeStart = range->start;
                row.rangeEnd   = range->end;
                row.downloaded = range->downloaded.load(std::memory_order_relaxed);
                row.sizeTotal  = range->size();
                row.progress   = row.sizeTotal > 0
                                   ? std::clamp(
                                         static_cast<double>(row.downloaded) / static_cast<double>(row.sizeTotal),
                                         0.0,
                                         1.0
                                     )
                                   : 0.0;
            }
        }
    }
    // 无条件推进采样时钟：否则第一次 elapsed 恒为 0，速率永远算不出来
    lastSampleMs_ = nowMs;

    if (static_cast<int>(rows_.size()) != desired) {
        beginResetModel();
        rows_ = std::move(next);
        endResetModel();
        return;
    }

    // 逐行比，只对变化行发 dataChanged，保持 ListView 行复用与滚动位置
    int first = -1, last = -1;
    for (int i = 0; i < desired; ++i) {
        auto& oldRow = rows_[i];
        auto& newRow = next[i];
        if (oldRow.status != newRow.status || oldRow.downloaded != newRow.downloaded
            || oldRow.sizeTotal != newRow.sizeTotal || oldRow.rangeStart != newRow.rangeStart
            || oldRow.rangeEnd != newRow.rangeEnd || oldRow.attempts != newRow.attempts
            || oldRow.speed != newRow.speed) {
            oldRow = std::move(newRow);
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first >= 0) {
        emit dataChanged(
            index(first),
            index(last),
            {RangeStatusRole,
             RangeProgressRole,
             RangeDownloadedRole,
             RangeSizeRole,
             RangeStartRole,
             RangeEndRole,
             RangeAttemptsRole,
             RangeDownloadedStrRole,
             RangeSizeStrRole,
             RangeSpeedStrRole}
        );
    }
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

void QmlTaskDetail::setLiveState(downloader::DownloadState const* state, TaskState fallbackState) {
    if (!state) {
        // 无实时状态（重启后未派发/已终态）：用数据库里持久化的状态兜底，
        // 否则 statusStr 为空，右键菜单会按"N/A"把操作项全部禁用。
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
        auto name = magic_enum::enum_name(fallbackState);
        auto newStatus = name.empty() ? QString{} : QString::fromStdString(std::string(name));
        if (statusStr_ != newStatus) {
            statusStr_ = newStatus;
            changed    = true;
        }
        rangesModel_->update(state); // 无实时状态 → 线程列表置为非活跃（N/A 占位）
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
    rangesModel_->update(state);

    bool liveChanged = (newStatus != statusStr_) || (newSpeed != speedStr_) || (newRemaining != remainingTimeStr_);

    statusStr_        = newStatus;
    speedStr_         = newSpeed;
    remainingTimeStr_ = newRemaining;

    if (liveChanged) emit liveStateChanged();
}

void QmlTaskDetail::setThreadCount(int threadCount) { rangesModel_->setThreadCount(threadCount); }

void QmlTaskDetail::setErrorMsg(QString const& msg) {
    if (errorMsg_ != msg) {
        errorMsg_ = msg;
        emit fileInfoChanged();
    }
}

} // namespace edm
