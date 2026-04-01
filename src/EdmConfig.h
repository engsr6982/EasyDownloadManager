#pragma once
#include <magic_enum/magic_enum.hpp>
#include <qsettings.h>
#include <qtclasshelpermacros.h>

#include "Global.h"

namespace edm {

struct ProxyConfig {
    ProxyType              type_{ProxyType::None};
    std::optional<QString> host_{std::nullopt};
    std::optional<int>     port_{std::nullopt};
    std::optional<QString> user_{std::nullopt};
    std::optional<QString> password_{std::nullopt};

    inline bool isNone() const noexcept { return type_ == ProxyType::None; }
    inline bool isHttpSeries() const noexcept { return type_ == ProxyType::Http || type_ == ProxyType::Https; }
    inline bool isSocks4Series() const noexcept { return type_ == ProxyType::Socks4 || type_ == ProxyType::Socks4a; }
    inline bool isSocks5Series() const noexcept { return type_ == ProxyType::Socks5 || type_ == ProxyType::Socks5h; }

    /**
     * 转为代理地址
     * @note 格式：[protocol://][user[:password]@]host[:port]
     * @return 代理地址
     */
    [[nodiscard]] std::string toProxyUrl() const;
};


class EdmConfig {
    QSettings settings_;
    explicit EdmConfig();

public:
    ~EdmConfig();
    Q_DISABLE_COPY_MOVE(EdmConfig);

public:
    [[nodiscard]] static EdmConfig& getInstance();

    // 线程
    int  getThreadCount() const;
    void setThreadCount(int count);

    // 带宽
    BandWidthLimit getBandwidthLimit() const;
    void           setBandwidthLimit(BandWidthLimit limit);

    // User-Agent
    QString getUserAgent() const;
    void    setUserAgent(QString const& userAgent);

    // 代理
    [[nodiscard]] ProxyConfig getProxyConfig() const;

    void setProxyConfig(ProxyConfig const& config);

    inline bool canUseProxy() const { return getProxyConfig().type_ != ProxyType::None; }

    // 存储、临时路径
    QString getSaveDir() const;
    void    setSaveDir(QString const& dir);

    // 自启动
    bool canAutoStart() const;
    void setAutoStart(bool autoStart);

    // 下载完成对话框
    bool canShowDownloadCompleteDialog() const;
    void setShowDownloadCompleteDialog(bool show);

public:
    static inline constexpr auto kDefaultAutoStart    = false;
    static inline constexpr auto kDefaultShowComplete = true;

    static QString getAppDataDir();
    static QString getDefaultSaveDir();
    static QString getDatabasePath();
};

} // namespace edm
