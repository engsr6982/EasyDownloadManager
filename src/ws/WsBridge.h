#pragma once

#include <QObject>
#include <QString>
#include <qtclasshelpermacros.h>

#include <memory>

class QWebSocket;

namespace edm {

class AppService;

namespace ws {

class WsServer;

/**
 * @brief WebSocket 协议层：鉴权、packet 分发与事件推送。
 *
 * 报文契约集中在 `WsPackets.h`：分发层只依赖 `IPacket` 反序列化与序列化，
 * handler 直接成员访问类型化的 packet 并以 `Expected<Result>` 返回，全程不接触 JSON。
 * 任务域信号序列化为事件后广播给已鉴权会话。
 */
class WsBridge : public QObject {
    Q_OBJECT

    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    explicit WsBridge(QObject* parent = nullptr);
    ~WsBridge() override;
    Q_DISABLE_COPY_MOVE(WsBridge);

    void                    setService(AppService* service);
    [[nodiscard]] WsServer* server() const;
    [[nodiscard]] int       clientCount() const;

    /// 按 EdmConfig 当前值同步令牌与监听状态，配置变化后可重复调用
    void applyConfig();
    void shutdown();

signals:
    void clientCountChanged(int count);
};

} // namespace ws
} // namespace edm
