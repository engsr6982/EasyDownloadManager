#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qcontainerfwd.h>
#include <qtmetamacros.h>

#include "Global.h"

namespace edm {

/**
 * @brief QML-friendly wrapper around EdmConfig.
 *
 * Exposes every QSettings-backed setting as a Q_PROPERTY with NOTIFY signals so
 * QML controls can two-way bind. Setters delegate to EdmConfig and emit the
 * appropriate NOTIFY so bound controls update.
 */
class QmlSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int threadCountIndex READ threadCountIndex WRITE setThreadCountIndex NOTIFY threadCountIndexChanged)
    Q_PROPERTY(int bandwidthLimit READ bandwidthLimit WRITE setBandwidthLimit NOTIFY bandwidthLimitChanged)
    Q_PROPERTY(QString userAgent READ userAgent WRITE setUserAgent NOTIFY userAgentChanged)
    Q_PROPERTY(QString saveDir READ saveDir WRITE setSaveDir NOTIFY saveDirChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(
        bool showStatusNotification READ showStatusNotification WRITE setShowStatusNotification NOTIFY
            showStatusNotificationChanged
    )

    Q_PROPERTY(bool proxyEnabled READ proxyEnabled WRITE setProxyEnabled NOTIFY proxyEnabledChanged)
    Q_PROPERTY(int proxyTypeIndex READ proxyTypeIndex WRITE setProxyTypeIndex NOTIFY proxyTypeIndexChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY proxyHostChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxyPortChanged)
    Q_PROPERTY(QString proxyUser READ proxyUser WRITE setProxyUser NOTIFY proxyUserChanged)
    Q_PROPERTY(QString proxyPassword READ proxyPassword WRITE setProxyPassword NOTIFY proxyPasswordChanged)

    // WebSocket server (browser integration)
    Q_PROPERTY(bool wsEnabled READ wsEnabled WRITE setWsEnabled NOTIFY wsEnabledChanged)
    Q_PROPERTY(int wsPort READ wsPort WRITE setWsPort NOTIFY wsPortChanged)
    Q_PROPERTY(bool wsAllowLan READ wsAllowLan WRITE setWsAllowLan NOTIFY wsAllowLanChanged)
    Q_PROPERTY(QString wsToken READ wsToken WRITE setWsToken NOTIFY wsTokenChanged)

    // constants
    Q_PROPERTY(QVector<QString> proxyTypes READ proxyTypes CONSTANT)
    Q_PROPERTY(QVector<int> threadCounts READ threadCounts CONSTANT)

public:
    explicit QmlSettings(QObject* parent = nullptr);
    ~QmlSettings() override = default;

    QVector<int> threadCounts() const;
    int          threadCountIndex() const;
    void         setThreadCountIndex(int idx);

    int  bandwidthLimit() const;
    void setBandwidthLimit(int limit);

    QString userAgent() const;
    void    setUserAgent(QString const& ua);

    QString saveDir() const;
    void    setSaveDir(QString const& dir);

    bool autoStart() const;
    void setAutoStart(bool enabled);

    bool showStatusNotification() const;
    void setShowStatusNotification(bool show);

    // Proxy
    bool proxyEnabled() const;
    void setProxyEnabled(bool enabled);

    QVector<QString> proxyTypes() const;

    int  proxyTypeIndex() const; // 1=Http,2=Https,3=Socks4,4=Socks4a,5=Socks5,6=Socks5h
    void setProxyTypeIndex(int idx);

    QString proxyHost() const;
    void    setProxyHost(QString const& host);

    int  proxyPort() const;
    void setProxyPort(int port);

    QString proxyUser() const;
    void    setProxyUser(QString const& user);

    QString proxyPassword() const;
    void    setProxyPassword(QString const& pwd);

    // WebSocket server
    bool wsEnabled() const;
    void setWsEnabled(bool enabled);

    int  wsPort() const;
    void setWsPort(int port);

    bool wsAllowLan() const;
    void setWsAllowLan(bool allow);

    QString wsToken();
    void    setWsToken(QString const& token);

    /// Generate and persist a new token; all existing connections are dropped
    Q_INVOKABLE QString regenerateWsToken();

    Q_INVOKABLE QString defaultUserAgent() const;

    // Force a full reload from EdmConfig -> re-emit every NOTIFY.
    Q_INVOKABLE void reload();

signals:
    void threadCountIndexChanged();
    void bandwidthLimitChanged();
    void userAgentChanged();
    void saveDirChanged();
    void autoStartChanged();
    void showStatusNotificationChanged();

    void proxyEnabledChanged();
    void proxyTypeIndexChanged();
    void proxyHostChanged();
    void proxyPortChanged();
    void proxyUserChanged();
    void proxyPasswordChanged();

    void wsEnabledChanged();
    void wsPortChanged();
    void wsAllowLanChanged();
    void wsTokenChanged();

private:
    int       threadIndexFromCount(int count) const;
    int       threadCountFromIndex(int index) const;
    int       proxyIndexFromType(ProxyType type) const;
    ProxyType proxyTypeFromIndex(int index) const;
};

} // namespace edm
