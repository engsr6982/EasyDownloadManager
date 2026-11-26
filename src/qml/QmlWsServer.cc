#include "QmlWsServer.h"

#include "EdmConfig.h"
#include "ws/WsBridge.h"
#include "ws/WsServer.h"

#include <QAbstractSocket>
#include <QNetworkInterface>

namespace edm {

namespace {

/// 首个非回环 IPv4 地址，局域网模式下供展示
QString lanAddress() {
    for (auto const& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return QStringLiteral("127.0.0.1");
}

} // namespace

QmlWsServer::QmlWsServer(QObject* parent) : QObject(parent) {}

void QmlWsServer::setBridge(ws::WsBridge* bridge) {
    bridge_ = bridge;
    if (!bridge_) return;

    auto* server = bridge_->server();
    connect(server, &ws::WsServer::listeningChanged, this, [this] {
        emit runningChanged();
        emit portChanged();
        emit endpointChanged();
        emit lastErrorChanged();
    });
    connect(server, &ws::WsServer::errorOccurred, this, [this] { emit lastErrorChanged(); });
    connect(bridge_, &ws::WsBridge::clientCountChanged, this, [this] { emit clientCountChanged(); });
}

bool QmlWsServer::running() const { return bridge_ && bridge_->server()->isListening(); }

int QmlWsServer::port() const {
    if (running()) return bridge_->server()->listeningPort();
    return EdmConfig::getInstance().getWsPort();
}

int QmlWsServer::clientCount() const { return bridge_ ? bridge_->clientCount() : 0; }

QString QmlWsServer::endpoint() const {
    auto const host = EdmConfig::getInstance().getWsAllowLan() ? lanAddress() : QStringLiteral("127.0.0.1");
    return QStringLiteral("ws://%1:%2").arg(host).arg(port());
}

QString QmlWsServer::lastError() const { return bridge_ ? bridge_->server()->lastError() : QString(); }

void QmlWsServer::applyConfig() {
    if (!bridge_) return;
    bridge_->applyConfig();
    emit runningChanged();
    emit portChanged();
    emit clientCountChanged();
    emit endpointChanged();
    emit lastErrorChanged();
}

} // namespace edm
