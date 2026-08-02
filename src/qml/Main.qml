import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ApplicationWindow {
    id: root

    title: i18n("NordVPN")

    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 28
    // Matches the Status page without the large empty area the old sidebar
    // layout left beneath it. Other pages remain scrollable and resizable.
    width: Kirigami.Units.gridUnit * 32
    height: Kirigami.Units.gridUnit * 34

    // At the default window size, a fixed drawer takes too much room from the
    // page. Use a compact hamburger menu on desktop and retain a drawer on
    // mobile, until there is room for both navigation and content.
    readonly property bool compactNavigation: Kirigami.Settings.isMobile
                                          || width < Kirigami.Units.gridUnit * 50
    readonly property bool compactDesktopNavigation: compactNavigation
                                                  && !Kirigami.Settings.isMobile

    // This is deliberately an app-owned menu rather than GlobalDrawer.isMenu:
    // the latter currently produces a binding loop in Kirigami's private
    // toolbar implementation. Pages use this as their title delegate so the
    // button occupies the normal leading-header position.
    readonly property Component navigationTitleDelegate: navigationTitleDelegateComponent

    // Which drawer entry is active, so the actions stay mutually exclusive.
    property string currentPage: "status"

    // With a tray icon, closing hides to it, matching how the rest of Plasma's
    // connectivity applets behave. Without one there is no way back, so closing
    // quits -- the daemon keeps the tunnel up either way.
    onClosing: (close) => {
        if (AppSettings.trayEnabled) {
            close.accepted = false;
            root.hide();
        }
    }

    onVisibleChanged: NordVpn.setWindowVisible(root.visible)

    // Overlay drawers close themselves when becoming modal. Re-open the
    // sidebar, and re-enable its collapse control, when the window grows past
    // the navigation breakpoint.
    onCompactNavigationChanged: {
        if (!compactNavigation) {
            Qt.callLater(() => {
                globalDrawer.collapsible = true;
                globalDrawer.open();
            });
        }
    }

    Component { id: statusPage; StatusPage {} }
    Component { id: locationsPage; LocationsPage {} }
    Component { id: settingsPage; SettingsPage {} }
    Component { id: accountPage; AccountPage {} }
    Component { id: loginPage; LoginPage {} }

    Component {
        id: navigationTitleDelegateComponent

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.maximumWidth: implicitWidth

            QQC2.ToolButton {
                id: navigationButton

                visible: root.compactDesktopNavigation && NordVpn.loggedIn
                display: QQC2.AbstractButton.IconOnly
                icon.name: "application-menu-symbolic"
                text: i18n("Navigation")
                onClicked: navigationMenu.popup(navigationButton, 0, navigationButton.height)

                QQC2.ToolTip {
                    visible: navigationButton.hovered
                    text: navigationButton.text
                    delay: Kirigami.Units.toolTipDelay
                    y: navigationButton.height
                }
            }

            Kirigami.Heading {
                text: root.pageStack.currentItem?.title ?? ""
                maximumLineCount: 1
                elide: Text.ElideRight
                textFormat: Text.PlainText
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
        }
    }

    QQC2.Menu {
        id: navigationMenu

        QQC2.MenuItem {
            text: i18n("Status")
            icon.name: "network-vpn"
            onTriggered: root.navigate("status")
        }

        QQC2.MenuItem {
            text: i18n("Locations")
            icon.name: "globe"
            onTriggered: root.navigate("locations")
        }

        QQC2.MenuItem {
            text: i18n("Settings")
            icon.name: "configure"
            onTriggered: root.navigate("settings")
        }

        QQC2.MenuItem {
            text: i18n("Account")
            icon.name: "user-identity"
            onTriggered: root.navigate("account")
        }
    }

    function componentFor(key) {
        switch (key) {
        case "status": return statusPage;
        case "locations": return locationsPage;
        case "settings": return settingsPage;
        case "account": return accountPage;
        case "login": return loginPage;
        }
        return null;
    }

    /*! Navigation goes through a key rather than a Component, so that pages
     *  never hand out a Component they own: clear() destroys the outgoing page,
     *  which would tear down the Component while the incoming page is still
     *  incubating from it. */
    function navigate(key) {
        if (root.currentPage === key && root.pageStack.depth === 1) {
            return;
        }
        const component = root.componentFor(key);
        if (!component) {
            return;
        }
        root.currentPage = key;
        root.pageStack.clear();
        root.pageStack.push(component);
        if (globalDrawer.modal) {
            globalDrawer.close();
        }
    }

    // Being signed out takes over the whole window: nothing else in the app can
    // do anything useful until an account is attached.
    Connections {
        target: NordVpn

        function onAvailabilityChanged() {
            if (!NordVpn.loggedIn && NordVpn.daemonAvailable) {
                root.navigate("login");
            } else if (NordVpn.loggedIn && root.currentPage === "login") {
                root.navigate("status");
            }
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer

        title: i18n("NordVPN")
        titleIcon: "network-vpn"
        // The compact desktop menu is provided by navigationMenu above. Keep
        // the GlobalDrawer for a persistent wide sidebar and a mobile drawer.
        isMenu: false
        modal: root.compactNavigation
        collapsible: !root.compactNavigation
        enabled: !root.compactDesktopNavigation
        handleVisible: !root.compactDesktopNavigation
                       && (modal || !drawerOpen)
                       && root.controlsVisible

        // Hidden while signed out; the login page is the only useful screen.
        actions: [
            Kirigami.Action {
                text: i18n("Status")
                visible: NordVpn.loggedIn
                icon.name: "network-vpn"
                checkable: true
                checked: root.currentPage === "status"
                onTriggered: root.navigate("status")
            },
            Kirigami.Action {
                text: i18n("Locations")
                icon.name: "globe"
                visible: NordVpn.loggedIn
                checkable: true
                checked: root.currentPage === "locations"
                onTriggered: root.navigate("locations")
            },
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "configure"
                visible: NordVpn.loggedIn
                checkable: true
                checked: root.currentPage === "settings"
                onTriggered: root.navigate("settings")
            },
            Kirigami.Action {
                text: i18n("Account")
                icon.name: "user-identity"
                visible: NordVpn.loggedIn
                checkable: true
                checked: root.currentPage === "account"
                onTriggered: root.navigate("account")
            }
        ]
    }

    pageStack.initialPage: statusPage
}
