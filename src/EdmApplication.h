#pragma once

#include <memory>
#include <qtclasshelpermacros.h>


class QApplication;
class QSystemTrayIcon;
class QQmlApplicationEngine;
class QQuickWindow;

namespace edm {

class AppService;
class QmlTaskListModel;
class QmlSettings;
class QmlController;

class EdmApplication {
    struct Impl;
    std::unique_ptr<Impl> impl;

    explicit EdmApplication();

public:
    ~EdmApplication();
    Q_DISABLE_COPY_MOVE(EdmApplication);

    static EdmApplication& getInstance();

    /**
     * @brief Initialize the application.
     *
     * Creates the AppService (database + download engine), QML engine,
     * registers QML types, and loads the main QML file. Must be called after
     * QApplication is constructed.
     */
    void initialize();

    /**
     * @brief Tear down all resources.
     */
    void shutdown();

    [[nodiscard]] QSystemTrayIcon*       getTrayIcon() const;
    [[nodiscard]] QQmlApplicationEngine* getQmlEngine() const;

    /// Returns the root QQuickWindow if loaded, otherwise nullptr.
    [[nodiscard]] QQuickWindow* getMainWindow() const;
};

} // namespace edm
