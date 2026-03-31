#pragma once
#include <cstdint>

namespace edm {

using BandWidthLimit = int64_t; // 带宽限制，单位为 kb/s

using FileSize = int64_t; // 文件大小，单位为 byte

using TimeStamp = int64_t; // 时间戳

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