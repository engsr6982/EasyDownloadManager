#include "EdmApplication.h"

#include "Dispatcher.h"
#include "QSystemTrayIcon"
#include "database/DownloadDatabase.h"
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
    std::unique_ptr<SettingsDialog>   settingsDialog_{nullptr};
    std::unique_ptr<QSystemTrayIcon>  trayIcon_{nullptr};

    void init() {
        database_       = std::make_unique<DownloadDatabase>();
        dispatcher_     = std::make_unique<Dispatcher>();
        mainWindow_     = std::make_unique<MainWindow>();
        settingsDialog_ = std::make_unique<SettingsDialog>(mainWindow_.get());

        _initSystemTrayIcon();
    }

    void destroy() {
        trayIcon_.reset();
        settingsDialog_.reset();
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
SettingsDialog*   EdmApplication::getSettingsDialog() const { return impl->settingsDialog_.get(); }
QSystemTrayIcon*  EdmApplication::getTrayIcon() const { return impl->trayIcon_.get(); }
DownloadDatabase* EdmApplication::getDatabase() const { return impl->database_.get(); }


void EdmApplication::tryShowSettingDialog() const {
    if (impl->settingsDialog_->isVisible()) {
        impl->settingsDialog_->raise();
        impl->settingsDialog_->activateWindow();
    } else {
        impl->settingsDialog_->show();
    }
}


} // namespace edm