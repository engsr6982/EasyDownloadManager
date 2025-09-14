#include "EasyDownloadManager.h"

#include "Dispatcher.h"
#include "ui/main/MainWindow.h"
#include "ui/settings/SettingsDialog.h"

namespace edm {

EasyDownloadManager* EasyDownloadManager::instance_{nullptr};

EasyDownloadManager& EasyDownloadManager::getOrNewInstance() {
    if (!instance_) {
        instance_ = new EasyDownloadManager{};
    }
    return *instance_;
}
void EasyDownloadManager::tryDestroyInstance() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

EasyDownloadManager::EasyDownloadManager() {
    dispatcher_     = std::make_unique<Dispatcher>();
    mainWindow_     = std::make_unique<MainWindow>();
    settingsDialog_ = std::make_unique<SettingsDialog>(mainWindow_.get());
}

EasyDownloadManager::~EasyDownloadManager() {
    settingsDialog_.reset();
    mainWindow_.reset();
    dispatcher_.reset();
}

MainWindow*     EasyDownloadManager::getMainWindow() const { return mainWindow_.get(); }
Dispatcher*     EasyDownloadManager::getDispatcher() const { return dispatcher_.get(); }
SettingsDialog* EasyDownloadManager::getSettingsDialog() const { return settingsDialog_.get(); }


void EasyDownloadManager::tryShowSettingDialog() const {
    if (settingsDialog_->isVisible()) {
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
    } else {
        settingsDialog_->show();
    }
}


} // namespace edm