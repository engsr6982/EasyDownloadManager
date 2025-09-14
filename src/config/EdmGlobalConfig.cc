#include "EdmGlobalConfig.h"

#include "utils/StringUtils.h"

#include <qdir.h>

#include <magic_enum/magic_enum.hpp>

namespace edm {

EdmGlobalConfig::EdmGlobalConfig() : settings_{"engsr6982", "edm"} {}

EdmGlobalConfig::~EdmGlobalConfig() = default;

EdmGlobalConfig& EdmGlobalConfig::instance() {
    static EdmGlobalConfig instance_;
    return instance_;
}

constexpr auto kThreadCount    = "downloader/threadCount";    // int
constexpr auto kBandwidthLimit = "downloader/bandwidthLimit"; // int
constexpr auto kUserAgent      = "downloader/userAgent";      // string
constexpr auto kSaveDir        = "downloader/saveDir";        // string
constexpr auto kTempDir        = "downloader/tempDir";        // string
constexpr auto kAutoStart      = "downloader/autoStart";      // bool
constexpr auto kShowComplete   = "downloader/showComplete";   // bool
constexpr auto kProxyType      = "proxy/type";                // string
constexpr auto kProxyHost      = "proxy/host";                // string
constexpr auto kProxyPort      = "proxy/port";                // int
constexpr auto kProxyUser      = "proxy/user";                // string
constexpr auto kProxyPassword  = "proxy/password";            // string


int EdmGlobalConfig::getThreadCount() const { return settings_.value(kThreadCount, 8).toInt(); }

void EdmGlobalConfig::setThreadCount(int count) {
    settings_.setValue(kThreadCount, count);
    settings_.sync();
}

unsigned int EdmGlobalConfig::getBandwidthLimit() const { return settings_.value(kBandwidthLimit, 0).toInt(); }

void EdmGlobalConfig::setBandwidthLimit(unsigned int limit) {
    settings_.setValue(kBandwidthLimit, limit);
    settings_.sync();
}

QString EdmGlobalConfig::getUserAgent() const {
    return settings_
        .value(
            kUserAgent,
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 "
            "Safari/537.36 Edg/140.0.0.0"
        )
        .toString();
}

void EdmGlobalConfig::setUserAgent(QString const& userAgent) {
    settings_.setValue(kUserAgent, userAgent);
    settings_.sync();
}


inline constexpr auto        kProxyTypeNone = magic_enum::enum_name(EdmGlobalConfig::ProxyType::None);
EdmGlobalConfig::ProxyConfig EdmGlobalConfig::getProxyConfig() const {
    auto const typeStr = settings_.value(kProxyType, string_utils::stringview2qstring(kProxyTypeNone)).toString();

    ProxyConfig cfg{};
    cfg.type_ = magic_enum::enum_cast<ProxyType>(string_utils::qstring2stringview(typeStr)).value_or(ProxyType::None);
    if (cfg.type_ == ProxyType::None) {
        return cfg;
    }
    cfg.host_     = settings_.value(kProxyHost, QString{}).toString();
    cfg.port_     = settings_.value(kProxyPort, 0).toInt();
    cfg.user_     = settings_.value(kProxyUser, QString{}).toString();
    cfg.password_ = settings_.value(kProxyPassword, QString{}).toString();
    return cfg;
}
void EdmGlobalConfig::setProxyConfig(ProxyConfig const& config) {
    settings_.setValue(kProxyType, string_utils::stringview2qstring(magic_enum::enum_name(config.type_)));
    settings_.setValue(kProxyHost, config.host_.value_or(QString{}));
    settings_.setValue(kProxyPort, config.port_.value_or(0));
    settings_.setValue(kProxyUser, config.user_.value_or(QString{}));
    settings_.setValue(kProxyPassword, config.password_.value_or(QString{}));
    settings_.sync();
}

QString EdmGlobalConfig::getSaveDir() const {
    return settings_.value(kSaveDir, QDir::homePath() + "/Downloads").toString();
}

void EdmGlobalConfig::setSaveDir(QString const& dir) {
    settings_.setValue(kSaveDir, dir);
    settings_.sync();
}

QString EdmGlobalConfig::getTempDir() const { return settings_.value(kTempDir, QDir::tempPath()).toString(); }

void EdmGlobalConfig::setTempDir(QString const& dir) {
    settings_.setValue(kTempDir, dir);
    settings_.sync();
}

bool EdmGlobalConfig::canAutoStart() const { return settings_.value(kAutoStart, false).toBool(); }

void EdmGlobalConfig::setAutoStart(bool autoStart) {
    settings_.setValue(kAutoStart, autoStart);
    settings_.sync();
}

bool EdmGlobalConfig::canShowDownloadCompleteDialog() const { return settings_.value(kShowComplete, true).toBool(); }

void EdmGlobalConfig::setShowDownloadCompleteDialog(bool show) {
    settings_.setValue(kShowComplete, show);
    settings_.sync();
}


} // namespace edm