#include "QmlTaskDetail.h"

#include "QmlTaskListModel.h"

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

/// WorkerState 到线程卡片状态文本的映射（渲染时才生成，模型内部只存数值）
QString workerStateText(int stateInt) {
    using downloader::WorkerState;
    switch (static_cast<WorkerState>(stateInt)) {
    case WorkerState::Requesting:
        return QStringLiteral("请求中");
    case WorkerState::Downloading:
        return QStringLiteral("接收中");
    case WorkerState::Retrying:
        return QStringLiteral("重试中");
    case WorkerState::Finished:
        return QStringLiteral("已完成");
    case WorkerState::Stopped:
        return QStringLiteral("已停止");
    default:
        return QStringLiteral("空闲");
    }
}
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
        return workerStateText(row.stateInt);
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
        if (row.speed <= 0.0) return QStringLiteral("N/A");
        return QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(row.speed))) + QStringLiteral("/s");
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlRangeModel::roleNames() const {
    return {
        {        RangeIndexRole,         "rangeIndex"},
        {       RangeStatusRole,        "rangeStatus"},
        {     RangeProgressRole,      "rangeProgress"},
        {   RangeDownloadedRole,    "rangeDownloaded"},
        {         RangeSizeRole,          "rangeSize"},
        {        RangeStartRole,         "rangeStart"},
        {          RangeEndRole,           "rangeEnd"},
        {     RangeAttemptsRole,      "rangeAttempts"},
        {RangeDownloadedStrRole, "rangeDownloadedStr"},
        {      RangeSizeStrRole,       "rangeSizeStr"},
        {     RangeSpeedStrRole,      "rangeSpeedStr"},
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
    // state 为空时无引擎槽位可读：占位行由 setThreadCount 维护，此处仅置模型为非活跃。
    if (state == nullptr) {
        if (active_) {
            active_ = false;
            emit activeChanged();
        }
        return;
    }

    // 线程槽位是定长原子数组，任何阶段读取都安全（worker 线程独占写各自槽位）。
    // 持久 worker 协议下 pause 为驻留不退出：暂停/失败/完成后的槽位依然有效，
    // 线程行照常显示真实状态（已停止/已完成）。
    // 分片解引用只在 ranges 结构稳定时进行：Pending/Preparing 阶段 start() 可能
    // 正在重建 ranges，此时线程行保留数值、分片列保持旧值。
    bool const rangesReadable =
        state != nullptr && state->state() != TaskState::Pending && state->state() != TaskState::Preparing;

    int const liveWorkers = state ? state->workerCount.load(std::memory_order_acquire) : 0;
    int const desired     = liveWorkers > 0 ? liveWorkers : std::max(threadCount_, 0);

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

    // 行数变化时才整体重置（行身份与引擎槽位一一对应，稳定不追加不重建）
    if (static_cast<int>(rows_.size()) != desired) {
        beginResetModel();
        rows_.assign(static_cast<size_t>(desired), ThreadRow{});
        prevBytes_.fill(0);
        slotSpeed_.fill(0.0);
        endResetModel();
        lastSampleMs_ = nowMs;
    }

    // 原地更新：引擎槽位原子量直接读入行结构（共享读取，无临时容器转换）
    std::vector<int> changedRows;
    for (int i = 0; i < desired; ++i) {
        auto const& slot = state->workers[static_cast<size_t>(i)];
        auto&       row  = rows_[static_cast<size_t>(i)];
        auto const  idx  = slot.rangeIndex.load(std::memory_order_relaxed);

        ThreadRow prev = row;
        row.stateInt   = slot.state.load(std::memory_order_relaxed);
        row.attempts   = slot.attempt.load(std::memory_order_relaxed);

        // 该线程速率：差分槽位累计字节
        auto const total = slot.downloaded.load(std::memory_order_relaxed);
        if (sample) {
            auto const delta                   = total - prevBytes_[static_cast<size_t>(i)];
            slotSpeed_[static_cast<size_t>(i)] = delta > 0 ? static_cast<double>(delta) / elapsed : 0.0;
        }
        // 基线无论是否采样都要推进（首次调用只建立基线，不产出速率）
        prevBytes_[static_cast<size_t>(i)] = total;
        row.speed                          = slotSpeed_[static_cast<size_t>(i)];

        // 该线程当前正在处理的分片（分片与线程不是固定绑定关系）
        if (rangesReadable && idx < state->ranges.size()) {
            if (auto const& range = state->ranges[idx]) {
                row.rangeStart = range->start;
                row.rangeEnd   = range->end;
                row.downloaded = range->downloaded.load(std::memory_order_relaxed);
                row.sizeTotal  = range->size();
                row.progress =
                    row.sizeTotal > 0
                        ? std::clamp(static_cast<double>(row.downloaded) / static_cast<double>(row.sizeTotal), 0.0, 1.0)
                        : 0.0;
            }
        }

        if (prev.stateInt != row.stateInt || prev.progress != row.progress || prev.downloaded != row.downloaded
            || prev.sizeTotal != row.sizeTotal || prev.rangeStart != row.rangeStart || prev.rangeEnd != row.rangeEnd
            || prev.attempts != row.attempts || prev.speed != row.speed) {
            changedRows.push_back(i);
        }
    }
    lastSampleMs_ = nowMs;

    // 变化行合并连续段发 dataChanged，保持 ListView 行复用与滚动位置
    if (!changedRows.empty()) {
        auto emitRange = [&](int first, int last) {
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
        };
        int first = changedRows.front();
        int prev  = first;
        for (size_t k = 1; k < changedRows.size(); ++k) {
            if (changedRows[k] != prev + 1) {
                emitRange(first, prev);
                first = changedRows[k];
            }
            prev = changedRows[k];
        }
        emitRange(first, prev);
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

void QmlTaskDetail::setFallbackStatus(TaskState state) {
    auto          name      = magic_enum::enum_name(state);
    QString const newStatus = name.empty() ? QString{} : QString::fromStdString(std::string(name));
    if (statusStr_ != newStatus) {
        statusStr_ = newStatus;
        emit liveStateChanged();
    }
}

void QmlTaskDetail::setLiveState(downloader::DownloadState const* state, TaskState fallbackState) {
    if (!state) {
        // 无实时状态（重启后未派发/已终态）：使用数据库里持久化的状态，
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
        setFallbackStatus(fallbackState);
        rangesModel_->update(state); // 无实时状态时线程列表置为非活跃（N/A 占位）
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

/// UI 拉取入口：抽屉可见时的定时器与本打开瞬间调用，仅刷新本任务
void QmlTaskDetail::pullLive() {
    if (auto* listModel = qobject_cast<QmlTaskListModel*>(parent())) {
        listModel->refreshDetailById(taskId_);
    }
}

void QmlTaskDetail::setErrorMsg(QString const& msg) {
    if (errorMsg_ != msg) {
        errorMsg_ = msg;
        emit fileInfoChanged();
    }
}

} // namespace edm
