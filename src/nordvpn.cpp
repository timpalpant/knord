#include "nordvpn.h"

#include "clirunner.h"
#include "countryflags.h"
#include "locationmodel.h"
#include "nordparse.h"

#include <KLocalizedString>

#include <QRegularExpression>
#include <QTimer>

namespace {

constexpr int kPollActiveMs = 2000;
constexpr int kPollIdleMs = 10000;
constexpr int kConnectTimeoutMs = 90000;
//! `nordvpn login` waits for the browser round trip, so give it a few minutes.
constexpr int kLoginTimeoutMs = 300000;
//! `nordvpn account` is the only reliable sign-in probe, so it gets its own,
//! much lazier cadence -- tightened while waiting on a browser sign-in.
constexpr int kAccountPollIdleMs = 60000;
constexpr int kAccountPollLoginMs = 2000;

/*! Pull the Nord Account sign-in URL out of the login command's chatter. */
QString extractLoginUrl(const QString &output)
{
    static const QRegularExpression re(QStringLiteral("https://\\S*nord\\S*"));
    const auto match = re.match(output);
    if (!match.hasMatch()) {
        return {};
    }
    QString url = match.captured(0);
    // Trailing punctuation from the surrounding sentence is not part of the URL.
    while (!url.isEmpty() && (url.endsWith(u'.') || url.endsWith(u',') || url.endsWith(u')'))) {
        url.chop(1);
    }
    return url;
}

/*! Settings the CLI prints, mapped to the `nordvpn set` subcommand that
 *  changes them. Anything not listed here is shown read-only. */
struct SettingSpec {
    const char *label;   //!< as printed by `nordvpn settings`
    const char *key;     //!< canonical key exposed to QML
    const char *command; //!< `nordvpn set <command>`, empty when read-only
};

const QList<SettingSpec> &settingSpecs()
{
    static const QList<SettingSpec> specs = {
        {"Technology", "technology", "technology"},
        {"Protocol", "protocol", "protocol"},
        {"Firewall", "firewall", "firewall"},
        {"Firewall Mark", "fwmark", "fwmark"},
        {"Routing", "routing", "routing"},
        {"Kill Switch", "killswitch", "killswitch"},
        {"Threat Protection Lite", "threatprotectionlite", "threatprotectionlite"},
        {"Notify", "notify", "notify"},
        {"Tray", "tray", "tray"},
        {"Auto-connect", "autoconnect", "autoconnect"},
        {"Meshnet", "meshnet", "meshnet"},
        {"LAN Discovery", "lan-discovery", "lan-discovery"},
        {"Virtual Location", "virtual-location", "virtual-location"},
        {"Post-quantum VPN", "post-quantum", "post-quantum"},
        {"ARP Ignore", "arp-ignore", "arp-ignore"},
        {"Analytics", "analytics", "analytics"},
        {"Obfuscate", "obfuscate", "obfuscate"},
        {"User Consent", "userconsent", ""},
        {"DNS", "dns", ""}, // set through setDns(), which takes a list
    };
    return specs;
}

QString commandForKey(const QString &key)
{
    for (const SettingSpec &spec : settingSpecs()) {
        if (key == QLatin1String(spec.key)) {
            return QString::fromLatin1(spec.command);
        }
    }
    return {};
}

} // namespace

NordVpn::NordVpn(QObject *parent)
    : QObject(parent)
    , m_cli(new CliRunner(this))
    , m_authCli(new CliRunner(this))
    , m_pollTimer(new QTimer(this))
    , m_uptimeTimer(new QTimer(this))
    , m_accountTimer(new QTimer(this))
    , m_loginDeadline(new QTimer(this))
    , m_countries(new LocationModel(this))
    , m_cities(new LocationModel(this))
    , m_groups(new LocationModel(this))
{
    connect(m_cli, &CliRunner::busyChanged, this, &NordVpn::busyChanged);

    m_pollTimer->setInterval(kPollActiveMs);
    connect(m_pollTimer, &QTimer::timeout, this, &NordVpn::refreshStatus);
    m_pollTimer->start();

    // Uptime advances locally between polls so the label ticks smoothly.
    m_uptimeTimer->setInterval(1000);
    connect(m_uptimeTimer, &QTimer::timeout, this, [this] {
        if (m_state == Connected) {
            ++m_uptimeSeconds;
            Q_EMIT uptimeChanged();
        }
    });
    m_uptimeTimer->start();

    m_accountTimer->setInterval(kAccountPollIdleMs);
    connect(m_accountTimer, &QTimer::timeout, this, &NordVpn::refreshAccount);
    m_accountTimer->start();

    m_loginDeadline->setSingleShot(true);
    connect(m_loginDeadline, &QTimer::timeout, this, [this] { finishLogin(false, i18n("Signing in timed out. Please try again.")); });

    refreshStatus();
    refreshSettings();
    refreshLocations();
    refreshAccount();
}

NordVpn::~NordVpn() = default;

NordVpn::State NordVpn::state() const
{
    return m_state;
}

QString NordVpn::stateLabel() const
{
    switch (m_state) {
    case Connected:
        return i18n("Protected");
    case Connecting:
        return i18n("Connecting…");
    case Disconnecting:
        return i18n("Disconnecting…");
    case Disconnected:
        return i18n("Not protected");
    case Unknown:
        break;
    }
    return i18n("Checking…");
}

bool NordVpn::isConnected() const
{
    return m_state == Connected;
}

QString NordVpn::server() const
{
    return m_server;
}

QString NordVpn::hostname() const
{
    return m_hostname;
}

QString NordVpn::ip() const
{
    return m_ip;
}

QString NordVpn::country() const
{
    return m_country;
}

QString NordVpn::countryFlag() const
{
    return CountryFlags::flagFor(m_country);
}

QString NordVpn::city() const
{
    return m_city;
}

QString NordVpn::technology() const
{
    return m_technology;
}

QString NordVpn::protocol() const
{
    return m_protocol;
}

QString NordVpn::transferReceived() const
{
    return m_transferReceived;
}

QString NordVpn::transferSent() const
{
    return m_transferSent;
}

qint64 NordVpn::uptimeSeconds() const
{
    return m_uptimeSeconds;
}

QString NordVpn::uptimeLabel() const
{
    if (m_state != Connected || m_uptimeSeconds <= 0) {
        return {};
    }

    const qint64 days = m_uptimeSeconds / 86400;
    const qint64 hours = (m_uptimeSeconds % 86400) / 3600;
    const qint64 minutes = (m_uptimeSeconds % 3600) / 60;
    const qint64 seconds = m_uptimeSeconds % 60;

    if (days > 0) {
        return i18nc("@info uptime", "%1d %2h %3m", days, hours, minutes);
    }
    if (hours > 0) {
        return i18nc("@info uptime", "%1h %2m", hours, minutes);
    }
    if (minutes > 0) {
        return i18nc("@info uptime", "%1m %2s", minutes, seconds);
    }
    return i18nc("@info uptime", "%1s", seconds);
}

bool NordVpn::isBusy() const
{
    return !m_pendingAction.isEmpty();
}

QString NordVpn::pendingAction() const
{
    return m_pendingAction;
}

QString NordVpn::lastError() const
{
    return m_lastError;
}

bool NordVpn::daemonAvailable() const
{
    return m_daemonAvailable;
}

bool NordVpn::loggedIn() const
{
    return m_loggedIn;
}

QVariantMap NordVpn::settings() const
{
    return m_settings;
}

QVariantMap NordVpn::account() const
{
    return m_account;
}

QStringList NordVpn::allowlist() const
{
    return m_allowlist;
}

LocationModel *NordVpn::countries() const
{
    return m_countries;
}

LocationModel *NordVpn::cities() const
{
    return m_cities;
}

LocationModel *NordVpn::groups() const
{
    return m_groups;
}

QString NordVpn::citiesCountry() const
{
    return m_citiesCountry;
}

void NordVpn::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT statusChanged();
    Q_EMIT uptimeChanged();
}

void NordVpn::setError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    Q_EMIT lastErrorChanged();
}

void NordVpn::clearError()
{
    setError({});
}

void NordVpn::setDaemonAvailable(bool available)
{
    if (m_daemonAvailable == available) {
        return;
    }
    m_daemonAvailable = available;
    Q_EMIT availabilityChanged();
}

void NordVpn::setLoggedIn(bool loggedIn)
{
    if (m_loggedIn == loggedIn) {
        return;
    }
    m_loggedIn = loggedIn;
    Q_EMIT availabilityChanged();

    // A status poll seeing an account again is the authoritative sign that the
    // browser round trip landed, so wrap up the login regardless of whether the
    // `nordvpn login` process has noticed yet.
    if (loggedIn && m_loginInProgress) {
        finishLogin(true, {});
        refreshSettings();
        refreshLocations();
    }
}

void NordVpn::finishLogin(bool success, const QString &error)
{
    m_authCli->cancelAll();
    m_loginDeadline->stop();
    m_accountTimer->setInterval(kAccountPollIdleMs);

    const bool wasInProgress = m_loginInProgress;
    m_loginInProgress = false;
    m_loginUrl.clear();
    Q_EMIT loginChanged();

    if (!error.isEmpty()) {
        setError(error);
    }
    if (success && wasInProgress) {
        Q_EMIT loginSucceeded();
    }
}

bool NordVpn::loginInProgress() const
{
    return m_loginInProgress;
}

QString NordVpn::loginUrl() const
{
    return m_loginUrl;
}

void NordVpn::beginLogin()
{
    if (m_loginInProgress) {
        return;
    }
    m_loginInProgress = true;
    m_loginUrl.clear();
    clearError();
    Q_EMIT loginChanged();

    m_accountTimer->setInterval(kAccountPollLoginMs);
    m_loginDeadline->start(kLoginTimeoutMs);

    m_authCli->enqueueStreaming(
        {QStringLiteral("login")},
        [this](const CliResult &result) {
            // The command only hands out the URL and exits; the sign-in itself
            // completes out of band when the browser redirects to nordvpn://,
            // so stay in the waiting state and let account polling decide.
            const bool alreadyIn = result.output.contains(QLatin1String("already logged in"), Qt::CaseInsensitive);
            if (alreadyIn) {
                refreshAccount();
                return;
            }
            if (!result.ok) {
                finishLogin(false, NordParse::errorSummary(result.output.isEmpty() ? result.failure : result.output));
                return;
            }
            refreshAccount();
        },
        [this](const QString &soFar) {
            const QString url = extractLoginUrl(soFar);
            if (url.isEmpty() || url == m_loginUrl) {
                return;
            }
            m_loginUrl = url;
            Q_EMIT loginChanged();
            Q_EMIT loginUrlReady(url);
        },
        kLoginTimeoutMs);
}

void NordVpn::completeLoginWithCallback(const QString &callbackUrl)
{
    const QString url = callbackUrl.trimmed();
    if (url.isEmpty()) {
        return;
    }

    // Stop the pending `nordvpn login`; this call finishes the handshake.
    m_authCli->cancelAll();
    m_loginInProgress = true;
    Q_EMIT loginChanged();
    clearError();

    // Unlike --token, the CLI takes the callback only as an argument, so this
    // one is briefly visible in /proc. It carries a single-use code that the
    // daemon redeems immediately, rather than the long lived access token.
    m_authCli->enqueue(
        {QStringLiteral("login"), QStringLiteral("--callback"), url},
        [this](const CliResult &result) {
            if (!result.ok) {
                finishLogin(false, NordParse::errorSummary(result.output.isEmpty() ? result.failure : result.output));
            }
            // On success the account poll flips loggedIn and finishes the flow.
            refreshStatus();
            refreshAccount();
        },
        60000);
}

void NordVpn::loginWithToken(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_authCli->cancelAll();
    m_loginInProgress = true;
    Q_EMIT loginChanged();
    clearError();

    // Given no argument, `nordvpn login --token` prompts for the token on its
    // terminal. That is the only way in that keeps the token out of argv, where
    // /proc would hand it to every other process on the machine.
    m_authCli->enqueueWithSecret(
        {QStringLiteral("login"), QStringLiteral("--token")},
        trimmed,
        [this](const CliResult &result) {
            if (!result.ok) {
                finishLogin(false, NordParse::errorSummary(result.output.isEmpty() ? result.failure : result.output));
            }
            // On success the account poll flips loggedIn and finishes the flow.
            refreshStatus();
            refreshAccount();
        },
        60000);
}

void NordVpn::cancelLogin()
{
    if (!m_loginInProgress) {
        return;
    }
    finishLogin(false, {});
}

void NordVpn::logout(bool persistToken)
{
    QStringList args{QStringLiteral("logout")};
    if (persistToken) {
        args.append(QStringLiteral("--persist-token"));
    }
    runMutation(args, i18n("Signing out…"), i18n("Signed out"));
}

void NordVpn::setWindowVisible(bool visible)
{
    m_windowVisible = visible;
    m_pollTimer->setInterval(visible ? kPollActiveMs : kPollIdleMs);
    if (visible) {
        refreshStatus();
    }
}

bool NordVpn::absorbCommonFailures(const CliResult &result)
{
    if (!result.ok && result.output.isEmpty() && !result.failure.isEmpty() && result.failure.contains(QLatin1String("nordvpn command"))) {
        setDaemonAvailable(false);
        setError(result.failure);
        setState(Unknown);
        return true;
    }

    if (NordParse::isDaemonUnreachable(result.output)) {
        setDaemonAvailable(false);
        setLoggedIn(true); // unknowable while the daemon is down
        setState(Unknown);
        setError(i18n("Cannot reach the NordVPN daemon. Try: sudo systemctl start nordvpnd"));
        return true;
    }
    setDaemonAvailable(true);

    if (NordParse::isLoggedOut(result.output)) {
        setLoggedIn(false);
        setState(Disconnected);
        return true;
    }

    // Deliberately no setLoggedIn(true) here: `nordvpn status` reports plain
    // "Disconnected" while signed out and never mentions the account, so the
    // absence of the marker proves nothing. Only `nordvpn account` succeeding
    // does, which applyAccount() handles.
    return false;
}

void NordVpn::refreshStatus()
{
    m_cli->enqueue(
        {QStringLiteral("status")},
        [this](const CliResult &result) {
            if (absorbCommonFailures(result)) {
                return;
            }
            if (!result.ok) {
                setError(NordParse::errorSummary(result.failure));
                return;
            }
            applyStatus(result.output);
        },
        10000,
        QStringLiteral("status"));
}

void NordVpn::applyStatus(const QString &output)
{
    const NordParse::Document doc = NordParse::parse(output);
    const QString status = doc.value(QStringLiteral("Status")).toLower();

    State newState = Unknown;
    if (status.contains(QLatin1String("connected")) && !status.contains(QLatin1String("disconnected"))) {
        newState = Connected;
    } else if (status.contains(QLatin1String("disconnected"))) {
        newState = Disconnected;
    } else if (status.contains(QLatin1String("connecting"))) {
        newState = Connecting;
    }

    m_server = doc.value(QStringLiteral("Server"));
    m_hostname = doc.value(QStringLiteral("Hostname"));
    m_ip = doc.value(QStringLiteral("IP"));
    m_country = doc.value(QStringLiteral("Country"));
    m_city = doc.value(QStringLiteral("City"));
    m_technology = doc.value(QStringLiteral("Current technology"));
    m_protocol = doc.value(QStringLiteral("Current protocol"));

    m_transferReceived.clear();
    m_transferSent.clear();
    NordParse::parseTransfer(doc.value(QStringLiteral("Transfer")), &m_transferReceived, &m_transferSent);

    const qint64 uptime = NordParse::parseUptime(doc.value(QStringLiteral("Uptime")));
    // Only take the daemon's value when it actually moved; otherwise the local
    // per-second tick would be repeatedly yanked back by stale poll results.
    if (std::llabs(uptime - m_uptimeSeconds) > 2) {
        m_uptimeSeconds = uptime;
    }

    if (newState != Connected) {
        m_uptimeSeconds = 0;
    }

    setState(newState);
    Q_EMIT statusChanged();
    Q_EMIT uptimeChanged();
}

void NordVpn::refreshSettings()
{
    m_cli->enqueue(
        {QStringLiteral("settings")},
        [this](const CliResult &result) {
            if (absorbCommonFailures(result) || !result.ok) {
                return;
            }
            applySettings(result.output);
        },
        10000,
        QStringLiteral("settings"));
}

void NordVpn::applySettings(const QString &output)
{
    const NordParse::Document doc = NordParse::parse(output);

    QVariantMap map;
    for (const SettingSpec &spec : settingSpecs()) {
        const QString label = QString::fromLatin1(spec.label);
        if (!doc.contains(label)) {
            continue;
        }
        const QString raw = doc.value(label);
        const QString key = QString::fromLatin1(spec.key);

        if (NordParse::isBoolLike(raw)) {
            map.insert(key, NordParse::toBool(raw));
        } else {
            map.insert(key, raw);
        }
    }

    // DNS is either a comma separated list or "disabled".
    const QString dns = doc.value(QStringLiteral("DNS"));
    if (NordParse::isBoolLike(dns)) {
        map.insert(QStringLiteral("dns"), QStringList{});
    } else if (!dns.isEmpty()) {
        QStringList servers = dns.split(u',', Qt::SkipEmptyParts);
        for (QString &server : servers) {
            server = server.trimmed();
        }
        map.insert(QStringLiteral("dns"), servers);
    }

    QStringList allowlist;
    for (auto it = doc.blocks.cbegin(); it != doc.blocks.cend(); ++it) {
        if (!it.key().startsWith(QLatin1String("Allowlisted"))) {
            continue;
        }
        // "Allowlisted ports" -> "ports"
        const QString kind = it.key().section(u' ', 1);
        for (const QString &entry : it.value()) {
            allowlist.append(QStringLiteral("%1: %2").arg(kind, entry));
        }
    }

    if (m_settings != map || m_allowlist != allowlist) {
        m_settings = map;
        m_allowlist = allowlist;
        Q_EMIT settingsChanged();
    }
}

void NordVpn::refreshAccount()
{
    m_cli->enqueue(
        {QStringLiteral("account")},
        [this](const CliResult &result) {
            if (absorbCommonFailures(result) || !result.ok) {
                return;
            }
            applyAccount(result.output);
        },
        15000,
        QStringLiteral("account"));
}

void NordVpn::applyAccount(const QString &output)
{
    const NordParse::Document doc = NordParse::parse(output);

    QVariantMap map;
    const auto take = [&](const char *label, const char *key) {
        const QString value = doc.value(QString::fromLatin1(label));
        if (!value.isEmpty()) {
            map.insert(QString::fromLatin1(key), value);
        }
    };

    take("Email address", "email");
    take("Account created", "created");
    take("Subscription", "subscription");
    take("Dedicated IP", "dedicatedIp");
    take("Dedicated Server", "dedicatedServer");
    take("Multi-factor authentication (MFA)", "mfa");

    // Reading account details back is the authoritative sign-in check.
    if (!map.isEmpty()) {
        setLoggedIn(true);
    }

    if (m_account != map) {
        m_account = map;
        Q_EMIT accountChanged();
    }
}

void NordVpn::refreshLocations()
{
    m_cli->enqueue(
        {QStringLiteral("countries")},
        [this](const CliResult &result) {
            if (absorbCommonFailures(result) || !result.ok) {
                return;
            }
            QList<LocationModel::Entry> entries;
            const QStringList names = result.output.split(u'\n', Qt::SkipEmptyParts);
            entries.reserve(names.size());
            for (const QString &raw : names) {
                const QString name = raw.trimmed();
                if (name.isEmpty()) {
                    continue;
                }
                entries.append(LocationModel::Entry{
                    name,
                    CountryFlags::prettify(name),
                    CountryFlags::flagFor(name),
                    {name},
                    true,
                });
            }
            m_countries->setEntries(entries);
        },
        20000,
        QStringLiteral("countries"));

    m_cli->enqueue(
        {QStringLiteral("groups")},
        [this](const CliResult &result) {
            if (absorbCommonFailures(result) || !result.ok) {
                return;
            }
            QList<LocationModel::Entry> entries;
            const QStringList names = result.output.split(u'\n', Qt::SkipEmptyParts);
            for (const QString &raw : names) {
                const QString name = raw.trimmed();
                if (name.isEmpty()) {
                    continue;
                }
                entries.append(LocationModel::Entry{
                    name,
                    CountryFlags::prettify(name),
                    {},
                    {QStringLiteral("--group"), name},
                    false,
                });
            }
            m_groups->setEntries(entries);
        },
        20000,
        QStringLiteral("groups"));
}

void NordVpn::loadCities(const QString &country)
{
    if (m_citiesCountry != country) {
        m_citiesCountry = country;
        m_cities->clear();
        m_cities->setFilter({});
        Q_EMIT citiesCountryChanged();
    }

    m_cli->enqueue(
        {QStringLiteral("cities"), country},
        [this, country](const CliResult &result) {
            if (absorbCommonFailures(result) || !result.ok) {
                return;
            }
            if (m_citiesCountry != country) {
                return; // the user moved on
            }
            QList<LocationModel::Entry> entries;
            const QStringList names = result.output.split(u'\n', Qt::SkipEmptyParts);
            for (const QString &raw : names) {
                const QString name = raw.trimmed();
                if (name.isEmpty()) {
                    continue;
                }
                entries.append(LocationModel::Entry{
                    name,
                    CountryFlags::prettify(name),
                    {},
                    {country, name},
                    false,
                });
            }
            m_cities->setEntries(entries);
        },
        20000,
        QStringLiteral("cities"));
}

void NordVpn::runMutation(const QStringList &args, const QString &action, const QString &successMessage)
{
    m_pendingAction = action;
    Q_EMIT busyChanged();
    clearError();

    m_cli->enqueue(
        args,
        [this, successMessage](const CliResult &result) {
            m_pendingAction.clear();
            Q_EMIT busyChanged();

            const bool commonFailure = absorbCommonFailures(result);

            if (!commonFailure && !result.ok) {
                const QString message = NordParse::errorSummary(result.output.isEmpty() ? result.failure : result.output);
                setError(message);
                Q_EMIT actionCompleted(message, false);
            } else if (!commonFailure && !successMessage.isEmpty()) {
                Q_EMIT actionCompleted(successMessage, true);
            }

            // The command's own output is not authoritative; re-read the real
            // state instead of trying to parse connect/set chatter.
            refreshStatus();
            refreshSettings();
        },
        kConnectTimeoutMs);
}

void NordVpn::quickConnect()
{
    setState(Connecting);
    runMutation({QStringLiteral("connect")}, i18n("Connecting…"), i18n("Connected"));
}

void NordVpn::connectTo(const QStringList &args, const QString &label)
{
    setState(Connecting);
    const QString target = label.isEmpty() ? args.join(u' ') : label;
    runMutation(QStringList{QStringLiteral("connect")} + args, i18n("Connecting to %1…", target), i18n("Connected to %1", target));
}

void NordVpn::disconnectVpn()
{
    setState(Disconnecting);
    runMutation({QStringLiteral("disconnect")}, i18n("Disconnecting…"), i18n("Disconnected"));
}

void NordVpn::setSetting(const QString &key, const QVariant &value)
{
    const QString command = commandForKey(key);
    if (command.isEmpty()) {
        setError(i18n("The setting “%1” cannot be changed from here.", key));
        return;
    }

    QString argument;
    if (value.typeId() == QMetaType::Bool) {
        argument = value.toBool() ? QStringLiteral("on") : QStringLiteral("off");
    } else {
        argument = value.toString();
    }
    if (argument.isEmpty()) {
        return;
    }

    runMutation({QStringLiteral("set"), command, argument}, i18n("Applying…"), {});
}

void NordVpn::setDns(const QStringList &servers)
{
    QStringList args{QStringLiteral("set"), QStringLiteral("dns")};
    if (servers.isEmpty()) {
        args.append(QStringLiteral("off"));
    } else {
        args.append(servers);
    }
    runMutation(args, i18n("Applying DNS…"), {});
}

void NordVpn::allowlistAddPort(int port, const QString &protocol)
{
    QStringList args{QStringLiteral("allowlist"), QStringLiteral("add"), QStringLiteral("port"), QString::number(port)};
    if (!protocol.isEmpty()) {
        args << QStringLiteral("protocol") << protocol;
    }
    runMutation(args, i18n("Updating allowlist…"), {});
}

void NordVpn::allowlistAddPortRange(int from, int to, const QString &protocol)
{
    QStringList args{QStringLiteral("allowlist"), QStringLiteral("add"), QStringLiteral("ports"), QString::number(from), QString::number(to)};
    if (!protocol.isEmpty()) {
        args << QStringLiteral("protocol") << protocol;
    }
    runMutation(args, i18n("Updating allowlist…"), {});
}

void NordVpn::allowlistAddSubnet(const QString &subnet)
{
    runMutation({QStringLiteral("allowlist"), QStringLiteral("add"), QStringLiteral("subnet"), subnet}, i18n("Updating allowlist…"), {});
}

void NordVpn::allowlistRemove(const QStringList &args)
{
    runMutation(QStringList{QStringLiteral("allowlist"), QStringLiteral("remove")} + args, i18n("Updating allowlist…"), {});
}

void NordVpn::allowlistRemoveAll()
{
    runMutation({QStringLiteral("allowlist"), QStringLiteral("remove"), QStringLiteral("all")}, i18n("Clearing allowlist…"), {});
}
