#include "EdmConfig.h"

#include <qdir.h>
#include <qmessagebox.h>
#include <qstandardpaths.h>

#include <magic_enum/magic_enum.hpp>

#include "Global.h"

namespace edm {

std::string ProxyConfig::toProxyUrl() const {
    if (isEnabled()) return {};

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
    // The socks4 series protocol does not support authentication.
    if (user_ && !user_->isEmpty() && !isSocks4Series()) {
        oss << user_->toStdString();
        if (password_ && !password_->isEmpty()) {
            oss << ":" << password_->toStdString();
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

constexpr auto kThreadCount        = "downloader/threadCount";         // int
constexpr auto kBandwidthLimit     = "downloader/bandwidthLimit";      // int
constexpr auto kUserAgent          = "downloader/userAgent";           // string
constexpr auto kSaveDir            = "downloader/saveDir";             // string
constexpr auto kAutoStart          = "downloader/autoStart";           // bool
constexpr auto kStatusNotification = "downloader/status_notification"; // bool
constexpr auto kProxyEnabled       = "proxy/enabled";                  // bool
constexpr auto kProxyType          = "proxy/type";                     // int
constexpr auto kProxyHost          = "proxy/host";                     // string
constexpr auto kProxyPort          = "proxy/port";                     // int
constexpr auto kProxyUser          = "proxy/user";                     // string
constexpr auto kProxyPassword      = "proxy/password";                 // string


int EdmConfig::getThreadCount() const {
    return settings_.value(kThreadCount, static_cast<int>(kDefaultThreadCount)).toInt();
}

void EdmConfig::setThreadCount(int count) {
    settings_.setValue(kThreadCount, count);
    settings_.sync();
}

BandLimit EdmConfig::getBandwidthLimit() const { return settings_.value(kBandwidthLimit, kDefaultBandLimit).toInt(); }

void EdmConfig::setBandwidthLimit(BandLimit limit) {
    settings_.setValue(kBandwidthLimit, limit);
    settings_.sync();
}

QString EdmConfig::getUserAgent() const { return settings_.value(kUserAgent, kDefaultUserAgent).toString(); }

void EdmConfig::setUserAgent(QString const& userAgent) {
    settings_.setValue(kUserAgent, userAgent);
    settings_.sync();
}

ProxyConfig EdmConfig::getProxyConfig() const {
    ProxyConfig cfg{};
    cfg.enabled_  = settings_.value(kProxyEnabled, kDefaultProxyEnabled).toBool();
    cfg.type_     = static_cast<ProxyType>(settings_.value(kProxyEnabled, static_cast<int>(kDefaultProxyType)).toInt());
    cfg.host_     = settings_.value(kProxyHost, kDefaultProxyHost).toString();
    cfg.port_     = settings_.value(kProxyPort, kDefaultProxyPort).toInt();
    cfg.user_     = settings_.value(kProxyUser, QString{}).toString();
    cfg.password_ = settings_.value(kProxyPassword, QString{}).toString();
    return cfg;
}
void EdmConfig::setProxyConfig(ProxyConfig const& config) {
    settings_.setValue(kProxyEnabled, config.isEnabled());
    settings_.setValue(kProxyType, static_cast<int>(config.type_));
    settings_.setValue(kProxyHost, config.host_.value_or(kDefaultProxyHost));
    settings_.setValue(kProxyPort, config.port_.value_or(kDefaultProxyPort));
    settings_.setValue(kProxyUser, config.user_.value_or(QString{}));
    settings_.setValue(kProxyPassword, config.password_.value_or(QString{}));
    settings_.sync();
}
bool EdmConfig::getProxyEnabled() const { return settings_.value(kProxyEnabled, kDefaultProxyEnabled).toBool(); }
void EdmConfig::setProxyEnabled(bool enabled) {
    settings_.setValue(kProxyEnabled, enabled);
    settings_.sync();
}
ProxyType EdmConfig::getProxyType() const {
    return static_cast<ProxyType>(settings_.value(kProxyType, static_cast<int>(kDefaultProxyType)).toInt());
}
void EdmConfig::setProxyType(ProxyType type) {
    settings_.setValue(kProxyType, static_cast<int>(type));
    settings_.sync();
}
QString EdmConfig::getProxyHost() const { return settings_.value(kProxyHost, kDefaultProxyHost).toString(); }
void    EdmConfig::setProxyHost(QString const& host) {
    settings_.setValue(kProxyHost, host);
    settings_.sync();
}
int  EdmConfig::getProxyPort() const { return settings_.value(kProxyPort, kDefaultProxyPort).toInt(); }
void EdmConfig::setProxyPort(int port) {
    settings_.setValue(kProxyPort, port);
    settings_.sync();
}
QString EdmConfig::getProxyUser() const { return settings_.value(kProxyUser, QString{}).toString(); }
void    EdmConfig::setProxyUser(QString const& user) {
    settings_.setValue(kProxyUser, user);
    settings_.sync();
}
QString EdmConfig::getProxyPassword() const { return settings_.value(kProxyPassword, QString{}).toString(); }
void    EdmConfig::setProxyPassword(QString const& password) {
    settings_.setValue(kProxyPassword, password);
    settings_.sync();
}

QString EdmConfig::getSaveDir() const { return settings_.value(kSaveDir, getDefaultSaveDir()).toString(); }

void EdmConfig::setSaveDir(QString const& dir) {
    settings_.setValue(kSaveDir, dir);
    settings_.sync();
}

bool EdmConfig::canAutoStart() const { return settings_.value(kAutoStart, kDefaultAutoStart).toBool(); }

void EdmConfig::setAutoStart(bool autoStart) {
    settings_.setValue(kAutoStart, autoStart);
    settings_.sync();
}

bool EdmConfig::canShowStatusNotification() const {
    return settings_.value(kStatusNotification, kDefaultShowComplete).toBool();
}

void EdmConfig::setShowStatusNotification(bool show) {
    settings_.setValue(kStatusNotification, show);
    settings_.sync();
}


QString EdmConfig::getAppDataDir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    (void)QDir().mkpath(path);
    return path;
}
QString EdmConfig::getDefaultSaveDir() { return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation); }
QString EdmConfig::getDatabasePath() { return getAppDataDir() + "/downloads.db"; }


} // namespace edm