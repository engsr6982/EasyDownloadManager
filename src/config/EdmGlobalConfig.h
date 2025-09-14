#pragma once
#include <qsettings.h>
#include <qtclasshelpermacros.h>

namespace edm {

class EdmGlobalConfig {
    QSettings settings_;
    explicit EdmGlobalConfig();

public:
    ~EdmGlobalConfig();
    Q_DISABLE_COPY_MOVE(EdmGlobalConfig);

public:
    [[nodiscard]] static EdmGlobalConfig& instance();

    // 线程
    int  getThreadCount() const;
    void setThreadCount(int count);

    // 带宽
    unsigned int getBandwidthLimit() const;
    void         setBandwidthLimit(unsigned int limit);

    // User-Agent
    QString getUserAgent() const;
    void    setUserAgent(QString const& userAgent);

    // 代理
    enum class ProxyType { None, Http, Https, Socks4, Socks4a, Socks5, Socks5h };
    struct ProxyConfig {
        ProxyType              type_{ProxyType::None};
        std::optional<QString> host_{std::nullopt};
        std::optional<int>     port_{std::nullopt};
        std::optional<QString> user_{std::nullopt};
        std::optional<QString> password_{std::nullopt};
    };
    [[nodiscard]] ProxyConfig getProxyConfig() const;

    void setProxyConfig(ProxyConfig const& config);

    // 存储、临时路径
    QString getSaveDir() const;
    void    setSaveDir(QString const& dir);

    QString getTempDir() const;
    void    setTempDir(QString const& dir);

    // 自启动
    bool canAutoStart() const;
    void setAutoStart(bool autoStart);

    // 下载完成对话框
    bool canShowDownloadCompleteDialog() const;
    void setShowDownloadCompleteDialog(bool show);
};

} // namespace edm
