#include "EdmApplication.h"

#include "AppService.h"
#include "EdmConfig.h"
#include "FramelessShadowFilter.h"
#include "Global.h"
#include "qml/CommandRegistry.h"
#include "qml/QmlController.h"
#include "qml/QmlScriptManager.h"
#include "qml/QmlSettings.h"
#include "qml/QmlTaskListModel.h"
#include "qml/QmlWsServer.h"
#include "scripting/api/ScriptHostServices.h"
#include "scripting/engine/ScriptingBootstrap.h"
#include "scripting/qt/ScriptQtHost.h"
#include "utils/TaskLogger.h"
#include "ws/WsBridge.h"

#include <QApplication>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <QUrl>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

// qt_add_qml_module 生成的 C++ 类型注册入口（qmltyperegistrations.cpp，全局作用域）。
// 模块直挂在 exe 上时 Qt 不会自动调用它，必须在加载 QML 前手动注册，
// 否则 QML_ELEMENT 类型（ScriptLogModel 等）在 QML 中不可见。
void qml_register_types_edm_qml_ui();

namespace edm {


struct EdmApplication::Impl {
    std::unique_ptr<AppService>                 appService_{nullptr};
    std::unique_ptr<script::qt::QtHostServices> scriptServices_{nullptr};
    std::unique_ptr<script::ScriptingBootstrap> scripting_{nullptr};
    std::unique_ptr<ws::WsBridge>               wsBridge_{nullptr};
    std::unique_ptr<QQmlApplicationEngine>      qmlEngine_{nullptr};
    std::unique_ptr<QSystemTrayIcon>            trayIcon_{nullptr};

    // QML bridge objects (owned by the application)
    QmlTaskListModel* taskListModel_{nullptr};
    QmlSettings*      settingsModel_{nullptr};
    QmlController*    controller_{nullptr};
    QmlScriptManager* scriptManager_{nullptr};
    CommandRegistry*  commandRegistry_{nullptr};
    QmlWsServer*      wsServerModel_{nullptr};

    QQuickWindow* mainWindow_{nullptr};

    FramelessShadowFilter shadowFilter_;
    bool                  shadowFilterInstalled_{false};

    void initiailze() {
        // 0. 任务级日志：先于其余装配初始化，落盘到应用数据目录下的 logs/
        TaskLoggerManager::init(EdmConfig::getAppDataDir().toStdString() + "/logs");

        // 1. 应用服务：数据库 + 下载引擎 + 观察器接线
        appService_ = std::make_unique<AppService>();

        // 2. 脚本扩展子系统：组装宿主服务并接线（脚本加载放到 QML 就绪后，避免拖慢首帧）
        scripting_ = std::make_unique<script::ScriptingBootstrap>();
        appService_->setScripting(scripting_.get());

        // 3. WebSocket 服务器：浏览器集成的协调入口，按配置决定是否监听
        wsBridge_ = std::make_unique<ws::WsBridge>();
        wsBridge_->setService(appService_.get());
        wsBridge_->applyConfig();

        // 4. QML engine
        _initializeQml();

        // 5. 脚本子系统初始化：扫描脚本目录并加载启用脚本
        _initializeScripting();

        // 6. System tray icon
        _initSystemTrayIcon();
    }

    void _installWindowsShadow() {
#ifdef Q_OS_WIN
        if (!mainWindow_) return;

        // 确保主窗口具有原生 HWND。
        HWND hwnd = reinterpret_cast<HWND>(mainWindow_->winId());

        // 只安装一次；过滤器与 Impl 同生命周期。
        shadowFilter_.setWindow(hwnd);

        if (!shadowFilterInstalled_) {
            qApp->installNativeEventFilter(&shadowFilter_);
            shadowFilterInstalled_ = true;
        }

        // 保留 Qt 已有的窗口样式，只添加可提供原生框架的样式。
        //
        // 关键：Qt 在 FramelessWindowHint 下把窗口建成 WS_POPUP，而 DWM 只对
        // "普通顶层窗口"播放最小化/还原动画，弹窗式窗口是直接消失的。
        // 去掉 WS_POPUP、换成 WS_OVERLAPPEDWINDOW，外观不受影响。
        // 非客户区由 FramelessShadowFilter 的 WM_NCCALCSIZE 处理为 0，标题栏不会画出来。
        LONG_PTR style  = GetWindowLongPtr(hwnd, GWL_STYLE);
        style          &= ~static_cast<LONG_PTR>(WS_POPUP);
        style          |= WS_OVERLAPPEDWINDOW;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        // 扩展 DWM 框架到客户区。
        const MARGINS margins{1, 1, 1, 1};
        const HRESULT hr = DwmExtendFrameIntoClientArea(hwnd, &margins);

        if (FAILED(hr)) {
            qWarning() << "DwmExtendFrameIntoClientArea failed:" << Qt::hex << static_cast<quint32>(hr);
        }

        // 通知 Windows 重新计算窗口框架。
        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
        );
#endif
    }

    void _initializeQml() {
        qmlEngine_ = std::make_unique<QQmlApplicationEngine>();

        // Register singletons so QML can access them via their type names.
        taskListModel_ = new QmlTaskListModel();
        taskListModel_->setService(appService_.get());

        settingsModel_ = new QmlSettings();
        controller_    = new QmlController();
        scriptManager_ = new QmlScriptManager();
        scriptManager_->setBootstrap(scripting_.get());
        commandRegistry_ = new CommandRegistry(nullptr);
        wsServerModel_   = new QmlWsServer();
        wsServerModel_->setBridge(wsBridge_.get());

        _registerBuiltinCommands();

        // The objects are parented to the engine for cleanup.
        taskListModel_->setParent(qmlEngine_.get());
        settingsModel_->setParent(qmlEngine_.get());
        controller_->setParent(qmlEngine_.get());
        scriptManager_->setParent(qmlEngine_.get());
        wsServerModel_->setParent(qmlEngine_.get());

        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "TaskListModel", taskListModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "SettingsModel", settingsModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "AppController", controller_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "ScriptManager", scriptManager_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "CommandRegistry", commandRegistry_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "WsServerModel", wsServerModel_);

        // 配置项变化即同步监听状态；applyConfig 幂等，reload 的批量通知可安全重入
        QObject::connect(settingsModel_, &QmlSettings::wsEnabledChanged, wsServerModel_, &QmlWsServer::applyConfig);
        QObject::connect(settingsModel_, &QmlSettings::wsPortChanged, wsServerModel_, &QmlWsServer::applyConfig);
        QObject::connect(settingsModel_, &QmlSettings::wsAllowLanChanged, wsServerModel_, &QmlWsServer::applyConfig);
        QObject::connect(settingsModel_, &QmlSettings::wsTokenChanged, wsServerModel_, &QmlWsServer::applyConfig);

        qml_register_types_edm_qml_ui();


        QObject::connect(
            qmlEngine_.get(),
            &QQmlApplicationEngine::objectCreationFailed,
            QCoreApplication::instance(),
            [](QUrl const& url) { qCritical() << "[ERROR] Object creation failed: " << url; },
            Qt::QueuedConnection
        );
        QObject::connect(qmlEngine_.get(), &QQmlApplicationEngine::warnings, [](QList<QQmlError> const& warnings) {
            for (auto const& error : warnings) {
                qCritical() << "[WARN] " << error.toString();
            }
        });

        QUrl const url(QStringLiteral("qrc:/qt/qml/edm/qml/ui/src-ui/Src/App.qml"));
        qmlEngine_->load(url);

        // Get root window for tray icon interaction
        auto roots = qmlEngine_->rootObjects();
        if (!roots.isEmpty()) {
            mainWindow_ = qobject_cast<QQuickWindow*>(roots.first());
            if (mainWindow_) {
                mainWindow_->setIcon(QIcon(":/assets/icons/logo.ico"));
                _installWindowsShadow(); // HWND 创建后，初始化原生阴影。
            }
        }

        // 6. Load persisted tasks into the model
        taskListModel_->reloadFromDatabase();

        // 7. Start the live refresh timer
        taskListModel_->startRefresh(1000);
    }

    void _showMainWindow() const {
        if (!mainWindow_) return;

        mainWindow_->showNormal();
        mainWindow_->raise();
        mainWindow_->requestActivate();
    }

    /// 注册命令面板内置命令（脚本子系统接入前完成，保证面板打开时即可用）
    void _registerBuiltinCommands() {
        auto* commands = commandRegistry_;
        auto* service  = appService_.get();
        auto* ctrl     = controller_;
        auto* scripts  = scriptManager_;

        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("暂停所有任务"),
            QStringLiteral("暂停全部进行中的下载任务"),
            [service] { service->pauseAllTasks(); }
        );
        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("恢复所有暂停的任务"),
            QStringLiteral("继续全部处于暂停状态的下载任务"),
            [service] { service->resumePausedTasks(); }
        );
        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("新建任务"),
            QStringLiteral("打开新建任务对话框"),
            [ctrl] { emit ctrl->newTaskRequested(); }
        );
        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("打开脚本目录"),
            QStringLiteral("在文件管理器中打开脚本扩展目录"),
            [scripts] { scripts->openScriptsDir(); }
        );
        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("全部重载脚本"),
            QStringLiteral("重新加载全部脚本扩展"),
            [scripts] { scripts->reloadAll(); }
        );
        commands->addCommand(
            QStringLiteral("应用"),
            QStringLiteral("打开下载目录"),
            QStringLiteral("在文件管理器中打开默认下载保存目录"),
            [ctrl] { ctrl->openFolder(EdmConfig::getDefaultSaveDir()); }
        );
    }

    /// 组装脚本宿主服务（Qt 适配 + 托盘通知/宿主日志/任务数据源），并初始化脚本子系统
    void _initializeScripting() {
        scriptServices_ = std::make_unique<script::qt::QtHostServices>(QCoreApplication::instance());
        auto& services  = *scriptServices_;

        services.scriptsDirValue    = EdmConfig::getScriptsDir().toStdString();
        services.appDataDirValue    = EdmConfig::getAppDataDir().toStdString();
        services.hookTimeoutMsValue = EdmConfig::getInstance().getScriptHookTimeoutMs();
        services.userAgentValue     = EdmConfig::getInstance().getUserAgent().toStdString();

        services.disabledScriptsHandler = [] {
            auto const               q = EdmConfig::getInstance().getDisabledScripts();
            std::vector<std::string> out;
            out.reserve(static_cast<std::size_t>(q.size()));
            for (auto const& s : q) out.push_back(s.toStdString());
            return out;
        };
        services.setDisabledScriptsHandler = [](std::vector<std::string> const& names) {
            QStringList q;
            q.reserve(static_cast<qsizetype>(names.size()));
            for (auto const& name : names) q.append(QString::fromStdString(name));
            EdmConfig::getInstance().setDisabledScripts(q);
        };
        services.warningHandler = [](std::string const& message) { qWarning().noquote() << message.c_str(); };

        auto* commands                  = commandRegistry_;
        services.commandRegisterHandler = [commands](
                                              std::string const&    source,
                                              std::string const&    title,
                                              std::string const&    description,
                                              std::function<void()> fn
                                          ) {
            return commands->addCommand(
                QString::fromStdString(source),
                QString::fromStdString(title),
                QString::fromStdString(description),
                std::move(fn)
            );
        };
        services.commandUnregisterHandler = [commands](std::int64_t handle) { commands->removeCommand(handle); };

        auto* service               = appService_.get();
        services.taskGetHandler     = [service](int id) { return service->taskSnapshot(id); };
        services.taskGetAllHandler  = [service] { return service->taskSnapshots(); };
        services.taskControlHandler = [service](int id, script::TaskAction action) {
            return service->taskAction(id, action);
        };

        services.notifyHandler =
            [this](script::NotifyLevel level, std::string const& title, std::string const& message) {
                if (!trayIcon_ || !QSystemTrayIcon::supportsMessages()) return;
                auto const icon = level == script::NotifyLevel::Error
                                    ? QSystemTrayIcon::Critical
                                    : (level == script::NotifyLevel::Warning ? QSystemTrayIcon::Warning
                                                                             : QSystemTrayIcon::Information);
                trayIcon_->showMessage(QString::fromStdString(title), QString::fromStdString(message), icon, 5000);
            };

        scripting_->initialize(scriptServices_.get());
    }

    void _initSystemTrayIcon() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qDebug() << "System tray is not available";
            return;
        }

        trayIcon_ = std::make_unique<QSystemTrayIcon>(QIcon(":/assets/icons/logo.ico"));
        auto menu = new QMenu();
        auto show = menu->addAction(QStringLiteral("显示主窗口"));
        auto exit = menu->addAction(QStringLiteral("退出"));
        trayIcon_->setContextMenu(menu);
        trayIcon_->show();

        QObject::connect(show, &QAction::triggered, [this]() { _showMainWindow(); });

        QObject::connect(exit, &QAction::triggered, []() { QApplication::quit(); });

        QObject::connect(
            trayIcon_.get(),
            &QSystemTrayIcon::activated,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    _showMainWindow();
                }
            }
        );

        connectStatusNotifications();
    }

    /// 任务进入终态时走系统托盘消息通知（受设置开关控制）。
    /// 以 trayIcon_ 作为连接上下文：托盘销毁后连接自动断开。
    void connectStatusNotifications() {
        QObject::connect(
            appService_.get(),
            &AppService::taskFinished,
            trayIcon_.get(),
            [this](int /*taskId*/, int state, QString const& fileName, QString const& errorMsg) {
                if (!trayIcon_) return;
                if (!EdmConfig::getInstance().canShowStatusNotification()) return;

                if (!QSystemTrayIcon::supportsMessages()) {
                    qDebug() << "系统不支持托盘通知，跳过状态通知";
                    return;
                }

                QString title;
                QString body;
                auto    icon = QSystemTrayIcon::Information;
                switch (static_cast<TaskState>(state)) {
                case TaskState::Finished:
                    title = QStringLiteral("下载完成");
                    body  = fileName;
                    break;
                case TaskState::Failed:
                    title = QStringLiteral("下载失败");
                    body  = errorMsg.isEmpty() ? fileName : (fileName + QChar('\n') + errorMsg);
                    icon  = QSystemTrayIcon::Critical;
                    break;
                case TaskState::Canceled:
                    title = QStringLiteral("已取消");
                    body  = fileName;
                    icon  = QSystemTrayIcon::Warning;
                    break;
                default:
                    return;
                }
                trayIcon_->showMessage(title, body, icon, 5000);
            }
        );

        // 点击通知回到主窗口
        QObject::connect(trayIcon_.get(), &QSystemTrayIcon::messageClicked, [this]() { _showMainWindow(); });
    }

    void shutdown() {
        taskListModel_->stopRefresh();

        if (shadowFilterInstalled_) {
            qApp->removeNativeEventFilter(&shadowFilter_);
            shadowFilterInstalled_ = false;
        }

#ifdef Q_OS_WIN
        shadowFilter_.setWindow(nullptr);
#endif

        trayIcon_.reset();
        mainWindow_ = nullptr;

        // 先卸载脚本引擎（onDisable 回调需要 appService/db 仍可用），再关闭 QML 与服务
        scripting_->shutdown();
        scriptServices_.reset();
        qmlEngine_.reset();

        // The objects are parented to the engine for cleanup.
        taskListModel_   = nullptr;
        settingsModel_   = nullptr;
        controller_      = nullptr;
        scriptManager_   = nullptr;
        commandRegistry_ = nullptr;
        wsServerModel_   = nullptr;

        // WebSocket 服务器的 handler 直接调用 AppService，须先于服务停止
        wsBridge_->shutdown();
        wsBridge_.reset();

        // 先停调度器并释放数据库（任务进入 Paused 并持久化），再销毁服务。
        appService_->shutdown();
        appService_.reset();
        scripting_.reset();

        // 任务日志全部落盘后关闭 spdlog
        TaskLoggerManager::shutdown();
    }
};

EdmApplication::EdmApplication() : impl(std::make_unique<Impl>()) {}
EdmApplication::~EdmApplication() = default;

EdmApplication& EdmApplication::getInstance() {
    static EdmApplication instance;
    return instance;
}

void EdmApplication::initialize() { impl->initiailze(); }
void EdmApplication::shutdown() { impl->shutdown(); }

QSystemTrayIcon*       EdmApplication::getTrayIcon() const { return impl->trayIcon_.get(); }
QQmlApplicationEngine* EdmApplication::getQmlEngine() const { return impl->qmlEngine_.get(); }

QQuickWindow* EdmApplication::getMainWindow() const { return impl->mainWindow_; }

} // namespace edm