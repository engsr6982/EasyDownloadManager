#include "EdmApplication.h"

#include "Dispatcher.h"
#include "EdmConfig.h"
#include "QSystemTrayIcon"
#include "database/DownloadDatabase.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"
#include "ui/main/MainWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "utils/IconUtils.h"

#include <QApplication>
#include <qcoreapplication.h>
#include <qmenu.h>
#include <winsock2.h>

namespace edm {

struct EdmApplication::Impl {
    std::unique_ptr<Dispatcher>       dispatcher_{nullptr};
    std::unique_ptr<DownloadDatabase> database_{nullptr};
    std::unique_ptr<MainWindow>       mainWindow_{nullptr};
    std::unique_ptr<QSystemTrayIcon>  trayIcon_{nullptr};

    void init() {
        database_ = std::make_unique<DownloadDatabase>();

        Dispatcher::Options dispatcherOptions;
        dispatcherOptions.taskLoader = [this](TaskId id) {
            return database_ ? database_->getTaskById(id) : nullptr;
        };
        dispatcherOptions.onTaskChanged = [this](std::shared_ptr<TaskContext> const& ctx) {
            if (database_ && ctx && ctx->model) {
                database_->insertTask(ctx->model);
            }
        };
        dispatcherOptions.configureFactory = [](std::shared_ptr<TaskModel> const& model) {
            auto configure = std::make_shared<TaskConfigure>(model);
            if (auto proxy = EdmConfig::getInstance().getProxyConfig(); !proxy.isNone()) {
                configure->proxyUrl_ = proxy.toProxyUrl();
            }
            return configure;
        };

        dispatcher_ = std::make_unique<Dispatcher>(std::move(dispatcherOptions));
        mainWindow_ = std::make_unique<MainWindow>();

        _initSystemTrayIcon();
    }

    void destroy() {
        trayIcon_.reset();
        mainWindow_.reset();
        dispatcher_.reset();
        database_.reset();
    }

    void _initSystemTrayIcon() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qDebug() << "System tray is not available";
            return;
        }
        trayIcon_ = std::make_unique<QSystemTrayIcon>(QIcon(":/icons/logo.ico"), mainWindow_.get());
        auto menu = new QMenu(mainWindow_.get());
        auto show = menu->addAction("显示主窗口");
        auto exit = menu->addAction("退出");
        trayIcon_->setContextMenu(menu);
        trayIcon_->show();

        QObject::connect(show, &QAction::triggered, mainWindow_.get(), &MainWindow::showNormal);
        QObject::connect(exit, &QAction::triggered, qApp, &QApplication::quit);

        QObject::connect(
            trayIcon_.get(),
            &QSystemTrayIcon::activated,
            mainWindow_.get(),
            [](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    auto main = EdmApplication::getInstance().getMainWindow();
                    main->showNormal();
                    main->activateWindow();
                }
            }
        );
    }
};

EdmApplication::EdmApplication() : impl(std::make_unique<Impl>()) {}
EdmApplication::~EdmApplication() = default;


EdmApplication& EdmApplication::getInstance() {
    static EdmApplication instance;
    return instance;
}

void EdmApplication::initApp() { impl->init(); }
void EdmApplication::destroyApp() { impl->destroy(); }


MainWindow*       EdmApplication::getMainWindow() const { return impl->mainWindow_.get(); }
Dispatcher*       EdmApplication::getDispatcher() const { return impl->dispatcher_.get(); }
QSystemTrayIcon*  EdmApplication::getTrayIcon() const { return impl->trayIcon_.get(); }
DownloadDatabase* EdmApplication::getDatabase() const { return impl->database_.get(); }


} // namespace edm
