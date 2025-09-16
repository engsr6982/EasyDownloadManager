#include "EdmGlobalConfig.h"

#include "EasyDownloadManager.h"
#include "ui/main/MainWindow.h"
#include "utils/StringUtils.h"

#include <qdir.h>
#include <qmessagebox.h>
#include <qstandardpaths.h>

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
constexpr auto kProxyType      = "proxy/type";                // int
constexpr auto kProxyHost      = "proxy/host";                // string
constexpr auto kProxyPort      = "proxy/port";                // int
constexpr auto kProxyUser      = "proxy/user";                // string
constexpr auto kProxyPassword  = "proxy/password";            // string


int EdmGlobalConfig::getThreadCount() const {
    return settings_.value(kThreadCount, static_cast<int>(kDefaultThreadCount)).toInt();
}

void EdmGlobalConfig::setThreadCount(int count) {
    settings_.setValue(kThreadCount, count);
    settings_.sync();
}

unsigned int EdmGlobalConfig::getBandwidthLimit() const {
    return settings_.value(kBandwidthLimit, kDefaultBandwidthLimit).toInt();
}

void EdmGlobalConfig::setBandwidthLimit(unsigned int limit) {
    settings_.setValue(kBandwidthLimit, limit);
    settings_.sync();
}

QString EdmGlobalConfig::getUserAgent() const { return settings_.value(kUserAgent, kDefaultUserAgent).toString(); }

void EdmGlobalConfig::setUserAgent(QString const& userAgent) {
    settings_.setValue(kUserAgent, userAgent);
    settings_.sync();
}

proxy_utils::ProxyConfig EdmGlobalConfig::getProxyConfig() const {
    proxy_utils::ProxyConfig cfg{};
    cfg.type_ =
        static_cast<proxy_utils::Type>(settings_.value(kProxyType, static_cast<int>(proxy_utils::Type::None)).toInt());
    if (cfg.type_ == proxy_utils::Type::None) {
        return cfg;
    }
    cfg.host_     = settings_.value(kProxyHost, QString{}).toString();
    cfg.port_     = settings_.value(kProxyPort, 0).toInt();
    cfg.user_     = settings_.value(kProxyUser, QString{}).toString();
    cfg.password_ = settings_.value(kProxyPassword, QString{}).toString();
    return cfg;
}
void EdmGlobalConfig::setProxyConfig(proxy_utils::ProxyConfig const& config) {
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

QString EdmGlobalConfig::getSaveDir() const { return settings_.value(kSaveDir, getDefaultSaveDir()).toString(); }

void EdmGlobalConfig::setSaveDir(QString const& dir) {
    settings_.setValue(kSaveDir, dir);
    settings_.sync();
}

QString EdmGlobalConfig::getTempDir() const { return settings_.value(kTempDir, getDefaultTempDir()).toString(); }

void EdmGlobalConfig::setTempDir(QString const& dir) {
    settings_.setValue(kTempDir, dir);
    settings_.sync();
}

bool EdmGlobalConfig::canAutoStart() const { return settings_.value(kAutoStart, kDefaultAutoStart).toBool(); }

void EdmGlobalConfig::setAutoStart(bool autoStart) {
    settings_.setValue(kAutoStart, autoStart);
    settings_.sync();
}

bool EdmGlobalConfig::canShowDownloadCompleteDialog() const {
    return settings_.value(kShowComplete, kDefaultShowComplete).toBool();
}

void EdmGlobalConfig::setShowDownloadCompleteDialog(bool show) {
    settings_.setValue(kShowComplete, show);
    settings_.sync();
}


QString EdmGlobalConfig::getDefaultSaveDir() {
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}
QString EdmGlobalConfig::getDefaultTempDir() {
    QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/edm";
    QDir    dir;
    if (!dir.exists(temp)) {
        if (!dir.mkpath(temp)) {
            auto msgBox = new QMessageBox(
                QMessageBox::Warning,
                "Error",
                "Failed to create temp directory",
                QMessageBox::Ok,
                EasyDownloadManager::getOrNewInstance().getMainWindow()
            );
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->show();
        }
    }
    return temp;
}


} // namespace edm