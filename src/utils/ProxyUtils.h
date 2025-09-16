#pragma once
#include <QString>
#include <optional>


namespace edm::proxy_utils {


enum class Type { None = 0, Http, Https, Socks4, Socks4a, Socks5, Socks5h };

inline bool isNone(Type type) noexcept { return type == Type::None; }
inline bool isHttpSeries(Type type) noexcept { return type == Type::Http || type == Type::Https; }
inline bool isSocks4Series(Type type) noexcept { return type == Type::Socks4 || type == Type::Socks4a; }
inline bool isSocks5Series(Type type) noexcept { return type == Type::Socks5 || type == Type::Socks5h; }

struct ProxyConfig {
    Type                   type_{Type::None};
    std::optional<QString> host_{std::nullopt};
    std::optional<int>     port_{std::nullopt};
    std::optional<QString> user_{std::nullopt};
    std::optional<QString> password_{std::nullopt};

    inline bool isNone() const noexcept { return proxy_utils::isNone(type_); }
    inline bool isHttpSeries() const noexcept { return proxy_utils::isHttpSeries(type_); }
    inline bool isSocks4Series() const noexcept { return proxy_utils::isSocks4Series(type_); }
    inline bool isSocks5Series() const noexcept { return proxy_utils::isSocks5Series(type_); }
};


} // namespace edm::proxy_utils