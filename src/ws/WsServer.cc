#include "WsServer.h"

#include <QHostAddress>
#include <QWebSocket>
#include <QWebSocketProtocol>
#include <QWebSocketServer>

#include <memory>
#include <utility>

namespace edm::ws {

namespace {

constexpr auto kServerName = "EasyDownloadManager";

} // namespace

struct WsServer::Impl {
    WsServer* owner{nullptr};

    std::unique_ptr<QWebSocketServer> server_;
    QList<QWebSocket*>                sessions_;
    QString                           lastError_;
    bool                              allowLan_{false};

    explicit Impl(WsServer* owner) : owner(owner) {}

    void setLastError(QString message) {
        lastError_ = std::move(message);
        emit owner->errorOccurred(lastError_);
    }

    void acceptPendingConnections() {
        while (auto* socket = server_->nextPendingConnection()) {
            sessions_.append(socket);

            QObject::connect(socket, &QWebSocket::textMessageReceived, owner, [this, socket](QString const& text) {
                emit owner->messageReceived(socket, text);
            });
            QObject::connect(socket, &QWebSocket::disconnected, owner, [this, socket] { dropSession(socket); });
            QObject::connect(socket, &QWebSocket::errorOccurred, owner, [this, socket](QAbstractSocket::SocketError) {
                dropSession(socket);
            });

            emit owner->sessionOpened(socket);
        }
    }

    /// disconnected 与 errorOccurred 可能先后触发，摘除操作幂等
    void dropSession(QWebSocket* socket) {
        if (!sessions_.removeOne(socket)) return;
        emit owner->sessionClosed(socket);
        socket->deleteLater();
    }

    void dropAllSessions() {
        auto const sessions = std::exchange(sessions_, {});
        for (auto* socket : sessions) {
            emit owner->sessionClosed(socket);
            socket->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("server stopped"));
            socket->deleteLater();
        }
    }
};

WsServer::WsServer(QObject* parent) : QObject(parent), impl(std::make_unique<Impl>(this)) {}

WsServer::~WsServer() { stop(); }

bool WsServer::start(quint16 port, bool allowLan) {
    if (isListening() && listeningPort() == port && impl->allowLan_ == allowLan) return true;
    stop();

    impl->server_ =
        std::make_unique<QWebSocketServer>(QString::fromLatin1(kServerName), QWebSocketServer::NonSecureMode, this);
    connect(impl->server_.get(), &QWebSocketServer::newConnection, this, [this] { impl->acceptPendingConnections(); });
    connect(impl->server_.get(), &QWebSocketServer::acceptError, this, [this](QAbstractSocket::SocketError) {
        impl->setLastError(impl->server_->errorString());
    });

    auto const address = allowLan ? QHostAddress::Any : QHostAddress::LocalHost;
    if (!impl->server_->listen(address, port)) {
        impl->setLastError(impl->server_->errorString());
        impl->server_.reset();
        return false;
    }

    impl->allowLan_ = allowLan;
    impl->lastError_.clear();
    emit listeningChanged(true);
    return true;
}

void WsServer::stop() {
    if (!impl->server_) return;

    impl->dropAllSessions();
    impl->server_->close();
    impl->server_.reset();
    emit listeningChanged(false);
}

bool    WsServer::isListening() const { return impl->server_ && impl->server_->isListening(); }
quint16 WsServer::listeningPort() const { return impl->server_ ? impl->server_->serverPort() : 0; }
QString WsServer::lastError() const { return impl->lastError_; }

void WsServer::send(QWebSocket* session, QString const& text) {
    if (!session || !session->isValid() || text.isEmpty()) return;
    session->sendTextMessage(text);
}

void WsServer::close(QWebSocket* session, QString const& reason) {
    if (!session) return;
    session->close(QWebSocketProtocol::CloseCodeNormal, reason);
}

} // namespace edm::ws
