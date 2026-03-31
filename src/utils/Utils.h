#pragma once
#include "Global.h"

#include <array>
#include <format>
#include <unordered_set>


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


inline Category resolveFileCategory(std::string_view fileName) {
    if (fileName.empty()) return Category::Other;

    // clang-format off
    // todo: 移除硬编码，放入配置文件?
    static const std::unordered_set<std::string_view> kVideo{
        ".3gp", ".asf", ".avi", ".flv", ".m4v", ".mkv", ".mov",
        ".mp4", ".mpeg", ".mpg", ".rm", ".swf", ".vob", ".wmv",
        ".webm", ".ogv", ".ts", ".m2ts", ".mts", ".mxf", ".divx"
    };
    static const std::unordered_set<std::string_view> kAudio{
        ".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a", ".wma",
        ".opus", ".ape", ".ac3", ".dts", ".amr", ".au", ".snd",
        ".aiff", ".aif", ".mid", ".midi", ".ra", ".ram"
    };
    static const std::unordered_set<std::string_view> kCompressed{
        ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".xz",
        ".tgz", ".tbz2", ".txz", ".zst", ".lzma", ".lz", ".iso",
        ".cab", ".arj", ".z", ".tar.gz", ".tar.bz2", ".tar.xz"
    };
    static const std::unordered_set<std::string_view> kDocument{
        ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
        ".txt", ".md", ".rtf", ".odt", ".ods", ".odp", ".odg",
        ".csv", ".tex", ".log", ".wps", ".pages", ".numbers", ".key",
        ".epub", ".mobi", ".azw", ".cbr", ".cbz", ".djvu"
    };
    static const std::unordered_set<std::string_view> kApplication{
        ".exe", ".msi", ".dmg", ".pkg", ".deb", ".rpm", ".apk",
        ".appimage", ".jar", ".war", ".ear", ".bin", ".sh", ".bat",
        ".cmd", ".ps1", ".py", ".pl", ".rb", ".php", ".js", ".html",
        ".htm", ".css", ".xml", ".json", ".yaml", ".yml", ".toml",
        ".ini", ".cfg", ".conf", ".dll", ".so", ".dylib", ".sys",
        ".drv", ".ocx", ".ax", ".scr", ".cpl"
    };
    // clang-format on

    auto idx = fileName.find_last_of('.');
    if (idx != std::string::npos) {
        auto ext = fileName.substr(idx);
        if (kVideo.contains(ext)) return Category::Video;
        if (kAudio.contains(ext)) return Category::Audio;
        if (kCompressed.contains(ext)) return Category::Compressed;
        if (kDocument.contains(ext)) return Category::Document;
        if (kApplication.contains(ext)) return Category::Application;
    }
    return Category::Other;
}


} // namespace edm::utils