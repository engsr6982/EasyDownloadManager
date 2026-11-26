#pragma once
#include <magic_enum/magic_enum.hpp>
#include <qsettings.h>
#include <qstringlist.h>
#include <qtclasshelpermacros.h>

#include "Global.h"

namespace edm {

struct ProxyConfig {
    bool                   enabled_{false};
    ProxyType              type_{ProxyType::Socks5};
    std::optional<QString> host_{std::nullopt};
    std::optional<int>     port_{std::nullopt};
    std::optional<QString> user_{std::nullopt};
    std::optional<QString> password_{std::nullopt};

    inline bool isEnabled() const noexcept { return enabled_; }
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
    BandLimit getBandwidthLimit() const;
    void      setBandwidthLimit(BandLimit limit);

    // User-Agent
    QString getUserAgent() const;
    void    setUserAgent(QString const& userAgent);

    // 代理
    [[nodiscard]] ProxyConfig getProxyConfig() const;
    void                      setProxyConfig(ProxyConfig const& config);

    bool      getProxyEnabled() const;
    void      setProxyEnabled(bool enabled);
    ProxyType getProxyType() const;
    void      setProxyType(ProxyType type);
    QString   getProxyHost() const;
    void      setProxyHost(QString const& host);
    int       getProxyPort() const;
    void      setProxyPort(int port);
    QString   getProxyUser() const;
    void      setProxyUser(QString const& user);
    QString   getProxyPassword() const;
    void      setProxyPassword(QString const& password);

    // 存储、临时路径
    QString getSaveDir() const;
    void    setSaveDir(QString const& dir);

    // 自启动
    bool canAutoStart() const;
    void setAutoStart(bool autoStart);

    // 状态通知
    bool canShowStatusNotification() const;
    void setShowStatusNotification(bool show);

    // 脚本扩展系统
    /// 脚本目录：环境变量 EDM_SCRIPT_DIR 优先（开发调试用），否则 AppData/scripts
    static QString getScriptsDir();
    /// 被禁用的脚本名列表（未列出的默认启用）
    QStringList getDisabledScripts() const;
    void        setDisabledScripts(QStringList const& names);
    /// 可变钩子（Promise handler）的阻塞等待超时（毫秒）
    int  getScriptHookTimeoutMs() const;
    void setScriptHookTimeoutMs(int ms);

    // WebSocket 服务器（浏览器集成）
    bool getWsEnabled() const;
    void setWsEnabled(bool enabled);
    int  getWsPort() const;
    void setWsPort(int port);
    /// 允许局域网连接：关闭时只监听回环地址
    bool getWsAllowLan() const;
    void setWsAllowLan(bool allow);
    /// 访问令牌：首次读取时生成随机值并持久化，清空后重新生成
    QString getWsToken();
    void    setWsToken(QString const& token);
    /// 生成新令牌并持久化，返回新值
    QString regenerateWsToken();

public:
    static inline constexpr auto kDefaultAutoStart    = false;
    static inline constexpr auto kDefaultShowComplete = true;

    static QString getAppDataDir();
    static QString getDefaultSaveDir();
    static QString getDatabasePath();
};

} // namespace edm
