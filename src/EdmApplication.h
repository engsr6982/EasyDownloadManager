#pragma once
#include <memory>
#include <qtclasshelpermacros.h>


class QSystemTrayIcon;

namespace edm {

class MainWindow;
class Dispatcher;
class SettingsDialog;
class DownloadDatabase;

class EdmApplication {
    struct Impl;
    std::unique_ptr<Impl> impl;

    explicit EdmApplication();

public:
    ~EdmApplication();
    Q_DISABLE_COPY_MOVE(EdmApplication);

    static EdmApplication& getInstance();

    /**
     * 初始化应用
     * @note 请在 QApplication 初始化后调用
     */
    void initApp();
    void destroyApp();

    [[nodiscard]] MainWindow*       getMainWindow() const;
    [[nodiscard]] Dispatcher*       getDispatcher() const;
    [[nodiscard]] SettingsDialog*   getSettingsDialog() const;
    [[nodiscard]] QSystemTrayIcon*  getTrayIcon() const;
    [[nodiscard]] DownloadDatabase* getDatabase() const;

    void tryShowSettingDialog() const;
};

} // namespace edm
