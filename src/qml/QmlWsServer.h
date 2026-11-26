#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qtclasshelpermacros.h>

namespace edm {

namespace ws {
class WsBridge;
} // namespace ws

/**
 * @brief WebSocket 服务器的 QML 桥接（只读运行状态 + 重启入口）。
 *
 * 配置项（启用/端口/局域网开关/令牌）经 `QmlSettings` 双向绑定，本对象只暴露
 * 实际监听状态，供设置页展示。配置变化后由 `EdmApplication` 调用 `applyConfig()`。
 */
class QmlWsServer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int port READ port NOTIFY portChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit QmlWsServer(QObject* parent = nullptr);
    ~QmlWsServer() override = default;

    void setBridge(ws::WsBridge* bridge);

    [[nodiscard]] bool    running() const;
    [[nodiscard]] int     port() const;
    [[nodiscard]] int     clientCount() const;
    [[nodiscard]] QString endpoint() const;
    [[nodiscard]] QString lastError() const;

    /// 按当前配置同步监听状态，可从 QML 手动触发重启
    Q_INVOKABLE void applyConfig();

signals:
    void runningChanged();
    void portChanged();
    void clientCountChanged();
    void endpointChanged();
    void lastErrorChanged();

private:
    ws::WsBridge* bridge_{nullptr};
};

} // namespace edm
