#include "clirunner.h"
#include "countryflags.h"
#include "nordparse.h"

#include <QTest>

/*!
 * The CLI's output format is the whole contract this app is built on, so these
 * fixtures are verbatim captures from `nordvpn` 5.2.0.
 */
class TestNordParse : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesConnectedStatus();
    void parsesDisconnectedStatus();
    void ignoresProseAndUrls();
    void parsesIndentedBlocks();
    void parsesSettings();
    void parsesBooleans();
    void parsesUptime_data();
    void parsesUptime();
    void parsesTransfer();
    void parsesTransferRejectsGarbage();
    void detectsDaemonDown();
    void detectsLoggedOut();
    void summarisesErrors();
    void sanitizesAnsiAndSpinners();
    void sanitizePreservesIndentation();
    void mapsCountryFlags_data();
    void mapsCountryFlags();
    void prettifiesNames();
};

void TestNordParse::parsesConnectedStatus()
{
    const QString output = QStringLiteral("Status: Connected\n"
                                          "Server: United States #6915\n"
                                          "Hostname: us6915.nordvpn.com\n"
                                          "IP: 84.17.35.246\n"
                                          "Country: United States\n"
                                          "City: New York\n"
                                          "Current technology: NORDLYNX\n"
                                          "Current protocol: UDP\n"
                                          "Post-quantum VPN: Disabled\n"
                                          "Transfer: 507.05 GiB received, 17.07 GiB sent\n"
                                          "Uptime: 20 hours 19 minutes 44 seconds");

    const auto doc = NordParse::parse(output);
    QCOMPARE(doc.value(QStringLiteral("Status")), QStringLiteral("Connected"));
    QCOMPARE(doc.value(QStringLiteral("Server")), QStringLiteral("United States #6915"));
    QCOMPARE(doc.value(QStringLiteral("Hostname")), QStringLiteral("us6915.nordvpn.com"));
    QCOMPARE(doc.value(QStringLiteral("IP")), QStringLiteral("84.17.35.246"));
    QCOMPARE(doc.value(QStringLiteral("City")), QStringLiteral("New York"));
    QCOMPARE(doc.value(QStringLiteral("Current technology")), QStringLiteral("NORDLYNX"));
}

void TestNordParse::parsesDisconnectedStatus()
{
    // Signed out or merely disconnected both look like this: note that nothing
    // in the output reveals whether an account is attached.
    const auto doc = NordParse::parse(QStringLiteral("Status: Disconnected"));
    QCOMPARE(doc.value(QStringLiteral("Status")), QStringLiteral("Disconnected"));
    QVERIFY(!doc.contains(QStringLiteral("Server")));
    QVERIFY(!NordParse::isLoggedOut(QStringLiteral("Status: Disconnected")));
}

void TestNordParse::ignoresProseAndUrls()
{
    // The account command appends footer links; the scheme colon must not be
    // mistaken for a key/value separator.
    const QString output = QStringLiteral("Email address: someone@example.com\n"
                                          "Terms of Service - https://my.nordaccount.com/legal/terms-of-service/?utm_medium=app");

    const auto doc = NordParse::parse(output);
    QCOMPARE(doc.value(QStringLiteral("Email address")), QStringLiteral("someone@example.com"));
    QCOMPARE(doc.values.size(), 1);
}

void TestNordParse::parsesIndentedBlocks()
{
    const QString output = QStringLiteral("Firewall: enabled\n"
                                          "Allowlisted ports:\n"
                                          "       22 (TCP)\n"
                                          "       3000 - 5000 (UDP)\n"
                                          "Allowlisted subnets:\n"
                                          "       192.168.1.0/24");

    const auto doc = NordParse::parse(output);
    QCOMPARE(doc.blocks.value(QStringLiteral("Allowlisted ports")).size(), 2);
    QCOMPARE(doc.blocks.value(QStringLiteral("Allowlisted ports")).at(0), QStringLiteral("22 (TCP)"));
    QCOMPARE(doc.blocks.value(QStringLiteral("Allowlisted ports")).at(1), QStringLiteral("3000 - 5000 (UDP)"));
    QCOMPARE(doc.blocks.value(QStringLiteral("Allowlisted subnets")).at(0), QStringLiteral("192.168.1.0/24"));
}

void TestNordParse::parsesSettings()
{
    const QString output = QStringLiteral("Technology: NORDLYNX\n"
                                          "Firewall: enabled\n"
                                          "Firewall Mark: 0xe1f1\n"
                                          "Kill Switch: disabled\n"
                                          "DNS: 94.140.14.14, 94.140.15.15, 8.8.8.8");

    const auto doc = NordParse::parse(output);
    QCOMPARE(doc.value(QStringLiteral("Technology")), QStringLiteral("NORDLYNX"));
    QVERIFY(NordParse::toBool(doc.value(QStringLiteral("Firewall"))));
    QVERIFY(!NordParse::toBool(doc.value(QStringLiteral("Kill Switch"))));
    // A hex mark must not be mistaken for a boolean.
    QVERIFY(!NordParse::isBoolLike(doc.value(QStringLiteral("Firewall Mark"))));
    QCOMPARE(doc.value(QStringLiteral("DNS")).split(u',').size(), 3);
}

void TestNordParse::parsesBooleans()
{
    QVERIFY(NordParse::toBool(QStringLiteral("enabled")));
    QVERIFY(NordParse::toBool(QStringLiteral("on")));
    QVERIFY(NordParse::toBool(QStringLiteral("  Enabled  ")));
    QVERIFY(!NordParse::toBool(QStringLiteral("disabled")));
    QVERIFY(!NordParse::toBool(QStringLiteral("off")));

    QVERIFY(NordParse::isBoolLike(QStringLiteral("Disabled")));
    QVERIFY(!NordParse::isBoolLike(QStringLiteral("NORDLYNX")));

    // Unknown values fall back rather than silently reading as false.
    QVERIFY(NordParse::toBool(QStringLiteral("wat"), true));
    QVERIFY(!NordParse::toBool(QStringLiteral("wat"), false));
}

void TestNordParse::parsesUptime_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<qint64>("expected");

    QTest::newRow("seconds") << QStringLiteral("44 seconds") << qint64(44);
    QTest::newRow("minutes") << QStringLiteral("3 minutes 5 seconds") << qint64(185);
    QTest::newRow("hours") << QStringLiteral("20 hours 19 minutes 44 seconds") << qint64(73184);
    QTest::newRow("days") << QStringLiteral("2 days 1 hour 1 minute 1 second") << qint64(176461);
    QTest::newRow("singular") << QStringLiteral("1 minute 1 second") << qint64(61);
    QTest::newRow("empty") << QString() << qint64(0);
}

void TestNordParse::parsesUptime()
{
    QFETCH(QString, input);
    QFETCH(qint64, expected);
    QCOMPARE(NordParse::parseUptime(input), expected);
}

void TestNordParse::parsesTransfer()
{
    QString rx;
    QString tx;
    NordParse::parseTransfer(QStringLiteral("507.05 GiB received, 17.07 GiB sent"), &rx, &tx);
    QCOMPARE(rx, QStringLiteral("507.05 GiB"));
    QCOMPARE(tx, QStringLiteral("17.07 GiB"));

    NordParse::parseTransfer(QStringLiteral("0.54 TiB received, 18.01 GiB sent"), &rx, &tx);
    QCOMPARE(rx, QStringLiteral("0.54 TiB"));
    QCOMPARE(tx, QStringLiteral("18.01 GiB"));
}

void TestNordParse::parsesTransferRejectsGarbage()
{
    QString rx = QStringLiteral("untouched");
    QString tx = QStringLiteral("untouched");
    NordParse::parseTransfer(QStringLiteral("nonsense"), &rx, &tx);
    QCOMPARE(rx, QStringLiteral("untouched"));
    QCOMPARE(tx, QStringLiteral("untouched"));

    // Must not crash on null out-params.
    NordParse::parseTransfer(QStringLiteral("1 GiB received, 2 GiB sent"), nullptr, nullptr);
}

void TestNordParse::detectsDaemonDown()
{
    QVERIFY(NordParse::isDaemonUnreachable(QStringLiteral("Whoops! Cannot reach System Daemon.")));
    QVERIFY(!NordParse::isDaemonUnreachable(QStringLiteral("Status: Connected")));
}

void TestNordParse::detectsLoggedOut()
{
    QVERIFY(NordParse::isLoggedOut(QStringLiteral("You're not logged in.")));
    QVERIFY(NordParse::isLoggedOut(QStringLiteral("You are not logged in.")));
    QVERIFY(!NordParse::isLoggedOut(QStringLiteral("You're already logged in.")));
}

void TestNordParse::summarisesErrors()
{
    const QString output = QStringLiteral("https://nordvpn.com/some-tracking-link\n"
                                          "The specified server does not exist.");
    QCOMPARE(NordParse::errorSummary(output), QStringLiteral("The specified server does not exist."));
}

void TestNordParse::sanitizesAnsiAndSpinners()
{
    // Colour codes plus a spinner that redraws the line with carriage returns.
    const QString raw = QStringLiteral("\x1B[32mConnecting\x1B[0m\n"
                                       "|\rConnecting to United States #1\r\x1B[KConnected to United States #1\n");

    const QString clean = CliRunner::sanitize(raw);
    QVERIFY(!clean.contains(u'\x1B'));
    QVERIFY(!clean.contains(u'\r'));
    QVERIFY(clean.contains(QStringLiteral("Connected to United States #1")));
    QVERIFY(!clean.contains(QStringLiteral("Connecting to United States #1")));
}

void TestNordParse::sanitizePreservesIndentation()
{
    // The allowlist block depends on leading whitespace surviving.
    const QString clean = CliRunner::sanitize(QStringLiteral("Allowlisted ports:\n       22 (TCP)  \n"));
    const auto doc = NordParse::parse(clean);
    QCOMPARE(doc.blocks.value(QStringLiteral("Allowlisted ports")).at(0), QStringLiteral("22 (TCP)"));
}

void TestNordParse::mapsCountryFlags_data()
{
    QTest::addColumn<QString>("country");
    QTest::addColumn<QString>("flag");

    QTest::newRow("us") << QStringLiteral("United_States") << QStringLiteral("🇺🇸");
    QTest::newRow("gb") << QStringLiteral("United_Kingdom") << QStringLiteral("🇬🇧");
    QTest::newRow("cz") << QStringLiteral("Czech_Republic") << QStringLiteral("🇨🇿");
    QTest::newRow("ba") << QStringLiteral("Bosnia_And_Herzegovina") << QStringLiteral("🇧🇦");
    QTest::newRow("kr") << QStringLiteral("South_Korea") << QStringLiteral("🇰🇷");
    QTest::newRow("jp") << QStringLiteral("Japan") << QStringLiteral("🇯🇵");
    QTest::newRow("de") << QStringLiteral("Germany") << QStringLiteral("🇩🇪");
    QTest::newRow("tr") << QStringLiteral("Turkey") << QStringLiteral("🇹🇷");
    QTest::newRow("hk") << QStringLiteral("Hong_Kong") << QStringLiteral("🇭🇰");
}

void TestNordParse::mapsCountryFlags()
{
    QFETCH(QString, country);
    QFETCH(QString, flag);
    QCOMPARE(CountryFlags::flagFor(country), flag);
}

void TestNordParse::prettifiesNames()
{
    QCOMPARE(CountryFlags::prettify(QStringLiteral("United_States")), QStringLiteral("United States"));
    QCOMPARE(CountryFlags::prettify(QStringLiteral("Onion_Over_VPN")), QStringLiteral("Onion Over VPN"));

    // An unknown place is not a hard error, it just gets no flag.
    QVERIFY(CountryFlags::flagFor(QStringLiteral("Atlantis")).isEmpty());
    QVERIFY(CountryFlags::flagFor(QString()).isEmpty());
}

QTEST_GUILESS_MAIN(TestNordParse)
#include "tst_nordparse.moc"
