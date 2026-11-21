#pragma once

#include "ScriptLogModel.h"
#include "scripting/engine/ScriptingBootstrap.h"

#include <QAbstractListModel>
#include <QHash>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

namespace edm {

/**
 * @brief ExtensionView（插件中心）的桥接单例。
 *
 * 以 QAbstractListModel 暴露扩展卡片数据（name/version/author/description/
 * state/errorText/dir），提供独立启停、独立重载、全局重载、打开目录与
 * 虚拟控制台（每脚本一个 ScriptLogModel 弹窗数据源）。
 */
class QmlScriptManager : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString scriptsDir READ scriptsDir CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        AuthorRole,
        DescriptionRole,
        HomepageRole,
        StateRole,
        ErrorTextRole,
        DirRole,
        LoadedRole,
        IconRole,
    };
    Q_ENUM(Roles)

    enum ScriptState {
        Disabled = 0, ///< 配置为禁用
        Enabled  = 1, ///< 已加载启用
        Error    = 2, ///< 启用但加载失败
    };
    Q_ENUM(ScriptState)

    explicit QmlScriptManager(QObject* parent = nullptr);
    ~QmlScriptManager() override;

    // ---- QAbstractListModel ----
    int                    rowCount(QModelIndex const& parent = {}) const override;
    QVariant               data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ---- Wiring（EdmApplication 调用） ----
    void setBootstrap(script::ScriptingBootstrap* bootstrap);

    // ---- Q_INVOKABLE ----
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setEnabled(QString const& name, bool enabled);
    Q_INVOKABLE bool reloadScript(QString const& name);
    Q_INVOKABLE void reloadAll();
    /// 从外部目录导入脚本包（FolderDialog 选择结果）
    Q_INVOKABLE bool importScript(QString const& sourceDir);
    Q_INVOKABLE void openScriptsDir();
    Q_INVOKABLE void openScriptDir(QString const& name);
    /// 虚拟控制台数据源（脚本卸载后仍返回历史日志模型）
    Q_INVOKABLE QAbstractListModel* consoleModel(QString const& name);
    Q_INVOKABLE void                clearConsole(QString const& name);

    [[nodiscard]] QString scriptsDir() const;

signals:
    void countChanged();
    /// 操作失败时上报（如 manifest 缺失、加载失败详情）
    void scriptActionError(QString const& action, QString const& name, QString const& message);

private slots:
    void onScriptsChanged();

private:
    struct Row {
        script::DiscoveredScript info;
        ScriptState              state{Disabled};
    };

    void rebuildRows();
    int  findRow(QString const& name) const;

    script::ScriptingBootstrap*     bootstrap_{nullptr};
    std::vector<Row>                rows_;
    QHash<QString, ScriptLogModel*> consoleModels_;
};

} // namespace edm
