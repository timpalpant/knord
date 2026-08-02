#include "appsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace {

constexpr auto kGroup = "General";
constexpr auto kTrayKey = "TrayEnabled";

KConfigGroup configGroup()
{
    return KSharedConfig::openConfig()->group(QLatin1String(kGroup));
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    m_trayEnabled = configGroup().readEntry(kTrayKey, true);
}

bool AppSettings::trayEnabled() const
{
    return m_trayEnabled;
}

void AppSettings::setTrayEnabled(bool enabled)
{
    if (m_trayEnabled == enabled) {
        return;
    }
    m_trayEnabled = enabled;

    auto group = configGroup();
    group.writeEntry(kTrayKey, enabled);
    group.sync();

    Q_EMIT trayEnabledChanged();
}
