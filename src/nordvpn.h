#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>

class CliRunner;
class LocationModel;
class QTimer;
struct CliResult;

/*!
 * Application-wide facade over the `nordvpn` command line client.
 *
 * Connection status is modelled with explicit typed properties because the UI
 * branches on it heavily. Settings and account details are exposed as maps
 * instead: they are long, mostly homogeneous, and NordVPN adds new keys with
 * some regularity, so a map keeps this class from needing an edit every time.
 */
class NordVpn : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY statusChanged)
    Q_PROPERTY(QString server READ server NOTIFY statusChanged)
    Q_PROPERTY(QString hostname READ hostname NOTIFY statusChanged)
    Q_PROPERTY(QString ip READ ip NOTIFY statusChanged)
    Q_PROPERTY(QString country READ country NOTIFY statusChanged)
    Q_PROPERTY(QString countryFlag READ countryFlag NOTIFY statusChanged)
    Q_PROPERTY(QString city READ city NOTIFY statusChanged)
    Q_PROPERTY(QString technology READ technology NOTIFY statusChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY statusChanged)
    Q_PROPERTY(QString transferReceived READ transferReceived NOTIFY statusChanged)
    Q_PROPERTY(QString transferSent READ transferSent NOTIFY statusChanged)
    Q_PROPERTY(qint64 uptimeSeconds READ uptimeSeconds NOTIFY uptimeChanged)
    Q_PROPERTY(QString uptimeLabel READ uptimeLabel NOTIFY uptimeChanged)

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool daemonAvailable READ daemonAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY availabilityChanged)

    Q_PROPERTY(bool loginInProgress READ loginInProgress NOTIFY loginChanged)
    Q_PROPERTY(QString loginUrl READ loginUrl NOTIFY loginChanged)

    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap account READ account NOTIFY accountChanged)
    Q_PROPERTY(QStringList allowlist READ allowlist NOTIFY settingsChanged)

    Q_PROPERTY(LocationModel *countries READ countries CONSTANT)
    Q_PROPERTY(LocationModel *cities READ cities CONSTANT)
    Q_PROPERTY(LocationModel *groups READ groups CONSTANT)
    Q_PROPERTY(QString citiesCountry READ citiesCountry NOTIFY citiesCountryChanged)

public:
    enum State {
        Unknown,
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
    };
    Q_ENUM(State)

    explicit NordVpn(QObject *parent = nullptr);
    ~NordVpn() override;

    State state() const;
    QString stateLabel() const;
    bool isConnected() const;
    QString server() const;
    QString hostname() const;
    QString ip() const;
    QString country() const;
    QString countryFlag() const;
    QString city() const;
    QString technology() const;
    QString protocol() const;
    QString transferReceived() const;
    QString transferSent() const;
    qint64 uptimeSeconds() const;
    QString uptimeLabel() const;

    bool isBusy() const;
    QString pendingAction() const;
    QString lastError() const;
    bool daemonAvailable() const;
    bool loggedIn() const;

    QVariantMap settings() const;
    QVariantMap account() const;
    QStringList allowlist() const;

    LocationModel *countries() const;
    LocationModel *cities() const;
    LocationModel *groups() const;
    QString citiesCountry() const;

    //! Connect to the recommended server.
    Q_INVOKABLE void quickConnect();
    //! Connect using an explicit argument tail, e.g. {"United_States", "New_York"}.
    Q_INVOKABLE void connectTo(const QStringList &args, const QString &label = {});
    Q_INVOKABLE void disconnectVpn();

    Q_INVOKABLE void refreshStatus();
    Q_INVOKABLE void refreshSettings();
    Q_INVOKABLE void refreshAccount();
    Q_INVOKABLE void refreshLocations();
    Q_INVOKABLE void loadCities(const QString &country);

    /*! \a key is a canonical settings key such as "killswitch". Accepts bool
     *  values for toggles and strings for technology/protocol/fwmark. */
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    //! \a servers is a list of up to 3 IPv4 addresses; empty disables custom DNS.
    Q_INVOKABLE void setDns(const QStringList &servers);

    Q_INVOKABLE void allowlistAddPort(int port, const QString &protocol = {});
    Q_INVOKABLE void allowlistAddPortRange(int from, int to, const QString &protocol = {});
    Q_INVOKABLE void allowlistAddSubnet(const QString &subnet);
    Q_INVOKABLE void allowlistRemove(const QStringList &args);
    Q_INVOKABLE void allowlistRemoveAll();

    /*! Starts `nordvpn login`. The CLI prints a URL and waits; opening that URL
     *  and finishing in the browser hands control back through the registered
     *  nordvpn:// handler. Emits loginUrlReady() as soon as the URL appears. */
    Q_INVOKABLE void beginLogin();
    /*! Fallback when the browser cannot hand back automatically: the URL behind
     *  the browser's "Continue" button, passed to `nordvpn login --callback`. */
    Q_INVOKABLE void completeLoginWithCallback(const QString &callbackUrl);
    //! Log in with an access token from Nord Account. Does not support MFA.
    Q_INVOKABLE void loginWithToken(const QString &token);
    Q_INVOKABLE void cancelLogin();
    /*! Logs out, which also disconnects. \a persistToken keeps the current
     *  access token usable instead of revoking it. */
    Q_INVOKABLE void logout(bool persistToken = false);

    bool loginInProgress() const;
    QString loginUrl() const;

    Q_INVOKABLE void clearError();
    /*! Slow polling down while the window is hidden; the tray still needs
     *  updates, just not every two seconds. */
    Q_INVOKABLE void setWindowVisible(bool visible);

Q_SIGNALS:
    void statusChanged();
    void uptimeChanged();
    void busyChanged();
    void lastErrorChanged();
    void availabilityChanged();
    void settingsChanged();
    void accountChanged();
    void citiesCountryChanged();
    void loginChanged();
    void loginUrlReady(const QString &url);
    void loginSucceeded();
    //! Emitted when a connect/disconnect finishes, for notifications.
    void actionCompleted(const QString &message, bool success);

private:
    void applyStatus(const QString &output);
    void applySettings(const QString &output);
    void applyAccount(const QString &output);
    void setState(State state);
    void setError(const QString &message);
    void setDaemonAvailable(bool available);
    void setLoggedIn(bool loggedIn);
    void runMutation(const QStringList &args, const QString &action, const QString &successMessage);
    //! Handles the daemon/login checks common to every command's output.
    bool absorbCommonFailures(const CliResult &result);
    //! Leaves the waiting-for-browser state, restoring the lazy poll cadence.
    void finishLogin(bool success, const QString &error);
    void schedulePoll();

    CliRunner *m_cli = nullptr;
    /*! Login runs on its own runner: `nordvpn login` blocks until the browser
     *  round trip finishes, and it must not stall status polling meanwhile. */
    CliRunner *m_authCli = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_uptimeTimer = nullptr;
    QTimer *m_accountTimer = nullptr;
    QTimer *m_loginDeadline = nullptr;

    State m_state = Unknown;
    QString m_server;
    QString m_hostname;
    QString m_ip;
    QString m_country;
    QString m_city;
    QString m_technology;
    QString m_protocol;
    QString m_transferReceived;
    QString m_transferSent;
    qint64 m_uptimeSeconds = 0;

    QString m_pendingAction;
    QString m_lastError;
    bool m_daemonAvailable = true;
    bool m_loggedIn = true;
    bool m_windowVisible = true;
    bool m_loginInProgress = false;
    QString m_loginUrl;

    QVariantMap m_settings;
    QVariantMap m_account;
    QStringList m_allowlist;

    LocationModel *m_countries = nullptr;
    LocationModel *m_cities = nullptr;
    LocationModel *m_groups = nullptr;
    QString m_citiesCountry;
};
