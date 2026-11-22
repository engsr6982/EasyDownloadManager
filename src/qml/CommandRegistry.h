#pragma once

#include <QAbstractListModel>
#include <QJSEngine>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <vector>

namespace edm {

/// 命令面板的注册表与过滤模型：内置命令与脚本命令统一登记，主线程访问。
/// 过滤为大小写不敏感的子序列匹配，标题命中优先于描述命中，按匹配分排序。
class CommandRegistry : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        DescriptionRole,
        SourceRole,
    };
    Q_ENUM(Roles)

    explicit CommandRegistry(QObject* parent);
    ~CommandRegistry() override;

    /// QML_SINGLETON 入口：返回应用侧实例（引擎接管所有权，随 QQmlApplicationEngine 销毁）
    static CommandRegistry* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    int                    rowCount(QModelIndex const& parent = {}) const override;
    QVariant               data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 注册表（宿主与脚本子系统调用）
    std::int64_t
    addCommand(QString const& source, QString const& title, QString const& description, std::function<void()> callback);
    void removeCommand(std::int64_t id);

    // QML
    [[nodiscard]] int count() const;
    Q_INVOKABLE void  setFilter(QString const& text);
    Q_INVOKABLE void  clearFilter();
    Q_INVOKABLE void  executeRow(int row);

signals:
    void countChanged();

private:
    struct Command {
        std::int64_t          id{0};
        QString               source;
        QString               title;
        QString               description;
        std::function<void()> callback;
    };
    struct Match {
        int score{0};
        int index{-1};
    };

    void rebuildMatches();

    std::vector<Command> commands_;
    std::vector<Match>   matches_;
    QString              filter_;
    std::int64_t         nextId_{1};
};

} // namespace edm
