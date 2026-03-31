#pragma once
#include "Global.h"

#include <array>
#include <format>


namespace edm::utils {


inline std::string FileSize2String(FileSize size) {
    if (size <= 0) return "0 B";

    constexpr std::array<const char*, 5> units{"B", "KB", "MB", "GB", "TB"};

    auto   sz        = static_cast<double>(size);
    size_t unitIndex = 0;

    while (sz >= 1024.0 && unitIndex < units.size() - 1) {
        sz /= 1024.0;
        ++unitIndex;
    }

    // 保留两位小数
    return std::format("{:.2f} {}", sz, units[unitIndex]);
}

inline std::string TimeStamp2String(TimeStamp time) {
    if (time <= 0) return "-";

    // 转换为 time_t
    std::time_t t = static_cast<std::time_t>(time);

    // 转换为本地时间
    std::tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &t);
#else
    localtime_r(&t, &localTm);
#endif

    // 格式化：yyyy-MM-dd HH:mm:ss
    return std::format(
        "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
        localTm.tm_year + 1900,
        localTm.tm_mon + 1,
        localTm.tm_mday,
        localTm.tm_hour,
        localTm.tm_min,
        localTm.tm_sec
    );
}


} // namespace edm::utils