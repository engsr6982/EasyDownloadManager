#include "EasyDownloadManager.h"

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

EasyDownloadManager* EasyDownloadManager::instance_{nullptr};

EasyDownloadManager& EasyDownloadManager::getOrNewInstance() {
    if (instance_ == nullptr) {
        instance_ = new EasyDownloadManager{};
        instance_->_delayInit();
    }
    return *instance_;
}
void EasyDownloadManager::tryDestroyInstance() {
    if (instance_) {
        instance_->_delayCleanup();
        delete instance_;
        instance_ = nullptr;
    }
}

EasyDownloadManager::EasyDownloadManager()  = default;
EasyDownloadManager::~EasyDownloadManager() = default;
void EasyDownloadManager::_delayInit() {
    database_       = std::make_unique<DownloadDatabase>();
    dispatcher_     = std::make_unique<Dispatcher>();
    mainWindow_     = std::make_unique<MainWindow>();
    settingsDialog_ = std::make_unique<SettingsDialog>(mainWindow_.get());

    _initSystemTrayIcon();
}
void EasyDownloadManager::_delayCleanup() {
    trayIcon_.reset();
    settingsDialog_.reset();
    mainWindow_.reset();
    dispatcher_.reset();
    database_.reset();
}

void EasyDownloadManager::_initSystemTrayIcon() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "System tray is not available";
        return;
    }
    trayIcon_ = std::make_unique<QSystemTrayIcon>(QIcon(":/icons/logo.ico"), getMainWindow());
    auto menu = new QMenu(getMainWindow());
    auto show = menu->addAction("显示主窗口");
    auto exit = menu->addAction("退出");
    trayIcon_->setContextMenu(menu);
    trayIcon_->show();

    QObject::connect(show, &QAction::triggered, getMainWindow(), &MainWindow::showNormal);
    QObject::connect(exit, &QAction::triggered, qApp, &QApplication::quit);

    QObject::connect(
        trayIcon_.get(),
        &QSystemTrayIcon::activated,
        getMainWindow(),
        [](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                auto main = EasyDownloadManager::getOrNewInstance().getMainWindow();
                main->showNormal();
                main->activateWindow();
            }
        }
    );
}

MainWindow*       EasyDownloadManager::getMainWindow() const { return mainWindow_.get(); }
Dispatcher*       EasyDownloadManager::getDispatcher() const { return dispatcher_.get(); }
SettingsDialog*   EasyDownloadManager::getSettingsDialog() const { return settingsDialog_.get(); }
QSystemTrayIcon*  EasyDownloadManager::getTrayIcon() const { return trayIcon_.get(); }
DownloadDatabase* EasyDownloadManager::getDatabase() const { return database_.get(); }


void EasyDownloadManager::tryShowSettingDialog() const {
    if (settingsDialog_->isVisible()) {
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
    } else {
        settingsDialog_->show();
    }
}


} // namespace edm