#pragma once
#include <memory>


class QSystemTrayIcon;

namespace edm {

class MainWindow;
class Dispatcher;
class SettingsDialog;

class EasyDownloadManager {
    static EasyDownloadManager* instance_;
    explicit EasyDownloadManager();

public:
    ~EasyDownloadManager();
    EasyDownloadManager(EasyDownloadManager const&)            = delete;
    EasyDownloadManager(EasyDownloadManager&&)                 = delete;
    EasyDownloadManager& operator=(EasyDownloadManager const&) = delete;
    EasyDownloadManager& operator=(EasyDownloadManager&&)      = delete;

    static EasyDownloadManager& getOrNewInstance();
    static void                 tryDestroyInstance();

    void _initSystemTrayIcon();

    MainWindow*     getMainWindow() const;
    Dispatcher*     getDispatcher() const;
    SettingsDialog* getSettingsDialog() const;
    QSystemTrayIcon* getTrayIcon() const;

    void tryShowSettingDialog() const;

private:
    std::unique_ptr<Dispatcher>      dispatcher_{nullptr};
    std::unique_ptr<MainWindow>      mainWindow_{nullptr};
    std::unique_ptr<SettingsDialog>  settingsDialog_{nullptr};
    std::unique_ptr<QSystemTrayIcon> trayIcon_{nullptr};
};

} // namespace edm
