#include "EdmApplication.h"

#include "AppService.h"
#include "FramelessShadowFilter.h"
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
        LONG_PTR style  = GetWindowLongPtr(hwnd, GWL_STYLE);
        style          |= WS_THICKFRAME;
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