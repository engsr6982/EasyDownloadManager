#pragma once
#include <memory>

namespace edm {

class MainWindow;
class Dispatcher;

class EasyDownloadManager {
    static EasyDownloadManager* instance_;
    explicit EasyDownloadManager();

public:
    ~EasyDownloadManager()                                 ;
    EasyDownloadManager(EasyDownloadManager const&)            = delete;
    EasyDownloadManager(EasyDownloadManager&&)                 = delete;
    EasyDownloadManager& operator=(EasyDownloadManager const&) = delete;
    EasyDownloadManager& operator=(EasyDownloadManager&&)      = delete;

    static EasyDownloadManager& getOrNewInstance();
    static void                 tryDestroyInstance();

    MainWindow* getMainWindow() const;
    Dispatcher* getDispatcher() const;

private:
    std::unique_ptr<Dispatcher> dispatcher_{nullptr};
    std::unique_ptr<MainWindow> mainWindow_{nullptr};
};

} // namespace edm
