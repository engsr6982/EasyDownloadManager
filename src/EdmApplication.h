#pragma once

#include <memory>
#include <qtclasshelpermacros.h>

class QSystemTrayIcon;
class QQmlApplicationEngine;
class QQuickWindow;

namespace edm {

class Dispatcher;
class DownloadDatabase;
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
     * Creates the database, dispatcher, QML engine, registers QML types, and
     * loads the main QML file. Must be called after QApplication is constructed.
     */
    void initApp();

    /**
     * @brief Tear down all resources.
     */
    void destroyApp();

    [[nodiscard]] Dispatcher*            getDispatcher() const;
    [[nodiscard]] DownloadDatabase*      getDatabase() const;
    [[nodiscard]] QSystemTrayIcon*       getTrayIcon() const;
    [[nodiscard]] QQmlApplicationEngine* getQmlEngine() const;

    /// Returns the root QQuickWindow if loaded, otherwise nullptr.
    [[nodiscard]] QQuickWindow* getMainWindow() const;

    // QML-facing singleton accessors (for internal wiring)
    [[nodiscard]] QmlTaskListModel* getTaskListModel() const;
    [[nodiscard]] QmlSettings*      getSettingsModel() const;
    [[nodiscard]] QmlController*    getController() const;
};

} // namespace edm
