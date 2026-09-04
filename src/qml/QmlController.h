#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qobject.h>
#include <qtmetamacros.h>

class QSystemTrayIcon;

namespace edm {

/**
 * @brief Application-level controller exposed to QML.
 *
 * Provides actions that don't fit in the task model or settings model:
 * - Minimize to tray
 * - Open file or folder via QDesktopServices
 * - Read clipboard text (for NewTaskDialog auto-paste)
 */
class QmlController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QmlController(QObject* parent = nullptr);
    ~QmlController() override = default;

    /// Minimize the main window to the system tray.
    Q_INVOKABLE void minimizeToTray();

    /// Open a file with the system's default handler.
    Q_INVOKABLE bool openFile(QString const& path);

    /// Open a folder in the system file manager.
    Q_INVOKABLE bool openFolder(QString const& path);

    /// Return the current clipboard text (for URL auto-paste).
    Q_INVOKABLE QString clipboardText() const;

    // Remove the file:// prefix from a URL.
    Q_INVOKABLE QString removeFileUrlPrefix(QString const& url);

signals:
    /// Emitted when an error occurs (e.g., file open failure).
    void errorOccurred(QString const& message);
};

} // namespace edm
