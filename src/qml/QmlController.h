#pragma once

#include "FramelessShadowFilter.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qobject.h>
#include <qtmetamacros.h>

#include <QQuickWindow>
#include <QWindow>
#include <memory>
#include <vector>


namespace edm {

/**
 * @brief Application-level controller exposed to QML.
 *
 * Provides actions that don't fit in the task model or settings model:
 * - Minimize to tray
 * - Open file or folder via QDesktopServices
 * - Read clipboard text (for NewTaskDialog auto-paste)
 * - Frameless window chrome (native shadow + edge resize) for QML-declared windows
 */
class QmlController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QmlController(QObject* parent = nullptr);
    ~QmlController() override;

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

    /// 为 QML 声明的窗口安装无边框窗口外观：DWM 阴影 + 边缘拖拽缩放。
    /// 窗口需已声明 FramelessWindowHint；随窗口销毁自动卸载。
    Q_INVOKABLE void installWindowFrameless(QWindow* window);

signals:
    /// Emitted when an error occurs (e.g., file open failure).
    void errorOccurred(QString const& message);

private:
    struct FramelessEntry {
        QQuickWindow*                          window{nullptr};
        std::unique_ptr<FramelessShadowFilter> filter;
    };

    /// （重新）施加无边框 chrome：HWND 在窗口隐藏/显示间可能重建，每次 apply 都取最新值
    void applyFramelessStyle(QQuickWindow* window, FramelessShadowFilter* filter);

    std::vector<FramelessEntry> framelessEntries_;
};

} // namespace edm
