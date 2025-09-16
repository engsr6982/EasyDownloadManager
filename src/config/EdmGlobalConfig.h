#pragma once
#include "utils/ProxyUtils.h"


#include <magic_enum/magic_enum.hpp>
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
    enum class AvailableThreads { k4 = 4, k8 = 8, k16 = 16, k32 = 32 };
    int  getThreadCount() const;
    void setThreadCount(int count);

    // 带宽
    unsigned int getBandwidthLimit() const;
    void         setBandwidthLimit(unsigned int limit);

    // User-Agent
    QString getUserAgent() const;
    void    setUserAgent(QString const& userAgent);

    // 代理
    [[nodiscard]] proxy_utils::ProxyConfig getProxyConfig() const;

    void setProxyConfig(proxy_utils::ProxyConfig const& config);

    inline bool canUseProxy() const { return getProxyConfig().type_ != proxy_utils::Type::None; }

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


public:
    static inline constexpr auto kDefaultThreadCount    = AvailableThreads::k8;
    static inline constexpr auto kDefaultBandwidthLimit = 0;
    static inline constexpr auto kDefaultUserAgent    = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                                        "(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0";
    static inline constexpr auto kDefaultProxyType    = proxy_utils::Type::None;
    static inline constexpr auto kDefaultProxyHost    = "127.0.0.1";
    static inline constexpr auto kDefaultProxyPort    = 1080;
    static inline constexpr auto kDefaultAutoStart    = false;
    static inline constexpr auto kDefaultShowComplete = true;

    static QString getDefaultSaveDir();
    static QString getDefaultTempDir();
};

} // namespace edm
