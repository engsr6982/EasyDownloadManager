#include "WsBridge.h"

#include "AppService.h"
#include "EdmConfig.h"
#include "Global.h"
#include "WsPacket.h"
#include "WsPackets.h"
#include "WsServer.h"
#include "database/TaskModel.h"
#include "scripting/api/ScriptHostServices.h"

#include <QSet>
#include <QTimer>
#include <QWebSocket>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace edm::ws {

namespace {

/// 未鉴权会话的存活上限，超时未通过鉴权即断开
constexpr int kAuthTimeoutMs = 10000;

} // namespace

struct WsBridge::Impl {
    /// 类型擦除的路由：create 构造具体 packet，handle 产出完整响应文本
    struct Route {
        std::function<std::unique_ptr<IPacket>()> create;
        std::function<std::string(IPacket&)>      handle;
    };

    WsBridge*   owner{nullptr};
    AppService* service{nullptr};
    WsServer    server;

    QString                                token_;
    QSet<QWebSocket*>                      authed_;
    std::unordered_map<std::string, Route> routes_;

    explicit Impl(WsBridge* owner) : owner(owner), server(owner) {}

    [[nodiscard]] int clientCount() const { return static_cast<int>(authed_.size()); }

    void publishClientCount() const { emit owner->clientCountChanged(clientCount()); }

    void send(QWebSocket* session, std::string const& text) { server.send(session, QString::fromStdString(text)); }

    void broadcast(std::string const& text) {
        auto const message = QString::fromStdString(text);
        for (auto* session : std::as_const(authed_)) {
            server.send(session, message);
        }
    }

    // ---- 分发注册 ----

    /// 注册请求路由：create 构造具体实例，handle 在 kMethod 确定的类型上分派
    template <typename RequestT>
    void addHandler(std::function<Expected<typename RequestT::Result>(RequestT const&)> fn) {
        auto create = [] { return std::unique_ptr<IPacket>{std::make_unique<RequestT>()}; };
        auto handle = [fn = std::move(fn)](IPacket& packet) -> std::string {
            auto& req    = static_cast<RequestT&>(packet);
            auto  result = fn(std::as_const(req));
            if (!result) return req.serializeError(result.error().message());
            return req.serialize(*result);
        };
        routes_.emplace(std::string{RequestT::kMethod}, Route{std::move(create), std::move(handle)});
    }

    void registerHandlers() {
        addHandler<PingPacket>([](PingPacket const&) -> Expected<PingPacket::Result> {
            return PingPacket::Result{true};
        });

        addHandler<CreateTaskPacket>([this](CreateTaskPacket const& req) -> Expected<CreateTaskPacket::Result> {
            return onCreateTask(req);
        });
        addHandler<ListTasksPacket>([this](ListTasksPacket const&) -> Expected<ListTasksPacket::Result> {
            return onListTasks();
        });
        addHandler<GetTaskPacket>([this](GetTaskPacket const& req) -> Expected<GetTaskPacket::Result> {
            return onGetTask(req);
        });
        addHandler<PauseTaskPacket>([this](PauseTaskPacket const& req) -> Expected<PauseTaskPacket::Result> {
            return onTaskAction<PauseTaskPacket>(req, script::TaskAction::Pause);
        });
        addHandler<ResumeTaskPacket>([this](ResumeTaskPacket const& req) -> Expected<ResumeTaskPacket::Result> {
            return onTaskAction<ResumeTaskPacket>(req, script::TaskAction::Resume);
        });
        addHandler<CancelTaskPacket>([this](CancelTaskPacket const& req) -> Expected<CancelTaskPacket::Result> {
            return onTaskAction<CancelTaskPacket>(req, script::TaskAction::Cancel);
        });
        addHandler<DeleteTaskPacket>([this](DeleteTaskPacket const& req) -> Expected<DeleteTaskPacket::Result> {
            if (auto r = service->deleteTask(req.taskId, req.deleteFiles); !r) {
                return makeStringError(r.error().message());
            }
            return DeleteTaskPacket::Result{req.taskId};
        });
        addHandler<RedownloadTaskPacket>(
            [this](RedownloadTaskPacket const& req) -> Expected<RedownloadTaskPacket::Result> {
                if (auto r = service->redownloadTask(req.taskId); !r) return makeStringError(r.error().message());
                return RedownloadTaskPacket::Result{req.taskId};
            }
        );
        addHandler<PauseAllPacket>([this](PauseAllPacket const&) -> Expected<PauseAllPacket::Result> {
            return PauseAllPacket::Result{service->pauseAllTasks()};
        });
        addHandler<ResumeAllPacket>([this](ResumeAllPacket const&) -> Expected<ResumeAllPacket::Result> {
            return ResumeAllPacket::Result{service->resumePausedTasks()};
        });
    }

    // ---- 业务 handler ----

    Expected<CreateTaskPacket::Result> onCreateTask(CreateTaskPacket const& req) const {
        auto& config = EdmConfig::getInstance();
        if (req.url.empty()) return makeStringError("url 不能为空");

        auto const saveDir  = req.saveDir.value_or(config.getSaveDir().toStdString());
        auto const useProxy = req.useProxy.value_or(config.getProxyEnabled());

        // 任一接力字段出现即视为浏览器集成建单，否则维持纯 URL 行为
        std::optional<BrowserMeta> meta;
        if (req.referer || req.origin || req.cookie || req.fileName || req.mimeType || req.pageUrl || req.pageTitle
            || req.size || req.headers) {
            BrowserMeta bm;
            bm.referer   = req.referer.value_or("");
            bm.origin    = req.origin.value_or("");
            bm.cookie    = req.cookie.value_or("");
            bm.fileName  = req.fileName.value_or("");
            bm.mimeType  = req.mimeType.value_or("");
            bm.pageUrl   = req.pageUrl.value_or("");
            bm.pageTitle = req.pageTitle.value_or("");
            bm.size      = req.size.value_or(-1);
            if (req.headers) bm.headers = *req.headers;
            meta = std::move(bm);
        }

        auto created =
            service->createTask(QString::fromStdString(req.url), QString::fromStdString(saveDir), useProxy, meta);
        if (!created) return makeStringError(created.error().message());
        return CreateTaskPacket::Result{(*created)->id};
    }

    Expected<ListTasksPacket::Result> onListTasks() const { return ListTasksPacket::Result{service->taskSnapshots()}; }

    Expected<GetTaskPacket::Result> onGetTask(GetTaskPacket const& req) const {
        auto snap = service->taskSnapshot(req.taskId);
        if (!snap) return makeStringError("任务不存在: " + std::to_string(req.taskId));
        return GetTaskPacket::Result{*snap};
    }

    template <typename RequestT>
    Expected<typename RequestT::Result> onTaskAction(RequestT const& req, script::TaskAction action) const {
        if (!service->taskAction(req.taskId, action)) {
            return makeStringError("任务操作被拒绝: " + std::to_string(req.taskId));
        }
        return typename RequestT::Result{req.taskId};
    }

    // ---- 事件推送 ----

    template <typename EventT>
    void broadcastEvent(typename EventT::PayloadType const& data) {
        broadcast(serializeEvent<EventT>(data));
    }

    void broadcastTaskUpdated(int taskId) {
        if (!service) return;
        if (auto snap = service->taskSnapshot(taskId)) {
            broadcastEvent<TaskUpdatedPacket>(*snap);
        }
    }

    void connectServiceSignals() {
        QObject::connect(service, &AppService::taskCreated, owner, [this](std::shared_ptr<TaskModel> const& model) {
            if (!model) return;
            if (auto snap = service->makeSnapshot(*model)) {
                broadcastEvent<TaskCreatedPacket>(*snap);
            }
        });
        QObject::connect(service, &AppService::taskStateChanged, owner, [this](int taskId) {
            broadcastTaskUpdated(taskId);
        });
        QObject::connect(service, &AppService::taskFileNameChanged, owner, [this](int taskId) {
            broadcastTaskUpdated(taskId);
        });
        QObject::connect(service, &AppService::taskRemoved, owner, [this](int taskId) {
            broadcastEvent<TaskRemovedPacket>(TaskIdPayload{taskId});
        });
        QObject::connect(service, &AppService::taskError, owner, [this](int taskId, QString const& message) {
            broadcastEvent<TaskErrorPacket>(TaskErrorPayload{taskId, message.toStdString()});
        });
        QObject::connect(
            service,
            &AppService::taskFinished,
            owner,
            [this](int taskId, int state, QString const& fileName, QString const& errorMsg) {
                broadcastEvent<TaskFinishedPacket>(TaskFinishedPayload{
                    taskId,
                    static_cast<TaskState>(state),
                    fileName.toStdString(),
                    errorMsg.toStdString(),
                });
            }
        );
    }

    // ---- 会话与鉴权 ----

    void handleSessionOpened(QWebSocket* session) {
        QTimer::singleShot(kAuthTimeoutMs, session, [this, session] {
            if (session->isValid() && !authed_.contains(session)) {
                server.close(session, QStringLiteral("auth timeout"));
            }
        });
    }

    void handleSessionClosed(QWebSocket* session) {
        if (authed_.remove(session) > 0) publishClientCount();
    }

    void handleMessage(QWebSocket* session, QString const& text) {
        auto const body = text.toStdString();

        PacketHeader header;
        if (auto const ec = glz::read<kPacketOpts>(header, body)) {
            send(session, serializeErrorEnvelope(0, "报文不是合法的协议消息"));
            return;
        }

        if (std::string_view{header.method} == AuthPacket::kMethod) {
            handleAuth(session, header, body);
            return;
        }
        if (!authed_.contains(session)) {
            send(session, serializeErrorEnvelope(header.id.value_or(0), "未鉴权"));
            return;
        }

        auto const it = routes_.find(header.method);
        if (it == routes_.end()) {
            send(session, serializeErrorEnvelope(header.id.value_or(0), "未知方法: " + header.method));
            return;
        }

        auto packet = it->second.create();
        packet->setId(header.id.value_or(0));
        if (auto parsed = packet->deserialize(body); !parsed) {
            send(session, packet->serializeError(parsed.error().message()));
            return;
        }
        send(session, it->second.handle(*packet));
    }

    void handleAuth(QWebSocket* session, PacketHeader const& header, std::string const& body) {
        AuthPacket packet;
        packet.setId(header.id.value_or(0));
        if (auto parsed = packet.deserialize(body); !parsed) {
            send(session, packet.serializeError(parsed.error().message()));
            return;
        }
        // 令牌为空表示不校验
        if (!token_.isEmpty() && QString::fromStdString(packet.token) != token_) {
            send(session, packet.serializeError("令牌无效"));
            server.close(session, QStringLiteral("invalid token"));
            return;
        }
        if (!authed_.contains(session)) {
            authed_.insert(session);
            publishClientCount();
        }
        send(session, packet.serialize(AuthPacket::Result{true}));
    }

    /// 令牌轮换后旧会话的鉴权凭据失效，全部断开
    void setToken(QString token) {
        if (token_ == token) return;
        token_ = std::move(token);

        auto const sessions = std::exchange(authed_, {});
        for (auto* session : sessions) {
            server.close(session, QStringLiteral("access token rotated"));
        }
        if (!sessions.isEmpty()) publishClientCount();
    }
};

WsBridge::WsBridge(QObject* parent) : QObject(parent), impl(std::make_unique<Impl>(this)) {
    connect(&impl->server, &WsServer::sessionOpened, this, [this](QWebSocket* s) { impl->handleSessionOpened(s); });
    connect(&impl->server, &WsServer::sessionClosed, this, [this](QWebSocket* s) { impl->handleSessionClosed(s); });
    connect(&impl->server, &WsServer::messageReceived, this, [this](QWebSocket* s, QString const& text) {
        impl->handleMessage(s, text);
    });
}

WsBridge::~WsBridge() { shutdown(); }

void WsBridge::setService(AppService* service) {
    if (impl->service == service) return;
    impl->service = service;
    if (!impl->service) return;
    impl->registerHandlers();
    impl->connectServiceSignals();
}

WsServer* WsBridge::server() const { return const_cast<WsServer*>(&impl->server); }

int WsBridge::clientCount() const { return impl->clientCount(); }

void WsBridge::applyConfig() {
    auto& config = EdmConfig::getInstance();
    impl->setToken(config.getWsToken());

    if (!config.getWsEnabled()) {
        impl->server.stop();
        return;
    }
    impl->server.start(static_cast<quint16>(config.getWsPort()), config.getWsAllowLan());
}

void WsBridge::shutdown() { impl->server.stop(); }

} // namespace edm::ws
