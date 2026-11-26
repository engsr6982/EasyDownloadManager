#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <qtclasshelpermacros.h>

#include <memory>

class QWebSocket;

namespace edm::ws {

/**
 * @brief WebSocket 服务端传输层。
 *
 * 职责限于监听、会话生命周期与文本帧收发，不解析报文语义：鉴权、方法分发与
 * 事件推送都在 `WsBridge`。默认只绑定回环地址，可配置监听全部网卡供局域网访问。
 */
class WsServer : public QObject {
    Q_OBJECT

    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    explicit WsServer(QObject* parent = nullptr);
    ~WsServer() override;
    Q_DISABLE_COPY_MOVE(WsServer);

    /// allowLan 为 true 时监听全部网卡，否则只监听回环地址
    bool start(quint16 port, bool allowLan = false);
    void stop();

    [[nodiscard]] bool    isListening() const;
    [[nodiscard]] quint16 listeningPort() const;
    [[nodiscard]] QString lastError() const;

    /// 向单个会话发送文本帧，会话已断开或文本为空时丢弃
    void send(QWebSocket* session, QString const& text);
    void close(QWebSocket* session, QString const& reason);

signals:
    void listeningChanged(bool listening);
    void errorOccurred(QString const& message);
    void sessionOpened(QWebSocket* session);
    void sessionClosed(QWebSocket* session);
    void messageReceived(QWebSocket* session, QString const& text);
};

} // namespace edm::ws
