#include "EdmApplication.h"

#include "AppService.h"
#include "EdmConfig.h"
#include "FramelessShadowFilter.h"
#include "Global.h"
#include "qml/QmlController.h"
#include "qml/QmlSettings.h"
#include "qml/QmlTaskListModel.h"

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

namespace edm {


struct EdmApplication::Impl {
    std::unique_ptr<AppService>            appService_{nullptr};
    std::unique_ptr<QQmlApplicationEngine> qmlEngine_{nullptr};
    std::unique_ptr<QSystemTrayIcon>       trayIcon_{nullptr};

    // QML bridge objects (owned by the application)
    QmlTaskListModel* taskListModel_{nullptr};
    QmlSettings*      settingsModel_{nullptr};
    QmlController*    controller_{nullptr};

    QQuickWindow* mainWindow_{nullptr};

    FramelessShadowFilter shadowFilter_;
    bool                  shadowFilterInstalled_{false};

    void initiailze() {
        // 1. 应用服务：数据库 + 下载引擎 + 观察器接线
        appService_ = std::make_unique<AppService>();

        // 2. QML engine
        _initializeQml();

        // 3. System tray icon
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
        // 去掉 WS_POPUP、换成 WS_OVERLAPPEDWINDOW，外观不受影响——
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

        // The objects are parented to the engine for cleanup.
        taskListModel_->setParent(qmlEngine_.get());
        settingsModel_->setParent(qmlEngine_.get());
        controller_->setParent(qmlEngine_.get());

        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "TaskListModel", taskListModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "SettingsModel", settingsModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "AppController", controller_);

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
        qmlEngine_.reset();

        // The objects are parented to the engine for cleanup.
        taskListModel_ = nullptr;
        settingsModel_ = nullptr;
        controller_    = nullptr;

        // 先停调度器并释放数据库（任务进入 Paused 并持久化），再销毁服务。
        appService_->shutdown();
        appService_.reset();
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