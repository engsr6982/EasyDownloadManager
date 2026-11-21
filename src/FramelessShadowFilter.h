#pragma once

#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


namespace edm {

class FramelessShadowFilter final : public QAbstractNativeEventFilter {
#ifdef Q_OS_WIN
private:
    HWND hwnd_ = nullptr;
    bool resizeBorders_{false};

public:
    inline void setWindow(HWND hwnd) noexcept { hwnd_ = hwnd; }

    inline HWND window() const noexcept { return hwnd_; }

    /// 启用后 WM_NCHITTEST 对窗口边缘返回 HT*（无边框窗口的拖拽缩放）
    inline void setResizeBordersEnabled(bool enabled) noexcept { resizeBorders_ = enabled; }
#endif

public:
    bool nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) override;
};

} // namespace edm
