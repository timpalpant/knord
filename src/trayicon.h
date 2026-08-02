#pragma once

#include <QObject>
#include <QPointer>

class NordVpn;
class KStatusNotifierItem;
class QAction;

/*!
 * System tray entry: connection state at a glance plus connect/disconnect
 * without raising the window.
 *
 * The KStatusNotifierItem is created and destroyed with setEnabled() rather
 * than merely hidden, so that turning the tray off really does remove the item
 * from the host's status area.
 */
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(NordVpn *backend, QObject *parent = nullptr);

    bool isEnabled() const;
    void setEnabled(bool enabled);

Q_SIGNALS:
    void activateRequested();

private:
    void createItem();
    void updateFromState();

    NordVpn *m_backend = nullptr;
    QPointer<KStatusNotifierItem> m_item;
    QPointer<QAction> m_toggleAction;
    bool m_enabled = false;
};
