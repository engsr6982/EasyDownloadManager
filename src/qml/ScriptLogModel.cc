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

QString ScriptLogModel::logHtml() const {
    if (!console_) return {};
    auto const& entries = console_->entries();
    QString     out;
    out.reserve(256 * entries.size());
    for (auto const& entry : entries) {
        QString levelColor;
        QString msgColor;
        QString levelTag;
        switch (entry.level) {
        case static_cast<int>(script::LogLevel::Error):
            levelTag   = "error";
            levelColor = "#F14C4C";
            msgColor   = "#F14C4C";
            break;
        case static_cast<int>(script::LogLevel::Warn):
            levelTag   = "warn";
            levelColor = "#FFB900";
            msgColor   = "#FFB900";
            break;
        case static_cast<int>(script::LogLevel::Debug):
            levelTag   = "debug";
            levelColor = "#8C8C8C";
            msgColor   = "#8C8C8C";
            break;
        default:
            levelTag   = "info";
            levelColor = "#4DA3FF";
            msgColor   = "#E8E8E8";
            break;
        }
        out += QStringLiteral(
                   "<span style=\"color:#9A9A9A\">%1</span> <span style=\"color:%2\">[%3]</span>"
                   " <span style=\"color:%4\">%5</span><br>"
        )
                   .arg(
                       QString::fromStdString(entry.time).toHtmlEscaped(),
                       levelColor,
                       levelTag,
                       msgColor,
                       QString::fromStdString(entry.message).toHtmlEscaped()
                   );
    }
    if (out.endsWith("<br>")) out.chop(4);
    return out;
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
    emit contentsChanged();
}

void ScriptLogModel::clear() {
    if (console_) console_->clear();
}

} // namespace edm
