#include "QmlSettings.h"

#include "EdmConfig.h"
#include "Global.h"

#include <magic_enum/magic_enum.hpp>

namespace edm {

QmlSettings::QmlSettings(QObject* parent) : QObject(parent) {}

int QmlSettings::threadIndexFromCount(int count) const {
    auto idx = magic_enum::enum_index(static_cast<AvailableThreads>(count));
    return static_cast<int>(idx.value_or(0)); // default to 0 -> k4
}
int QmlSettings::threadCountFromIndex(int index) const {
    auto values = magic_enum::enum_values<AvailableThreads>();
    if (index < 0 || index >= static_cast<int>(values.size())) {
        return static_cast<int>(AvailableThreads::k8); // out of range, default to k8
    }
    return static_cast<int>(values[index]);
}

int QmlSettings::proxyIndexFromType(ProxyType type) const {
    auto idx = magic_enum::enum_index(type);
    return static_cast<int>(idx.value_or(0)); // default to 0 -> Http
}
ProxyType QmlSettings::proxyTypeFromIndex(int index) const {
    auto values = magic_enum::enum_values<ProxyType>();
    if (index < 0 || index >= static_cast<int>(values.size())) {
        return ProxyType::Http; // out of range, default to Http
    }
    return values[index];
}

QVector<int> QmlSettings::threadCounts() const {
    QVector<int> counts;
    for (auto i : magic_enum::enum_values<AvailableThreads>()) {
        counts.append(static_cast<int>(i));
    }
    return counts;
}
int  QmlSettings::threadCountIndex() const { return threadIndexFromCount(EdmConfig::getInstance().getThreadCount()); }
void QmlSettings::setThreadCountIndex(int idx) {
    int count = threadCountFromIndex(idx);
    if (EdmConfig::getInstance().getThreadCount() == count) return;
    EdmConfig::getInstance().setThreadCount(count);
    emit threadCountIndexChanged();
}

int QmlSettings::bandwidthLimit() const { return static_cast<int>(EdmConfig::getInstance().getBandwidthLimit()); }

void QmlSettings::setBandwidthLimit(int limit) {
    if (EdmConfig::getInstance().getBandwidthLimit() == limit) return;
    EdmConfig::getInstance().setBandwidthLimit(limit);
    emit bandwidthLimitChanged();
}

QString QmlSettings::userAgent() const { return EdmConfig::getInstance().getUserAgent(); }

void QmlSettings::setUserAgent(QString const& ua) {
    if (EdmConfig::getInstance().getUserAgent() == ua) return;
    EdmConfig::getInstance().setUserAgent(ua);
    emit userAgentChanged();
}

QString QmlSettings::saveDir() const { return EdmConfig::getInstance().getSaveDir(); }

void QmlSettings::setSaveDir(QString const& dir) {
    if (EdmConfig::getInstance().getSaveDir() == dir) return;
    EdmConfig::getInstance().setSaveDir(dir);
    emit saveDirChanged();
}

bool QmlSettings::autoStart() const { return EdmConfig::getInstance().canAutoStart(); }

void QmlSettings::setAutoStart(bool enabled) {
    if (EdmConfig::getInstance().canAutoStart() == enabled) return;
    EdmConfig::getInstance().setAutoStart(enabled);
    emit autoStartChanged();
}

bool QmlSettings::showStatusNotification() const { return EdmConfig::getInstance().canShowStatusNotification(); }
void QmlSettings::setShowStatusNotification(bool show) {
    if (EdmConfig::getInstance().canShowStatusNotification() == show) return;
    EdmConfig::getInstance().setShowStatusNotification(show);
    emit showStatusNotificationChanged();
}

bool QmlSettings::proxyEnabled() const { return EdmConfig::getInstance().getProxyEnabled(); }

void QmlSettings::setProxyEnabled(bool enabled) {
    EdmConfig::getInstance().setProxyEnabled(enabled);
    emit proxyEnabledChanged();
}

QVector<QString> QmlSettings::proxyTypes() const {
    QVector<QString> types;
    for (auto type : magic_enum::enum_names<ProxyType>()) {
        types.append(QString::fromStdString(std::string{type}));
    }
    return types;
}

int  QmlSettings::proxyTypeIndex() const { return proxyIndexFromType(EdmConfig::getInstance().getProxyType()); }
void QmlSettings::setProxyTypeIndex(int idx) {
    auto      oldType = EdmConfig::getInstance().getProxyType();
    ProxyType newType = proxyTypeFromIndex(idx);
    if (oldType == newType) return;
    EdmConfig::getInstance().setProxyType(newType);
    emit proxyTypeIndexChanged();
}

QString QmlSettings::proxyHost() const { return EdmConfig::getInstance().getProxyHost(); }
void    QmlSettings::setProxyHost(QString const& host) {
    EdmConfig::getInstance().setProxyHost(host);
    emit proxyHostChanged();
}

int  QmlSettings::proxyPort() const { return EdmConfig::getInstance().getProxyPort(); }
void QmlSettings::setProxyPort(int port) {
    EdmConfig::getInstance().setProxyPort(port);
    emit proxyPortChanged();
}

QString QmlSettings::proxyUser() const { return EdmConfig::getInstance().getProxyUser(); }
void    QmlSettings::setProxyUser(QString const& user) {
    EdmConfig::getInstance().setProxyUser(user);
    emit proxyUserChanged();
}

QString QmlSettings::proxyPassword() const { return EdmConfig::getInstance().getProxyPassword(); }
void    QmlSettings::setProxyPassword(QString const& pwd) {
    EdmConfig::getInstance().setProxyPassword(pwd);
    emit proxyPasswordChanged();
}

QString QmlSettings::defaultUserAgent() const { return QString::fromStdString(kDefaultUserAgent); }

void QmlSettings::reload() {
    emit threadCountIndexChanged();
    emit bandwidthLimitChanged();
    emit userAgentChanged();
    emit saveDirChanged();
    emit autoStartChanged();
    emit showStatusNotificationChanged();
    emit proxyEnabledChanged();
    emit proxyTypeIndexChanged();
    emit proxyHostChanged();
    emit proxyPortChanged();
    emit proxyUserChanged();
    emit proxyPasswordChanged();
}

} // namespace edm
