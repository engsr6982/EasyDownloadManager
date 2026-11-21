#include "FramelessShadowFilter.h"

#include <QByteArray>

namespace edm {

bool FramelessShadowFilter::nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType != QByteArrayLiteral("windows_generic_MSG")) return false;

    if (!message) return false;

    auto* msg = static_cast<MSG*>(message);

    // 仅处理绑定的窗口的原生消息。
    if (!hwnd_ || msg->hwnd != hwnd_) return false;

    // 无边框窗口的边缘拖拽缩放：命中 8px 缩放带时返回对应 HT*，其余消息交给 Qt。
    if (resizeBorders_ && msg->message == WM_NCHITTEST && !IsZoomed(hwnd_)) {
        auto const x = static_cast<short>(LOWORD(msg->lParam));
        auto const y = static_cast<short>(HIWORD(msg->lParam));

        RECT rc;
        if (!GetWindowRect(hwnd_, &rc)) return false;

        constexpr LONG kBorder = 8;
        bool const     left    = x < rc.left + kBorder;
        bool const     right   = x >= rc.right - kBorder;
        bool const     top     = y < rc.top + kBorder;
        bool const     bottom  = y >= rc.bottom - kBorder;

        if (top && left) {
            *result = HTTOPLEFT;
        } else if (top && right) {
            *result = HTTOPRIGHT;
        } else if (bottom && left) {
            *result = HTBOTTOMLEFT;
        } else if (bottom && right) {
            *result = HTBOTTOMRIGHT;
        } else if (top) {
            *result = HTTOP;
        } else if (bottom) {
            *result = HTBOTTOM;
        } else if (left) {
            *result = HTLEFT;
        } else if (right) {
            *result = HTRIGHT;
        } else {
            return false;
        }

        return true;
    }

    // 自定义非客户区布局。
    if (msg->message == WM_NCCALCSIZE && msg->wParam != 0) {
        if (result) *result = 0;

        return true;
    }

    // 其他所有消息交给 Qt 与 Windows 继续处理。
    return false;

#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);

    return false;
#endif
}

} // namespace edm