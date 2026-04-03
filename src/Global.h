#pragma once
#include <cstdint>

namespace edm {

using BandWidthLimit = int64_t; // 带宽限制，单位为 kb/s

using FileSize = int64_t; // 文件大小，单位为 byte

enum class Category {
    Video       = 0, // 视频
    Audio       = 1, // 音频
    Compressed  = 2, // 压缩包
    Document    = 3, // 文档
    Application = 4, // 应用程序
    Other       = 5  // 其它
};

enum class TaskState {
    Pending  = 0, // 等待开始
    Running  = 1, // 正在运行
    Paused   = 2, // 暂停
    Canceled = 3, // 取消
    Finished = 4, // 完成
    Failed   = 5, // 失败(异常)
};

enum class Resumable {
    Unknown = -1, // 未知
    No      = 0,  // 不支持断点续传
    Yes     = 1   // 支持断点续传
};

enum class AvailableThreads { k4 = 4, k8 = 8, k16 = 16, k32 = 32 };

enum class ProxyType { None = 0, Http, Https, Socks4, Socks4a, Socks5, Socks5h };

struct GlobalDefaults {
    static inline constexpr auto kDefaultThreadCount    = AvailableThreads::k8;
    static inline constexpr auto kDefaultBandwidthLimit = BandWidthLimit{0};
    static inline constexpr auto kDefaultUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                                     "(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0";
    static inline constexpr auto kDefaultProxyType = ProxyType::None;
    static inline constexpr auto kDefaultProxyHost = "127.0.0.1";
    static inline constexpr auto kDefaultProxyPort = 1080;
};

} // namespace edm