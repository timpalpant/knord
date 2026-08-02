import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.knord

FormCard.FormCardPage {
    id: page

    title: i18n("Settings")
    titleDelegate: applicationWindow().navigationTitleDelegate

    // A toggle is disabled while any command is in flight, otherwise a burst of
    // clicks would queue up contradictory `nordvpn set` calls.
    readonly property bool editable: NordVpn.daemonAvailable && !NordVpn.busy

    Component.onCompleted: NordVpn.refreshSettings()

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        type: Kirigami.MessageType.Error
        text: NordVpn.lastError
        visible: NordVpn.lastError.length > 0
        showCloseButton: true
        onVisibleChanged: if (!visible) NordVpn.clearError()
    }

    FormCard.FormHeader {
        title: i18n("Protection")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Kill Switch")
            description: i18n("Block internet access whenever the VPN drops")
            checked: NordVpn.settings.killswitch ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("killswitch", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Threat Protection Lite")
            description: i18n("Block ads, trackers and malicious domains")
            checked: NordVpn.settings.threatprotectionlite ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("threatprotectionlite", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Firewall")
            description: i18n("Let NordVPN manage firewall rules")
            checked: NordVpn.settings.firewall ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("firewall", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Post-quantum encryption")
            description: i18n("Only works on standard NordLynx servers, and not with Meshnet")
            checked: NordVpn.settings["post-quantum"] ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("post-quantum", checked)
        }
    }

    FormCard.FormHeader {
        title: i18n("Connection")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            id: technologyCombo
            text: i18n("Technology")
            model: ["NORDLYNX", "OPENVPN", "NORDWHISPER"]
            currentIndex: model.indexOf(NordVpn.settings.technology ?? "NORDLYNX")
            enabled: page.editable
            onActivated: (index) => {
                if (model[index] !== NordVpn.settings.technology) {
                    NordVpn.setSetting("technology", model[index]);
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        // The CLI only accepts `set protocol` while OpenVPN is selected.
        FormCard.FormComboBoxDelegate {
            id: protocolCombo
            text: i18n("Protocol")
            model: ["UDP", "TCP"]
            visible: (NordVpn.settings.technology ?? "") === "OPENVPN"
            currentIndex: Math.max(0, model.indexOf(NordVpn.settings.protocol ?? "UDP"))
            enabled: page.editable
            onActivated: (index) => {
                if (model[index] !== NordVpn.settings.protocol) {
                    NordVpn.setSetting("protocol", model[index]);
                }
            }
        }

        FormCard.FormDelegateSeparator {
            visible: protocolCombo.visible
        }

        FormCard.FormSwitchDelegate {
            text: i18n("Auto-connect")
            description: i18n("Connect automatically when the system starts")
            checked: NordVpn.settings.autoconnect ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("autoconnect", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Virtual locations")
            description: i18n("Allow servers hosted outside the country they advertise")
            checked: NordVpn.settings["virtual-location"] ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("virtual-location", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Meshnet")
            description: i18n("Private network linking your own devices")
            checked: NordVpn.settings.meshnet ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("meshnet", checked)
        }
    }

    FormCard.FormHeader {
        title: i18n("Local Network")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("LAN discovery")
            description: i18n("Reach printers and other devices on your local network")
            checked: NordVpn.settings["lan-discovery"] ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("lan-discovery", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Ignore ARP requests")
            description: i18n("Stop the device answering ARP probes while the VPN is up")
            checked: NordVpn.settings["arp-ignore"] ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("arp-ignore", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Allowlist")
            description: NordVpn.allowlist.length > 0
                ? i18np("%1 rule", "%1 rules", NordVpn.allowlist.length)
                : i18n("No ports or subnets excluded")
            icon.name: "network-server"
            onClicked: applicationWindow().pageStack.push(allowlistPage)
        }
    }

    FormCard.FormHeader {
        title: i18n("DNS")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: dnsField
            label: i18n("Custom DNS servers")
            placeholderText: i18n("e.g. 1.1.1.1, 1.0.0.1")
            text: (NordVpn.settings.dns ?? []).join(", ")
            enabled: page.editable
            // Up to 3 IPv4 addresses; setting them disables Threat Protection Lite.
            status: Kirigami.MessageType.Information
            statusMessage: i18n("Up to 3 IPv4 addresses. Setting DNS turns off Threat Protection Lite.")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Apply DNS servers")
            icon.name: "dialog-ok-apply"
            enabled: page.editable
            onClicked: {
                const servers = dnsField.text.split(",")
                    .map(s => s.trim())
                    .filter(s => s.length > 0);
                NordVpn.setDns(servers);
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Use NordVPN DNS")
            description: i18n("Clear custom DNS servers")
            icon.name: "edit-clear-all"
            enabled: page.editable && (NordVpn.settings.dns ?? []).length > 0
            onClicked: NordVpn.setDns([])
        }
    }

    FormCard.FormHeader {
        title: i18n("Application")
    }

    FormCard.FormCard {
        // KNord's own preference, not a NordVPN daemon setting.
        FormCard.FormSwitchDelegate {
            text: i18n("Show tray icon")
            description: AppSettings.trayEnabled
                ? i18n("Closing the window leaves KNord running in the tray")
                : i18n("Closing the window quits KNord. The VPN stays connected either way.")
            checked: AppSettings.trayEnabled
            onToggled: AppSettings.trayEnabled = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Desktop notifications")
            description: i18n("Let the NordVPN daemon post its own notifications")
            checked: NordVpn.settings.notify ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("notify", checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Send anonymous analytics")
            checked: NordVpn.settings.analytics ?? false
            enabled: page.editable
            onToggled: NordVpn.setSetting("analytics", checked)
        }
    }

    Component {
        id: allowlistPage
        AllowlistPage {}
    }
}
