#include "EasyDownloadManager.h"

#include "windows/main/MainWindow.h"

namespace edm {


EasyDownloadManager::EasyDownloadManager() { mainWindow_ = std::make_unique<MainWindow>(); }

EasyDownloadManager::~EasyDownloadManager() = default;

void EasyDownloadManager::showMainWindow() const { mainWindow_->show(); }


} // namespace edm