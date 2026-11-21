#include "QmlController.h"

#include "EdmApplication.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QQuickWindow>
#include <QUrl>
#include <qstringview.h>

#include <algorithm>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

namespace edm {

QmlController::QmlController(QObject* parent) : QObject(parent) {}

QmlController::~QmlController() {
#ifdef Q_OS_WIN
    for (auto const& entry : framelessEntries_) {
        QApplication::instance()->removeNativeEventFilter(entry.filter.get());
    }
#endif
}

void QmlController::minimizeToTray() {
    if (auto* window = EdmApplication::getInstance().getMainWindow()) {
        window->hide();
    }
}

bool QmlController::openFile(QString const& path) {
    if (path.isEmpty()) return false;
    QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        emit errorOccurred(QStringLiteral("Failed to open file: ") + path);
        return false;
    }
    return true;
}

bool QmlController::openFolder(QString const& path) {
    if (path.isEmpty()) return false;
    QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        emit errorOccurred(QStringLiteral("Failed to open folder: ") + path);
        return false;
    }
    return true;
}

QString QmlController::clipboardText() const { return QApplication::clipboard()->text(); }

QString QmlController::removeFileUrlPrefix(QString const& url) {
    // Since QUrl can't be called directly in QML,
    //  here's a helper to convert file URLs in QML
    return QUrl{url}.toLocalFile();
}

void QmlController::installWindowFrameless(QWindow* window) {
#ifdef Q_OS_WIN
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    if (!quickWindow) return;

    // 幂等：同一窗口只注册一次（重复调用会重复 style 交换，过滤器翻倍）
    auto found = std::find_if(framelessEntries_.begin(), framelessEntries_.end(), [&](FramelessEntry const& entry) {
        return entry.window == quickWindow;
    });
    if (found != framelessEntries_.end()) return;

    auto filter = std::make_unique<FramelessShadowFilter>();
    filter->setResizeBordersEnabled(true);
    QApplication::instance()->installNativeEventFilter(filter.get());
    applyFramelessStyle(quickWindow, filter.get());
    framelessEntries_.push_back({quickWindow, std::move(filter)});

    // 每次显示都重新施加：隐藏时 Qt 可能销毁平台窗口，重新显示会拿到新 HWND，
    // 旧过滤器的 hwnd_ 失配后 WM_NCCALCSIZE 与 DWM 边距不再生效
    // 窗口销毁即卸载过滤器
    connect(quickWindow, &QQuickWindow::visibleChanged, this, [this, quickWindow](bool visible) {
        if (!visible) return;
        auto entry = std::find_if(framelessEntries_.begin(), framelessEntries_.end(), [&](FramelessEntry const& e) {
            return e.window == quickWindow;
        });
        if (entry != framelessEntries_.end()) applyFramelessStyle(entry->window, entry->filter.get());
    });
    connect(quickWindow, &QObject::destroyed, this, [this, quickWindow] {
        std::erase_if(framelessEntries_, [quickWindow](FramelessEntry const& entry) {
            if (entry.window != quickWindow) return false;
            QApplication::instance()->removeNativeEventFilter(entry.filter.get());
            return true;
        });
    });
#else
    Q_UNUSED(window);
#endif
}

void QmlController::applyFramelessStyle(QQuickWindow* window, FramelessShadowFilter* filter) {
#ifdef Q_OS_WIN
    if (!window || !filter) return;

    // 确保原生 HWND 存在（隐藏窗口首次调用时创建；重建后取得新值）
    window->create();
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    filter->setWindow(hwnd);

    // 与主窗口一致的样式处理：去掉 WS_POPUP 换成可参与 DWM 动画的 WS_OVERLAPPEDWINDOW，
    // 非客户区由 FramelessShadowFilter 的 WM_NCCALCSIZE 收为 0。
    LONG_PTR style  = GetWindowLongPtr(hwnd, GWL_STYLE);
    style          &= ~static_cast<LONG_PTR>(WS_POPUP);
    style          |= WS_OVERLAPPEDWINDOW;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    const MARGINS margins{1, 1, 1, 1};
    if (FAILED(DwmExtendFrameIntoClientArea(hwnd, &margins))) {
        emit errorOccurred(QStringLiteral("DwmExtendFrameIntoClientArea failed"));
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    Q_UNUSED(window);
    Q_UNUSED(filter);
#endif
}

} // namespace edm
