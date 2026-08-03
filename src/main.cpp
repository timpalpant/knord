#include "appsettings.h"
#include "nordvpn.h"
#include "trayicon.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QWindow>

int main(int argc, char *argv[])
{
    // QApplication rather than QGuiApplication: KStatusNotifierItem's context
    // menu is a QMenu, which needs the widgets stack.
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("knord"));

    KAboutData about(
        QStringLiteral("knord"), i18n("KNord"), QStringLiteral("0.1.1"), i18n("A native Plasma front end for NordVPN"), KAboutLicense::GPL_V3);
    about.setDesktopFileName(QStringLiteral("io.github.timpalpant.knord"));
    KAboutData::setApplicationData(about);

    if (QQuickStyle::name().isEmpty()) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("network-vpn")));

    QQmlApplicationEngine engine;
    // Makes i18n()/i18nc() available to every QML file in the module.
    KLocalization::setupLocalizedContext(&engine);

    engine.loadFromModule("io.github.timpalpant.knord", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning("knord: failed to load the QML scene");
        return 1;
    }

    auto *backend = engine.singletonInstance<NordVpn *>(QStringLiteral("io.github.timpalpant.knord"), QStringLiteral("NordVpn"));
    if (!backend) {
        qWarning("knord: could not resolve the NordVpn singleton");
        return 1;
    }

    auto *settings = engine.singletonInstance<AppSettings *>(QStringLiteral("io.github.timpalpant.knord"), QStringLiteral("AppSettings"));
    if (!settings) {
        qWarning("knord: could not resolve the AppSettings singleton");
        return 1;
    }

    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());

    auto *tray = new TrayIcon(backend, &app);
    QObject::connect(tray, &TrayIcon::activateRequested, &app, [window] {
        if (!window) {
            return;
        }
        window->show();
        window->raise();
        window->requestActivate();
    });

    // With no tray icon there is nothing to restore the window from, so closing
    // it has to end the process rather than leave it running invisibly.
    const auto applyTrayPreference = [&app, tray, settings] {
        const bool enabled = settings->trayEnabled();
        tray->setEnabled(enabled);
        QApplication::setQuitOnLastWindowClosed(!enabled);
    };
    QObject::connect(settings, &AppSettings::trayEnabledChanged, &app, applyTrayPreference);
    applyTrayPreference();

    // Mirror connect/disconnect outcomes into the Plasma notification stack so
    // they are visible when the window is closed to the tray.
    QObject::connect(backend, &NordVpn::actionCompleted, &app, [](const QString &message, bool success) {
        auto *notification = new KNotification(success ? QStringLiteral("connected") : QStringLiteral("failed"));
        notification->setComponentName(QStringLiteral("knord"));
        notification->setTitle(i18n("NordVPN"));
        notification->setText(message);
        notification->setIconName(success ? QStringLiteral("network-vpn") : QStringLiteral("dialog-error"));
        notification->setFlags(KNotification::CloseOnTimeout);
        notification->sendEvent();
    });

    return app.exec();
}
