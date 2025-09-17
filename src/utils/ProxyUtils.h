#pragma once
#include <QString>
#include <optional>
#include <sstream>


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

/**
 * 转为代理地址
 * @note 格式：[protocol://][user[:password]@]host[:port]
 * @param cfg 代理配置
 * @return 代理地址
 */
inline std::string toProxyUrl(const ProxyConfig& cfg) {
    if (cfg.isNone()) return {};

    std::ostringstream oss;

    // 1. protocol
    switch (cfg.type_) {
    case Type::Http:
        oss << "http://";
        break;
    case Type::Https:
        oss << "https://";
        break;
    case Type::Socks4:
        oss << "socks4://";
        break;
    case Type::Socks4a:
        oss << "socks4a://";
        break;
    case Type::Socks5:
        oss << "socks5://";
        break;
    case Type::Socks5h:
        oss << "socks5h://";
        break;
    default:
        return {};
    }

    // 2. user/password
    if (cfg.user_ && !cfg.user_->isEmpty()) {
        oss << cfg.user_->toStdString();
        if (cfg.isSocks5Series() || cfg.isHttpSeries()) {
            if (cfg.password_ && !cfg.password_->isEmpty()) {
                oss << ":" << cfg.password_->toStdString();
            }
        }
        oss << "@";
    }

    // 3. host
    if (cfg.host_ && !cfg.host_->isEmpty()) {
        oss << cfg.host_->toStdString();
    } else {
        return {};
    }

    // 4. port
    if (cfg.port_ && *cfg.port_ > 0) {
        oss << ":" << *cfg.port_;
    }

    return oss.str();
}


} // namespace edm::proxy_utils