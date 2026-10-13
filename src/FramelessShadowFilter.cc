#include "FramelessShadowFilter.h"

#include <QByteArray>

namespace edm {

bool FramelessShadowFilter::nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType != QByteArrayLiteral("windows_generic_MSG")) return false;

    if (!message) return false;

    auto* msg = static_cast<MSG*>(message);

    // 仅处理主窗口的原生消息。
    if (!hwnd_ || msg->hwnd != hwnd_) return false;

    // 自定义非客户区布局。
    if (msg->message == WM_NCCALCSIZE && msg->wParam != 0) {
        if (result) *result = 0;

        return true;
    }

    // 关键修复：其他所有消息都交给 Qt / Windows 继续处理。
    return false;

#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);

    return false;
#endif
}

} // namespace edm