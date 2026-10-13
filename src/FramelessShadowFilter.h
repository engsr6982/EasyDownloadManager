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

public:
    inline void setWindow(HWND hwnd) noexcept { hwnd_ = hwnd; }

    inline HWND window() const noexcept { return hwnd_; }
#endif

public:
    bool nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) override;
};

} // namespace edm
