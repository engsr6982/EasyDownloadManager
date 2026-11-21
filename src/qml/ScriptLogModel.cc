#include "ScriptLogModel.h"

#include <QString>

namespace edm {

namespace {
constexpr int kTimeRole    = Qt::UserRole + 1;
constexpr int kLevelRole   = Qt::UserRole + 2;
constexpr int kMessageRole = Qt::UserRole + 3;
} // namespace

ScriptLogModel::ScriptLogModel(std::shared_ptr<script::ScriptConsole> console, QObject* parent)
: QAbstractListModel(parent),
  console_(std::move(console)) {
    if (console_) {
        knownCount_ = static_cast<int>(console_->entries().size());
        console_->setChangeCallback([this] { onConsoleChanged(); });
    }
}

ScriptLogModel::~ScriptLogModel() {
    if (console_) console_->setChangeCallback(nullptr);
}

int ScriptLogModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid() || !console_) return 0;
    return static_cast<int>(console_->entries().size());
}

QVariant ScriptLogModel::data(QModelIndex const& index, int role) const {
    if (!console_) return {};
    auto const& entries = console_->entries();
    if (!index.isValid() || index.row() >= static_cast<int>(entries.size())) return {};
    auto const& entry = entries[static_cast<std::size_t>(index.row())];
    switch (role) {
    case kTimeRole:
        return QString::fromStdString(entry.time);
    case kLevelRole:
        return entry.level;
    case kMessageRole:
        return QString::fromStdString(entry.message);
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptLogModel::roleNames() const {
    return {
        {   kTimeRole,    "time"},
        {  kLevelRole,   "level"},
        {kMessageRole, "message"},
    };
}

void ScriptLogModel::onConsoleChanged() {
    if (!console_) return;
    int const newCount = static_cast<int>(console_->entries().size());
    if (newCount > knownCount_ && knownCount_ > 0) {
        beginInsertRows({}, knownCount_, newCount - 1);
        knownCount_ = newCount;
        endInsertRows();
    } else {
        // 清空或缓冲满后的裁剪：整表刷新
        beginResetModel();
        knownCount_ = newCount;
        endResetModel();
    }
}

void ScriptLogModel::clear() {
    if (console_) console_->clear();
}

} // namespace edm
