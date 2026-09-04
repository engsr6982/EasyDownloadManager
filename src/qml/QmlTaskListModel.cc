#include "QmlTaskListModel.h"

#include "Dispatcher.h"
#include "EdmConfig.h"
#include "QmlTaskDetail.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "database/TaskModelConversion.h"
#include "downloader/DownloadTypes.h"
#include "event/EventBus.h"
#include "utils/TimeUtils.h"
#include "utils/Utils.h"

#include <QDateTime>
#include <QFile>
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

    // Listen to EventBus so newly created tasks are inserted.
    connect(EventBus::instance(), &EventBus::onTaskCreated, this, [this](std::shared_ptr<edm::TaskModel> const& model) {
        if (!model) return;
        int id = model->id;
        if (id <= kInvalidTaskID) return;
        if (idToRow_.find(id) != idToRow_.end()) {
            refreshRow(static_cast<int>(idToRow_[id]));
            return;
        }
        insertRowFromModel(model);
    });
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
            return computeProgress(0, m->fileSize);
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

void QmlTaskListModel::setDispatcher(Dispatcher* d) { dispatcher_ = d; }
void QmlTaskListModel::setDatabase(DownloadDatabase* db) { database_ = db; }

void QmlTaskListModel::reloadFromDatabase() {
    if (!database_) return;
    beginResetModel();
    rows_.clear();
    idToRow_.clear();

    totalCount_ = database_->getTaskCount();

    // 加载第一批任务（按 lastTry 倒序，最新在前）
    if (totalCount_ > 0) {
        auto batch = database_->getTasksRange(0, kBatchSize);
        for (auto& task : batch) {
            rows_.push_back(Row{std::move(task), nullptr});
            idToRow_[rows_.back().model->id] = rows_.size() - 1;
        }
        // 关联 Dispatcher 中的实时状态
        if (dispatcher_) {
            for (size_t i = 0; i < rows_.size(); ++i) {
                if (auto s = dispatcher_->getTaskState(rows_[i].model->id)) {
                    rows_[i].liveState = s;
                }
            }
        }
    }
    endResetModel();
    emit countChanged();
}

void QmlTaskListModel::loadNextBatch() {
    if (!database_) return;
    int const offset    = static_cast<int>(rows_.size());
    int const remaining = totalCount_ - offset;
    if (remaining <= 0) return;

    int const limit = std::min(kBatchSize, remaining);
    auto      batch = database_->getTasksRange(offset, limit);
    if (batch.empty()) return;

    int const firstRow = static_cast<int>(rows_.size());
    int const lastRow  = firstRow + static_cast<int>(batch.size()) - 1;
    beginInsertRows({}, firstRow, lastRow);
    for (auto& task : batch) {
        rows_.push_back(Row{std::move(task), nullptr});
        idToRow_[rows_.back().model->id] = rows_.size() - 1;
    }
    // 关联 Dispatcher 中的实时状态
    if (dispatcher_) {
        for (int i = firstRow; i <= lastRow; ++i) {
            if (auto s = dispatcher_->getTaskState(rows_[i].model->id)) {
                rows_[i].liveState = s;
            }
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
    if (dispatcher_) {
        if (auto s = dispatcher_->getTaskState(rows_.front().model->id)) {
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
        it->second->setFileInfo(
            rows_[row].model->id,
            QString::fromStdString(rows_[row].model->fileName),
            formatFileSize(rows_[row].model->fileSize),
            QString::fromStdString(rows_[row].model->url),
            QString::fromStdString(rows_[row].model->saveDir),
            QString::fromStdString(rows_[row].model->pageUrl),
            formatTimestamp(rows_[row].model->firstTry),
            formatTimestamp(rows_[row].model->lastTry),
            QString::fromStdString(std::string(magic_enum::enum_name(rows_[row].model->category))),
            QString::fromStdString(rows_[row].model->mimeType),
            QString::fromStdString(rows_[row].model->errorMsg),
            rows_[row].model->resumable == Resumable::Yes
        );
        it->second->setLiveState(rows_[row].liveState.get());
    }
}

// ---------- Refresh tick ----------

void QmlTaskListModel::onRefreshTick() {
    if (!dispatcher_) return;
    dispatcher_->updateAllSpeeds();
    for (size_t i = 0; i < rows_.size(); ++i) {
        int id = rows_[i].model->id;
        if (auto s = dispatcher_->getTaskState(id)) {
            rows_[i].liveState = s;
        }
        refreshRow(static_cast<int>(i));
    }
}

// ---------- Task actions ----------

bool QmlTaskListModel::createTask(QString const& url, QString const& saveDir, bool useProxy) {
    if (!dispatcher_) {
        emit createTaskError(url, QStringLiteral("Dispatcher not initialized"));
        return false;
    }
    if (url.trimmed().isEmpty()) {
        emit createTaskError(url, QStringLiteral("URL is empty"));
        return false;
    }

    auto& conf = EdmConfig::getInstance();

    auto model         = TaskModel::make();
    auto stdUrl        = url.toStdString();
    auto stdSaveDir    = saveDir.toStdString();
    model->url         = stdUrl;
    model->bandLimit   = conf.getBandwidthLimit();
    model->threadCount = conf.getThreadCount();
    model->retryCount  = kRetryCount;
    model->firstTry    = time_utils::nowSeconds();
    model->lastTry     = model->firstTry;
    model->userAgent   = conf.getUserAgent().toStdString();
    model->saveDir     = stdSaveDir;

    // 从 URL 路径推导占位文件名 (在 MetaInfoFetcher 获取真实文件名之前)
    model->fileName = utils::resolveFileName(stdUrl);

    // 持久化到数据库以获取有效 TaskId
    database_->insertTask(model);

    // 转换为引擎配置
    auto configure = toTaskContext(model);
    if (useProxy) {
        if (auto proxy = conf.getProxyConfig(); !proxy.isEnabled()) {
            configure->proxyUrl = proxy.toProxyUrl();
        }
    }

    auto created = dispatcher_->createTask(configure);
    if (!created) {
        QString msg = QString::fromStdString(created.error().message());
        emit    createTaskError(url, msg);
        return false;
    }

    // 将新任务插入 UI 模型（最新任务放最前面）
    // 注意：insertRowFromModel 接受 const&，不会移动 model，因此后续可以安全使用。
    insertRowFromModel(model);
    emit EventBus::instance() -> onTaskCreated(model);
    return true;
}

bool QmlTaskListModel::pauseTask(int taskId) {
    if (!dispatcher_) return false;
    auto r = dispatcher_->pauseTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("pause"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::resumeTask(int taskId) {
    if (!dispatcher_) return false;
    auto r = dispatcher_->resumeTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("resume"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::cancelTask(int taskId) {
    if (!dispatcher_) return false;
    auto r = dispatcher_->cancelTask(taskId);
    if (!r) {
        emit taskActionError(QStringLiteral("cancel"), taskId, QString::fromStdString(r.error().message()));
        return false;
    }
    return true;
}

bool QmlTaskListModel::deleteTask(int taskId, bool deleteFiles) {
    int row = findRow(taskId);
    if (row < 0) return false;

    // Best-effort cancel; ignore errors (task may be terminal).
    if (dispatcher_) dispatcher_->cancelTask(taskId);

    // 如果需要同时删除磁盘文件
    if (deleteFiles && row < static_cast<int>(rows_.size())) {
        auto const& m        = rows_[row].model;
        auto        basePath = QString::fromStdString(m->saveDir) + u'/' + QString::fromStdString(m->fileName);

        // 删除可能的最终文件、临时文件和元数据文件
        QString const filesToDelete[] = {
            basePath,                               // 最终文件
            basePath + QStringLiteral(".edm"),      // 临时下载文件
            basePath + QStringLiteral(".edm.meta"), // 进度元数据
        };
        for (auto const& f : filesToDelete) {
            QFile::remove(f);
        }
    }

    // 从数据库删除
    if (database_) {
        database_->deleteTask(taskId);
    }

    removeRow(row);
    return true;
}

QmlTaskDetail* QmlTaskListModel::getTaskDetail(int taskId) {
    int row = findRow(taskId);
    if (row < 0) return nullptr;

    auto it = details_.find(taskId);
    if (it != details_.end() && it->second) {
        return it->second.data();
    }

    auto        detail = new QmlTaskDetail(this);
    auto const& r      = rows_[row];
    detail->setFileInfo(
        r.model->id,
        QString::fromStdString(r.model->fileName),
        formatFileSize(r.model->fileSize),
        QString::fromStdString(r.model->url),
        QString::fromStdString(r.model->saveDir),
        QString::fromStdString(r.model->pageUrl),
        formatTimestamp(r.model->firstTry),
        formatTimestamp(r.model->lastTry),
        QString::fromStdString(std::string(magic_enum::enum_name(r.model->category))),
        QString::fromStdString(r.model->mimeType),
        QString::fromStdString(r.model->errorMsg),
        r.model->resumable == Resumable::Yes
    );
    detail->setLiveState(r.liveState.get());
    details_[taskId] = QPointer<QmlTaskDetail>(detail);
    return detail;
}

} // namespace edm
