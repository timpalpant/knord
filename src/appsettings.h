#pragma once

#include <QObject>
#include <QQmlEngine>

/*!
 * KNord's own preferences, stored in ~/.config/knordrc.
 *
 * Distinct from the NordVPN daemon's settings, which live in the NordVpn
 * singleton and are read back from `nordvpn settings`.
 */
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool trayEnabled READ trayEnabled WRITE setTrayEnabled NOTIFY trayEnabledChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    bool trayEnabled() const;
    void setTrayEnabled(bool enabled);

Q_SIGNALS:
    void trayEnabledChanged();

private:
    bool m_trayEnabled = true;
};
