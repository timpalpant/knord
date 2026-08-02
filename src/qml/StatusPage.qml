import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ScrollablePage {
    id: page

    title: i18n("Status")
    titleDelegate: applicationWindow().navigationTitleDelegate

    readonly property bool actionable: NordVpn.daemonAvailable && NordVpn.loggedIn

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: NordVpn.lastError
            visible: NordVpn.lastError.length > 0
            showCloseButton: true
            onVisibleChanged: if (!visible) NordVpn.clearError()
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: NordVpn.daemonAvailable && !NordVpn.loggedIn
            text: i18n("You are not logged in. Run <tt>nordvpn login</tt> in a terminal and follow the browser prompt.")
        }

        // Connection state hero.
        Kirigami.AbstractCard {
            Layout.fillWidth: true

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: NordVpn.connected ? "security-high" : "security-low"
                    implicitWidth: Kirigami.Units.iconSizes.enormous
                    implicitHeight: Kirigami.Units.iconSizes.enormous
                    color: NordVpn.connected ? Kirigami.Theme.positiveTextColor
                                             : Kirigami.Theme.neutralTextColor
                    isMask: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Kirigami.Units.largeSpacing
                }

                Kirigami.Heading {
                    text: NordVpn.stateLabel
                    level: 1
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                QQC2.Label {
                    text: {
                        if (!NordVpn.connected) {
                            return NordVpn.busy ? NordVpn.pendingAction : i18n("Your traffic is not being routed through NordVPN.");
                        }
                        const where = NordVpn.city.length > 0
                            ? i18nc("city, country", "%1, %2", NordVpn.city, NordVpn.country)
                            : NordVpn.country;
                        return NordVpn.countryFlag.length > 0 ? NordVpn.countryFlag + "  " + where : where;
                    }
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                QQC2.ProgressBar {
                    indeterminate: true
                    visible: NordVpn.busy
                    Layout.fillWidth: true
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                    Layout.alignment: Qt.AlignHCenter
                }

                QQC2.Button {
                    text: NordVpn.connected ? i18n("Disconnect") : i18n("Quick Connect")
                    icon.name: NordVpn.connected ? "network-disconnect" : "network-connect"
                    enabled: page.actionable && !NordVpn.busy
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    onClicked: NordVpn.connected ? NordVpn.disconnectVpn() : NordVpn.quickConnect()
                }
            }
        }

        // Connection details, only meaningful while a tunnel is up.
        Kirigami.AbstractCard {
            Layout.fillWidth: true
            visible: NordVpn.connected

            header: Kirigami.Heading {
                text: i18n("Connection")
                level: 3
                padding: Kirigami.Units.largeSpacing
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                DetailRow { label: i18n("Server"); value: NordVpn.server; Layout.fillWidth: true }
                DetailRow { label: i18n("Hostname"); value: NordVpn.hostname; monospace: true; Layout.fillWidth: true }
                DetailRow { label: i18n("IP address"); value: NordVpn.ip; monospace: true; Layout.fillWidth: true }
                DetailRow { label: i18n("Technology"); value: NordVpn.technology; Layout.fillWidth: true }
                DetailRow { label: i18n("Protocol"); value: NordVpn.protocol; Layout.fillWidth: true }
                DetailRow { label: i18n("Uptime"); value: NordVpn.uptimeLabel; Layout.fillWidth: true }
                DetailRow { label: i18n("Received"); value: NordVpn.transferReceived; Layout.fillWidth: true }
                DetailRow { label: i18n("Sent"); value: NordVpn.transferSent; Layout.fillWidth: true }
            }
        }
    }

    actions: [
        Kirigami.Action {
            text: i18n("Choose Location")
            icon.name: "globe"
            enabled: page.actionable
            onTriggered: applicationWindow().navigate("locations")
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            displayHint: Kirigami.DisplayHint.IconOnly
            onTriggered: {
                NordVpn.refreshStatus();
                NordVpn.refreshSettings();
            }
        }
    ]
}
