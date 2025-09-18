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
    int  getThreadCount() const;
    void setThreadCount(int count);

    // 带宽
    BandWidthLimit getBandwidthLimit() const;
    void           setBandwidthLimit(BandWidthLimit limit);

    // User-Agent
    QString getUserAgent() const;
    void    setUserAgent(QString const& userAgent);

    // 代理
    [[nodiscard]] proxy_utils::ProxyConfig getProxyConfig() const;

    void setProxyConfig(proxy_utils::ProxyConfig const& config);

    inline bool canUseProxy() const { return getProxyConfig().type_ != ProxyType::None; }

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
    static inline constexpr auto kDefaultAutoStart    = false;
    static inline constexpr auto kDefaultShowComplete = true;

    static QString getDefaultSaveDir();
    static QString getDefaultTempDir();
};

} // namespace edm
