#pragma once
#include <memory>

namespace edm {

class MainWindow;

class EasyDownloadManager {
public:
    EasyDownloadManager();
    ~EasyDownloadManager();

    EasyDownloadManager(EasyDownloadManager const&)            = delete;
    EasyDownloadManager(EasyDownloadManager&&)                 = delete;
    EasyDownloadManager& operator=(EasyDownloadManager const&) = delete;
    EasyDownloadManager& operator=(EasyDownloadManager&&)      = delete;

    void showMainWindow() const;

private:
    std::unique_ptr<MainWindow> mainWindow_{nullptr};
};

} // namespace edm
