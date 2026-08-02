#include "trayicon.h"

#include "nordvpn.h"

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include <QAction>
#include <QMenu>

TrayIcon::TrayIcon(NordVpn *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    connect(m_backend, &NordVpn::statusChanged, this, &TrayIcon::updateFromState);
    connect(m_backend, &NordVpn::busyChanged, this, &TrayIcon::updateFromState);
    connect(m_backend, &NordVpn::availabilityChanged, this, &TrayIcon::updateFromState);
}

bool TrayIcon::isEnabled() const
{
    return m_enabled;
}

void TrayIcon::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    if (enabled) {
        createItem();
        updateFromState();
    } else if (m_item) {
        delete m_item;
        m_item = nullptr;
    }
}

void TrayIcon::createItem()
{
    m_item = new KStatusNotifierItem(QStringLiteral("knord"), this);
    m_item->setCategory(KStatusNotifierItem::SystemServices);
    m_item->setTitle(i18n("NordVPN"));
    m_item->setStandardActionsEnabled(true);

    auto *menu = new QMenu();

    m_toggleAction = menu->addAction(QString());
    connect(m_toggleAction, &QAction::triggered, this, [this] {
        if (m_backend->isConnected()) {
            m_backend->disconnectVpn();
        } else {
            m_backend->quickConnect();
        }
    });

    menu->addSeparator();
    m_item->setContextMenu(menu);

    connect(m_item, &KStatusNotifierItem::activateRequested, this, [this] { Q_EMIT activateRequested(); });
}

void TrayIcon::updateFromState()
{
    if (!m_item) {
        return;
    }

    const bool connected = m_backend->isConnected();
    const bool busy = m_backend->isBusy();

    m_item->setIconByName(connected ? QStringLiteral("network-vpn") : QStringLiteral("network-disconnect"));
    m_item->setStatus(connected ? KStatusNotifierItem::Active : KStatusNotifierItem::Passive);

    if (connected) {
        const QString where = m_backend->city().isEmpty() ? m_backend->country()
                                                          : i18nc("@info:tooltip city, country", "%1, %2", m_backend->city(), m_backend->country());
        m_item->setToolTip(QStringLiteral("network-vpn"), i18n("NordVPN: connected"), where);
    } else {
        m_item->setToolTip(QStringLiteral("network-disconnect"), i18n("NordVPN: disconnected"), m_backend->stateLabel());
    }

    if (m_toggleAction) {
        m_toggleAction->setText(connected ? i18n("Disconnect") : i18n("Quick Connect"));
        m_toggleAction->setEnabled(!busy && m_backend->daemonAvailable() && m_backend->loggedIn());
    }
}
