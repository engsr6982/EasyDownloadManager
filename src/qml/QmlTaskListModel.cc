#include "QmlTaskListModel.h"

#include "AppService.h"
#include "QmlTaskDetail.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "downloader/DownloadTypes.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"

#include <QDateTime>
#include <QString>

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace edm {

// ---------- Static helpers ----------

QString QmlTaskListModel::formatFileSize(FileSize size) { return QString::fromStdString(utils::FileSize2String(size)); }

QString QmlTaskListModel::formatSpeed(double bytesPerSec) {
    if (bytesPerSec <= 0.0) return QString{};
    return QString::fromStdString(utils::FileSize2String(static_cast<FileSize>(bytesPerSec))) + QStringLiteral("/s");
}

QString QmlTaskListModel::formatStatus(TaskState s) {
    auto name = magic_enum::enum_name(s);
    if (name.empty()) return QStringLiteral("N/A");
    return QString::fromStdString(std::string(name));
}

QString QmlTaskListModel::formatTimestamp(time_t t) {
    if (t <= 0) return QString{};
    return QString::fromStdString(time_utils::formatTime(time_utils::toClockTime(t)));
}

double QmlTaskListModel::computeProgress(int64_t downloaded, int64_t total) {
    if (total <= 0) return 0.0;
    double p = static_cast<double>(downloaded) / static_cast<double>(total);
    return std::clamp(p, 0.0, 1.0);
}

QString QmlTaskListModel::formatProgressBar(bool indeterminate, double progress, TaskState state) {
    Q_UNUSED(indeterminate);
    Q_UNUSED(progress);
    Q_UNUSED(state);
    return {};
}

// ---------- Construction ----------

QmlTaskListModel::QmlTaskListModel(QObject* parent) : QAbstractListModel(parent) {
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(1000);
    refreshTimer_->setSingleShot(false);
    connect(refreshTimer_, &QTimer::timeout, this, &QmlTaskListModel::onRefreshTick);
}

QmlTaskListModel::~QmlTaskListModel() { stopRefresh(); }

int QmlTaskListModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant QmlTaskListModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    auto const& row = rows_[index.row()];
    auto const& m   = row.model;
    if (!m) return {};

    auto const* state = row.liveState.get();

    switch (role) {
    case TaskIdRole:
        return m->id;
    case FileNameRole:
        return QString::fromStdString(m->fileName);
    case FileSizeStrRole:
        if (state) {
            int64_t total = state->totalBytes();
            if (total > 0) return formatFileSize(static_cast<FileSize>(total));
        }
        return formatFileSize(m->fileSize);
    case ProgressValueRole: {
        if (!state) {
            // 重启后没有实时状态：已完成的任务直接显示满进度，其余留空
            // （已下载字节数没有持久化，无法还原中间进度）
            return m->state == TaskState::Finished ? computeProgress(1, 1) : computeProgress(0, m->fileSize);
        }
        int64_t downloaded = state->downloadedBytes();
        int64_t total      = state->totalBytes();
        if (total <= 0) total = m->fileSize;
        return computeProgress(downloaded, total);
    }
    case SpeedStrRole:
        if (state && state->state() == TaskState::Running) {
            auto s = formatSpeed(state->speed());
            if (!s.isEmpty()) return s;
        }
        return QStringLiteral("N/A");
    case StatusStrRole:
        if (state) return formatStatus(state->state());
        return formatStatus(m->state);
    case UrlStrRole:
        return QString::fromStdString(m->url);
    case SaveDirRole:
        return QString::fromStdString(m->saveDir);
    case ReferrerRole:
        return QString::fromStdString(m->pageUrl);
    case TimestampRole:
        return formatTimestamp(m->lastTry ? m->lastTry : m->firstTry);
    case CategoryRole:
        return static_cast<int>(m->category);
    case FirstTryRole:
        return static_cast<qlonglong>(m->firstTry);
    case LastTryRole:
        return static_cast<qlonglong>(m->lastTry);
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlTaskListModel::roleNames() const {
    return {
        {       TaskIdRole,        "taskId"},
        {     FileNameRole,      "fileName"},
        {  FileSizeStrRole,   "fileSizeStr"},
        {ProgressValueRole, "progressValue"},
        {     SpeedStrRole,      "speedStr"},
        {    StatusStrRole,     "statusStr"},
        {       UrlStrRole,        "urlStr"},
        {      SaveDirRole,       "saveDir"},
        {     ReferrerRole,      "referrer"},
        {    TimestampRole,     "timestamp"},
        {     CategoryRole,      "category"},
        {     FirstTryRole,      "firstTry"},
        {      LastTryRole,       "lastTry"},
    };
}

// ---------- Wiring ----------

void QmlTaskListModel::setService(AppService* service) {
    service_ = service;
    if (!service_) return;

    connect(service, &AppService::taskCreated, this, &QmlTaskListModel::onServiceTaskCreated);
    connect(service, &AppService::taskRemoved, this, &QmlTaskListModel::onServiceTaskRemoved);
    connect(service, &AppService::taskStateChanged, this, &QmlTaskListModel::onServiceTaskStateChanged);
    connect(service, &AppService::taskError, this, &QmlTaskListModel::onServiceTaskError);
    connect(service, &AppService::taskFileNameChanged, this, &QmlTaskListModel::onServiceTaskFileNameChanged);
}

void QmlTaskListModel::reloadFromDatabase() {
    auto* db = service_ ? service_->database() : nullptr;
    if (!db) return;
    beginResetModel();
    rows_.clear();
    idToRow_.clear();

    totalCount_ = db->getTaskCount();

    // 加载第一批任务（按 lastTry 倒序，最新在前）
    if (totalCount_ > 0) {
        auto batch = db->getTasksRange(0, kBatchSize);
        for (auto& task : batch) {
            // 崩溃遗留的 Running 实际没有在下载，先归位为 Paused，避免 UI 谎报"下载中"
            normalizeCrashedTaskModel(task);
            rows_.push_back(Row{std::move(task), nullptr});
            idToRow_[rows_.back().model->id] = rows_.size() - 1;
        }
        // 关联服务中活跃任务的实时状态
        for (size_t i = 0; i < rows_.size(); ++i) {
            if (auto s = service_->taskState(rows_[i].model->id)) {
                rows_[i].liveState = s;
            }
        }
    }
    endResetModel();
    emit countChanged();
}

void QmlTaskListModel::loadNextBatch() {
    auto* db = service_ ? service_->database() : nullptr;
    if (!db) return;
    int const offset    = static_cast<int>(rows_.size());
    int const remaining = totalCount_ - offset;
    if (remaining <= 0) return;

    int const limit = std::min(kBatchSize, remaining);
    auto      batch = db->getTasksRange(offset, limit);
    if (batch.empty()) return;

    int const firstRow = static_cast<int>(rows_.size());
    int const lastRow  = firstRow + static_cast<int>(batch.size()) - 1;
    beginInsertRows({}, firstRow, lastRow);
    for (auto& task : batch) {
        normalizeCrashedTaskModel(task);
        rows_.push_back(Row{std::move(task), nullptr});
        idToRow_[rows_.back().model->id] = rows_.size() - 1;
    }
    // 关联服务中活跃任务的实时状态
    for (int i = firstRow; i <= lastRow; ++i) {
        if (auto s = service_->taskState(rows_[i].model->id)) {
            rows_[i].liveState = s;
        }
    }
    endInsertRows();
    emit countChanged();
}

bool QmlTaskListModel::canFetchMore(QModelIndex const& parent) const {
    if (parent.isValid()) return false;
    return static_cast<int>(rows_.size()) < totalCount_;
}

void QmlTaskListModel::fetchMore(QModelIndex const& parent) {
    if (parent.isValid()) return;
    loadNextBatch();
}

void QmlTaskListModel::startRefresh(int intervalMs) {
    refreshTimer_->setInterval(intervalMs);
    if (!refreshTimer_->isActive()) refreshTimer_->start();
}

void QmlTaskListModel::stopRefresh() {
    if (refreshTimer_->isActive()) refreshTimer_->stop();
}

// ---------- Row management ----------

int QmlTaskListModel::findRow(int taskId) const {
    auto it = idToRow_.find(taskId);
    if (it == idToRow_.end()) return -1;
    return static_cast<int>(it->second);
}

void QmlTaskListModel::insertRowFromModel(std::shared_ptr<TaskModel> const& model) {
    if (!model) return;
    // 新任务插入到列表最前面（最新操作的任务排最前）
    beginInsertRows({}, 0, 0);
    rows_.insert(rows_.begin(), Row{model, nullptr});
    // 重建 idToRow_ 映射（所有行索引 +1）
    idToRow_.clear();
    for (size_t i = 0; i < rows_.size(); ++i) {
        idToRow_[rows_[i].model->id] = i;
    }
    if (service_) {
        if (auto s = service_->taskState(rows_.front().model->id)) {
            rows_.front().liveState = s;
        }
    }
    // totalCount_ 也需要 +1，确保新任务后续分页加载时不会重复
    ++totalCount_;
    endInsertRows();
    emit countChanged();
}

void QmlTaskListModel::removeRow(int row) {
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    beginRemoveRows({}, row, row);
    int id = rows_[row].model->id;
    rows_.erase(rows_.begin() + row);
    --totalCount_;
    idToRow_.erase(id);
    // Rebuild idToRow_ mapping after erase
    idToRow_.clear();
    for (size_t i = 0; i < rows_.size(); ++i) {
        idToRow_[rows_[i].model->id] = i;
    }
    // Drop cached detail
    details_.erase(id);
    endRemoveRows();
    emit countChanged();
}

void QmlTaskListModel::refreshRow(int row) {
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    auto idx = index(row);
    emit dataChanged(idx, idx);
    // Also refresh detail if cached
    int  id = rows_[row].model->id;
    auto it = details_.find(id);
    if (it != details_.end() && it->second) {
        updateDetailFromRow(it->second.data(), row);
    }
}

void QmlTaskListModel::updateDetailFromRow(QmlTaskDetail* detail, int row) {
    if (!detail || row < 0 || row >= static_cast<int>(rows_.size())) return;
    auto const& m = rows_[row].model;
    detail->setFileInfo(
        m->id,
        QString::fromStdString(m->fileName),
        formatFileSize(m->fileSize),
        QString::fromStdString(m->url),
        QString::fromStdString(m->saveDir),
        QString::fromStdString(m->pageUrl),
        formatTimestamp(m->firstTry),
        formatTimestamp(m->lastTry),
        QString::fromStdString(std::string(magic_enum::enum_name(m->category))),
        QString::fromStdString(m->mimeType),
        QString::fromStdString(m->errorMsg),
        m->resumable == Resumable::Yes
    );
    detail->setThreadCount(m->threadCount);
    detail->setLiveState(rows_[row].liveState.get(), m->state);
}

// ---------- Refresh tick ----------

void QmlTaskListModel::onRefreshTick() {
    if (!service_) return;
    service_->updateSpeeds();
    for (size_t i = 0; i < rows_.size(); ++i) {
        int id = rows_[i].model->id;
        if (auto s = service_->taskState(id)) {
            rows_[i].liveState = s;
        }
        refreshRow(static_cast<int>(i));
    }
}

// ---------- Task actions ----------

bool QmlTaskListModel::createTask(QString const& url, QString const& saveDir, bool useProxy) {
    if (!service_) {
        emit createTaskError(url, QStringLiteral("Service is not initialized"));
        return false;
    }
    if (url.trimmed().isEmpty()) {
        emit createTaskError(url, QStringLiteral("url is empty"));
        return false;
    }

    // 业务逻辑在 AppService 中，成功经 taskCreated 信号插入行（见 onServiceTaskCreated）。
    auto result = service_->createTask(url, saveDir, useProxy);
    if (!result) {
        emit createTaskError(url, QString::fromStdString(result.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::pauseTask(int taskId) {
    if (!service_) return false;
    auto r = service_->pauseTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("pause"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::resumeTask(int taskId) {
    if (!service_) return false;
    auto r = service_->resumeTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("resume"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::cancelTask(int taskId) {
    if (!service_) return false;
    auto r = service_->cancelTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("cancel"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::redownloadTask(int taskId) {
    if (!service_) return false;
    auto r = service_->redownloadTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("redownload"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::deleteTask(int taskId, bool deleteFiles) {
    if (!service_) return false;

    // 取消、删除磁盘文件与数据库记录均在服务内完成；成功会触发
    // onServiceTaskRemoved 移除对应行。
    auto r = service_->deleteTask(taskId, deleteFiles);
    if (!r) {
        emit taskActionError(QStringLiteral("delete"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

QmlTaskDetail* QmlTaskListModel::getTaskDetail(int taskId) {
    int row = findRow(taskId);
    if (row < 0) return nullptr;

    auto        it     = details_.find(taskId);
    QmlTaskDetail* detail = nullptr;
    if (it != details_.end() && it->second) {
        detail = it->second.data();
    } else {
        detail               = new QmlTaskDetail(this);
        details_[taskId]     = QPointer<QmlTaskDetail>(detail);
    }
    // 打开时总是用当前行刷新，避免展示旧数据（新建任务后立刻打开的场景）
    updateDetailFromRow(detail, row);
    return detail;
}

// ---------- AppService 信号槽 ----------

void QmlTaskListModel::onServiceTaskCreated(std::shared_ptr<TaskModel> const& model) {
    if (!model) return;
    int id = model->id;
    if (id <= kInvalidTaskID) return;
    int row = findRow(id);
    if (row >= 0) {
        refreshRow(row);
        return;
    }
    insertRowFromModel(model);
}

void QmlTaskListModel::onServiceTaskRemoved(int taskId) {
    int row = findRow(taskId);
    if (row >= 0) removeRow(row);
}

void QmlTaskListModel::onServiceTaskStateChanged(int taskId) {
    int row = findRow(taskId);
    if (row < 0) return;
    // 状态变化时引擎往往同时刷新了元信息（文件大小/MIME/文件名），必须重读数据库行，
    // 否则详情抽屉会一直显示行缓存里的旧值（例如文件体积始终为 0 B）。
    if (!reloadRowFromService(taskId)) {
        removeRow(row); // 数据库已无该任务（如异步创建失败被清理）
    }
}

void QmlTaskListModel::normalizeCrashedTaskModel(std::shared_ptr<TaskModel> const& model) {
    // Running / Preparing 都是"上次退出时还在进行"的残留状态，实际并没有在下载
    if (!model || (model->state != TaskState::Running && model->state != TaskState::Preparing)) return;
    // 若调度器中仍存在该任务的实时状态，说明确实在下载，保持不变
    if (service_ && service_->taskState(model->id)) return;

    model->state = TaskState::Paused;
    auto* db = service_ ? service_->database() : nullptr;
    if (db) {
        // 与 DB 保持一致，避免下次启动再次判定为 Running
        try {
            db->insertTask(model);
        } catch (std::exception const& e) {
            qWarning() << "QmlTaskListModel: normalize crashed task" << model->id << "failed:" << e.what();
        }
    }
}

bool QmlTaskListModel::reloadRowFromService(int taskId) {
    auto* db = service_ ? service_->database() : nullptr;
    if (!db) return false;
    int row = findRow(taskId);
    if (row < 0) return false;
    auto model = db->getTaskById(taskId);
    if (!model) return false; // 数据库已无此行（例如创建失败被清理）
    rows_[row].model = std::move(model);
    if (auto s = service_->taskState(taskId)) {
        rows_[row].liveState = s;
    }
    refreshRow(row);
    return true;
}

void QmlTaskListModel::onServiceTaskError(int taskId, QString const& message) {
    // 引擎侧的异步失败（元信息探测失败、重试耗尽等）也要让用户看见，
    // 否则点了"继续/重新下载"没反应会无从判断。
    emit taskActionError(QStringLiteral("task"), taskId, message);

    // errorMsg 已由服务写入数据库，重读该行让 UI 展示最新错误；
    // 若数据库已无该任务（如异步创建失败被清理），则移除视图行
    if (!reloadRowFromService(taskId)) {
        int row = findRow(taskId);
        if (row >= 0) removeRow(row);
    }
}

void QmlTaskListModel::onServiceTaskFileNameChanged(int taskId) {
    reloadRowFromService(taskId);
}

} // namespace edm
