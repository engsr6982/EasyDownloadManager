#include "QmlScriptManager.h"

#include <QDesktopServices>
#include <QFile>
#include <QUrl>

#include <algorithm>
#include <unordered_map>

namespace edm {

namespace {

/// 由单条发现记录推导展示态；enabled 且尚未加载完成的先显示为 Enabled
QmlScriptManager::ScriptState resolveState(script::DiscoveredScript const& info) {
    if (info.error) return QmlScriptManager::Error;
    if (!info.loaded && info.enabled && !info.errorText.empty()) return QmlScriptManager::Error;
    return (info.loaded || info.enabled) ? QmlScriptManager::Enabled : QmlScriptManager::Disabled;
}

} // namespace

QmlScriptManager::QmlScriptManager(QObject* parent) : QAbstractListModel(parent) {}

QmlScriptManager::~QmlScriptManager() {
    if (bootstrap_) {
        bootstrap_->setScriptsChangedHandler(nullptr);
    }
}

void QmlScriptManager::setBootstrap(script::ScriptingBootstrap* bootstrap) {
    if (bootstrap_ == bootstrap) return;
    if (bootstrap_) {
        bootstrap_->setScriptsChangedHandler(nullptr);
    }
    bootstrap_ = bootstrap;
    if (bootstrap_) {
        bootstrap_->setScriptsChangedHandler([this] { onScriptsChanged(); });
        refresh();
    }
}

int QmlScriptManager::rowCount(QModelIndex const& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant QmlScriptManager::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(rows_.size())) return {};
    auto const& row = rows_[static_cast<size_t>(index.row())];
    auto const& m   = row.info.manifest;
    switch (role) {
    case NameRole:
        return QString::fromStdString(m.name);
    case VersionRole:
        return QString::fromStdString(m.version);
    case AuthorRole:
        return m.author ? QString::fromStdString(*m.author) : QString();
    case DescriptionRole:
        return m.description ? QString::fromStdString(*m.description) : QString();
    case HomepageRole:
        return m.homepage ? QString::fromStdString(*m.homepage) : QString();
    case StateRole:
        return row.state;
    case ErrorTextRole:
        return QString::fromStdString(row.info.errorText);
    case DirRole:
        return QString::fromStdString(row.info.dir);
    case LoadedRole:
        return row.info.loaded;
    case IconRole: {
        // 缺值返回空串；invalid QVariant 在 QML 侧为 undefined
        if (!m.icon) return QString();
        auto const iconPath =
            QString::fromStdString(row.info.dir) + QStringLiteral("/") + QString::fromStdString(*m.icon);
        if (!QFile::exists(iconPath)) return QString();
        return QUrl::fromLocalFile(iconPath).toString();
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlScriptManager::roleNames() const {
    return {
        {       NameRole,        "name"},
        {    VersionRole,     "version"},
        {     AuthorRole,      "author"},
        {DescriptionRole, "description"},
        {   HomepageRole,    "homepage"},
        {      StateRole,       "state"},
        {  ErrorTextRole,   "errorText"},
        {        DirRole,         "dir"},
        {     LoadedRole,      "loaded"},
        {       IconRole,        "icon"},
    };
}

void QmlScriptManager::refresh() { rebuildRows(); }

void QmlScriptManager::rebuildRows() {
    if (!bootstrap_) return;
    auto infos = bootstrap_->discoverScripts();

    // 脚本集合未变时原地更新（dataChanged）保持 delegate 存活；
    // 脚本数量或名称集合变化（导入/删除）时才整体 reset
    if (infos.size() == rows_.size()) {
        std::unordered_map<std::string_view, script::DiscoveredScript*> byName;
        byName.reserve(infos.size());
        for (auto& info : infos) byName.emplace(info.manifest.name, &info);

        bool allMatched = std::all_of(rows_.begin(), rows_.end(), [&](Row const& row) {
            return byName.contains(row.info.manifest.name);
        });
        if (allMatched) {
            for (size_t i = 0; i < rows_.size(); ++i) {
                auto& row = rows_[i];
                row.info  = *byName[row.info.manifest.name];
                row.state = resolveState(row.info);
                emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0));
            }
            return;
        }
    }

    beginResetModel();
    rows_.clear();
    for (auto& info : infos) {
        Row row;
        row.state = resolveState(info);
        row.info  = std::move(info);
        rows_.push_back(std::move(row));
    }
    endResetModel();
    emit countChanged();
}

int QmlScriptManager::findRow(QString const& name) const {
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (QString::fromStdString(rows_[i].info.manifest.name) == name) return static_cast<int>(i);
    }
    return -1;
}

bool QmlScriptManager::setEnabled(QString const& name, bool enabled) {
    if (!bootstrap_) return false;
    auto const ok = bootstrap_->setEnabled(name.toStdString(), enabled);
    if (!ok) {
        emit scriptActionError(
            enabled ? QStringLiteral("启用") : QStringLiteral("禁用"),
            name,
            QStringLiteral("操作失败，详见应用日志")
        );
    }
    onScriptsChanged();
    return ok;
}

bool QmlScriptManager::reloadScript(QString const& name) {
    if (!bootstrap_) return false;
    auto const ok = bootstrap_->reloadScript(name.toStdString());
    if (!ok) {
        // 失败详情已写入该脚本的虚拟控制台
        emit scriptActionError(QStringLiteral("重载"), name, QStringLiteral("重载失败，请打开控制台查看详情"));
    }
    onScriptsChanged();
    return ok;
}

bool QmlScriptManager::importScript(QString const& sourceDir) {
    if (!bootstrap_) return false;
    auto const error = bootstrap_->importScript(sourceDir.toStdString());
    if (error) {
        emit scriptActionError(QStringLiteral("导入"), sourceDir, QString::fromStdString(*error));
        onScriptsChanged();
        return false;
    }
    onScriptsChanged();
    return true;
}

bool QmlScriptManager::uninstallScript(QString const& name) {
    if (!bootstrap_) return false;
    auto const error = bootstrap_->uninstallScript(name.toStdString());
    if (error) {
        emit scriptActionError(QStringLiteral("卸载"), name, QString::fromStdString(*error));
    }
    onScriptsChanged();
    return !error.has_value();
}

void QmlScriptManager::reloadAll() {
    if (!bootstrap_) return;
    bootstrap_->reloadAll();
    onScriptsChanged();
}

void QmlScriptManager::openScriptsDir() {
    if (!bootstrap_) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsDir()));
}

void QmlScriptManager::openScriptDir(QString const& name) {
    if (!bootstrap_) return;
    auto const row = findRow(name);
    if (row < 0) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(rows_[static_cast<size_t>(row)].info.dir)));
}

QAbstractListModel* QmlScriptManager::consoleModel(QString const& name) {
    if (!bootstrap_) return nullptr;
    auto it = consoleModels_.constFind(name);
    if (it != consoleModels_.constEnd()) return it.value();
    // 首次打开时创建模型（脚本未加载也给控制台弹窗一个数据源）
    auto* model = new ScriptLogModel(bootstrap_->ensureConsole(name.toStdString()), this);
    consoleModels_.insert(name, model);
    return model;
}

void QmlScriptManager::clearConsole(QString const& name) {
    if (!bootstrap_) return;
    bootstrap_->clearConsole(name.toStdString());
}

QString QmlScriptManager::scriptsDir() const {
    return bootstrap_ ? QString::fromStdString(bootstrap_->scriptsDir()) : QString();
}

void QmlScriptManager::onScriptsChanged() { rebuildRows(); }

} // namespace edm
