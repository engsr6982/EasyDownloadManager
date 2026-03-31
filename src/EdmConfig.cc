#include "EdmConfig.h"

#include <qdir.h>
#include <qmessagebox.h>
#include <qstandardpaths.h>

#include <magic_enum/magic_enum.hpp>

namespace edm {

std::string ProxyConfig::toProxyUrl() const {
    if (isNone()) return {};

    std::ostringstream oss;

    // 1. protocol
    switch (type_) {
    case ProxyType::Http:
        oss << "http://";
        break;
    case ProxyType::Https:
        oss << "https://";
        break;
    case ProxyType::Socks4:
        oss << "socks4://";
        break;
    case ProxyType::Socks4a:
        oss << "socks4a://";
        break;
    case ProxyType::Socks5:
        oss << "socks5://";
        break;
    case ProxyType::Socks5h:
        oss << "socks5h://";
        break;
    default:
        return {};
    }

    // 2. user/password
    if (user_ && !user_->isEmpty()) {
        oss << user_->toStdString();
        if (isSocks5Series() || isHttpSeries()) {
            if (password_ && !password_->isEmpty()) {
                oss << ":" << password_->toStdString();
            }
        }
        oss << "@";
    }

    // 3. host
    if (host_ && !host_->isEmpty()) {
        oss << host_->toStdString();
    } else {
        return {};
    }

    // 4. port
    if (port_ && *port_ > 0) {
        oss << ":" << *port_;
    }
    return oss.str();
}


EdmConfig::EdmConfig() : settings_{"engsr6982", "edm"} {}

EdmConfig::~EdmConfig() = default;

EdmConfig& EdmConfig::getInstance() {
    static EdmConfig instance_;
    return instance_;
}

constexpr auto kThreadCount    = "downloader/threadCount";    // int
constexpr auto kBandwidthLimit = "downloader/bandwidthLimit"; // int
constexpr auto kUserAgent      = "downloader/userAgent";      // string
constexpr auto kSaveDir        = "downloader/saveDir";        // string
constexpr auto kTempDir        = "downloader/tempDir";        // string
constexpr auto kAutoStart      = "downloader/autoStart";      // bool
constexpr auto kShowComplete   = "downloader/showComplete";   // bool
constexpr auto kProxyType      = "proxy/type";                // int
constexpr auto kProxyHost      = "proxy/host";                // string
constexpr auto kProxyPort      = "proxy/port";                // int
constexpr auto kProxyUser      = "proxy/user";                // string
constexpr auto kProxyPassword  = "proxy/password";            // string


int EdmConfig::getThreadCount() const {
    return settings_.value(kThreadCount, static_cast<int>(GlobalDefaults::kDefaultThreadCount)).toInt();
}

void EdmConfig::setThreadCount(int count) {
    settings_.setValue(kThreadCount, count);
    settings_.sync();
}

BandWidthLimit EdmConfig::getBandwidthLimit() const {
    return settings_.value(kBandwidthLimit, GlobalDefaults::kDefaultBandwidthLimit).toInt();
}

void EdmConfig::setBandwidthLimit(BandWidthLimit limit) {
    settings_.setValue(kBandwidthLimit, limit);
    settings_.sync();
}

QString EdmConfig::getUserAgent() const {
    return settings_.value(kUserAgent, GlobalDefaults::kDefaultUserAgent).toString();
}

void EdmConfig::setUserAgent(QString const& userAgent) {
    settings_.setValue(kUserAgent, userAgent);
    settings_.sync();
}

ProxyConfig EdmConfig::getProxyConfig() const {
    ProxyConfig cfg{};
    cfg.type_ = static_cast<ProxyType>(settings_.value(kProxyType, static_cast<int>(ProxyType::None)).toInt());
    if (cfg.type_ == ProxyType::None) {
        return cfg;
    }
    cfg.host_     = settings_.value(kProxyHost, QString{}).toString();
    cfg.port_     = settings_.value(kProxyPort, 0).toInt();
    cfg.user_     = settings_.value(kProxyUser, QString{}).toString();
    cfg.password_ = settings_.value(kProxyPassword, QString{}).toString();
    return cfg;
}
void EdmConfig::setProxyConfig(ProxyConfig const& config) {
    settings_.setValue(kProxyType, static_cast<int>(config.type_));
    settings_.setValue(kProxyHost, config.host_.value_or(QString{}));
    settings_.setValue(kProxyPort, config.port_.value_or(0));
    if (config.isSocks4Series()) {
        settings_.setValue(kProxyUser, QString{});
        settings_.setValue(kProxyPassword, QString{});
    } else {
        settings_.setValue(kProxyUser, config.user_.value_or(QString{}));
        settings_.setValue(kProxyPassword, config.password_.value_or(QString{}));
    }
    settings_.sync();
}

QString EdmConfig::getSaveDir() const { return settings_.value(kSaveDir, getDefaultSaveDir()).toString(); }

void EdmConfig::setSaveDir(QString const& dir) {
    settings_.setValue(kSaveDir, dir);
    settings_.sync();
}

QString EdmConfig::getTempDir() const { return settings_.value(kTempDir, getDefaultTempDir()).toString(); }

void EdmConfig::setTempDir(QString const& dir) {
    settings_.setValue(kTempDir, dir);
    settings_.sync();
}

bool EdmConfig::canAutoStart() const { return settings_.value(kAutoStart, kDefaultAutoStart).toBool(); }

void EdmConfig::setAutoStart(bool autoStart) {
    settings_.setValue(kAutoStart, autoStart);
    settings_.sync();
}

bool EdmConfig::canShowDownloadCompleteDialog() const {
    return settings_.value(kShowComplete, kDefaultShowComplete).toBool();
}

void EdmConfig::setShowDownloadCompleteDialog(bool show) {
    settings_.setValue(kShowComplete, show);
    settings_.sync();
}


QString EdmConfig::getAppDataDir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/edm";
    (void)QDir().mkpath(path);
    return path;
}
QString EdmConfig::getDefaultSaveDir() { return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation); }
QString EdmConfig::getDefaultTempDir() {
    QString path = getAppDataDir() + "/tasks";
    (void)QDir().mkpath(path);
    return path;
}
QString EdmConfig::getDatabasePath() { return getAppDataDir() + "/downloads.db"; }


} // namespace edm