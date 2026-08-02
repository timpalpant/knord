import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ScrollablePage {
    id: page

    title: i18n("Allowlist")
    titleDelegate: applicationWindow().navigationTitleDelegate

    readonly property bool editable: NordVpn.daemonAvailable && !NordVpn.busy

    ListView {
        id: view

        model: NordVpn.allowlist

        header: Kirigami.InlineViewHeader {
            width: view.width
            text: i18n("Excluded from the VPN tunnel")
        }

        delegate: QQC2.ItemDelegate {
            id: entryDelegate

            required property string modelData

            width: view.width
            hoverEnabled: true

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    text: entryDelegate.modelData
                    font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                QQC2.ToolButton {
                    icon.name: "list-remove"
                    display: QQC2.AbstractButton.IconOnly
                    enabled: page.editable
                    QQC2.ToolTip.text: i18n("Remove")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: page.removeEntry(entryDelegate.modelData)
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: view.count === 0
            icon.name: "network-server"
            text: i18n("Nothing is excluded")
            explanation: i18n("Allowlisted ports and subnets bypass the VPN. They can accept incoming connections from outside your network.")
        }
    }

    /*! Entries render as "ports: 22 (TCP)" or "subnets: 192.168.0.0/16"; turn
     *  one back into the argument list that `nordvpn allowlist remove` wants. */
    function removeEntry(entry) {
        const kind = entry.split(":")[0].trim();
        const rest = entry.substring(entry.indexOf(":") + 1).trim();

        if (kind.startsWith("subnet")) {
            NordVpn.allowlistRemove(["subnet", rest]);
            return;
        }

        // "22 (TCP)" or "3000 - 5000 (UDP)"
        const protoMatch = rest.match(/\(([A-Za-z]+)\)/);
        const proto = protoMatch ? protoMatch[1].toUpperCase() : "";
        const numbers = rest.replace(/\(.*\)/, "").split("-").map(s => s.trim()).filter(s => s.length > 0);

        let args = [];
        if (numbers.length === 2) {
            args = ["ports", numbers[0], numbers[1]];
        } else if (numbers.length === 1) {
            args = ["port", numbers[0]];
        } else {
            return;
        }
        if (proto.length > 0) {
            args = args.concat(["protocol", proto]);
        }
        NordVpn.allowlistRemove(args);
    }

    actions: [
        Kirigami.Action {
            text: i18n("Add…")
            icon.name: "list-add"
            enabled: page.editable
            onTriggered: addDialog.open()
        },
        Kirigami.Action {
            text: i18n("Remove All")
            icon.name: "edit-clear-all"
            enabled: page.editable && view.count > 0
            onTriggered: NordVpn.allowlistRemoveAll()
        }
    ]

    Kirigami.PromptDialog {
        id: addDialog

        title: i18n("Add Allowlist Rule")
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Add")
                icon.name: "list-add"
                enabled: kindCombo.currentIndex === 2
                    ? subnetField.text.trim().length > 0
                    : portField.value > 0
                onTriggered: {
                    const proto = protocolCombo.currentIndex === 0 ? "" : protocolCombo.currentText;
                    if (kindCombo.currentIndex === 0) {
                        NordVpn.allowlistAddPort(portField.value, proto);
                    } else if (kindCombo.currentIndex === 1) {
                        NordVpn.allowlistAddPortRange(portField.value, portToField.value, proto);
                    } else {
                        NordVpn.allowlistAddSubnet(subnetField.text.trim());
                    }
                    addDialog.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: addDialog.close()
            }
        ]

        contentItem: Kirigami.FormLayout {
            QQC2.ComboBox {
                id: kindCombo
                Kirigami.FormData.label: i18n("Type:")
                model: [i18n("Port"), i18n("Port range"), i18n("Subnet")]
            }

            QQC2.SpinBox {
                id: portField
                Kirigami.FormData.label: kindCombo.currentIndex === 1 ? i18n("From:") : i18n("Port:")
                visible: kindCombo.currentIndex !== 2
                from: 1
                to: 65535
                value: 22
                editable: true
            }

            QQC2.SpinBox {
                id: portToField
                Kirigami.FormData.label: i18n("To:")
                visible: kindCombo.currentIndex === 1
                from: 1
                to: 65535
                value: 1024
                editable: true
            }

            QQC2.ComboBox {
                id: protocolCombo
                Kirigami.FormData.label: i18n("Protocol:")
                visible: kindCombo.currentIndex !== 2
                model: [i18n("TCP and UDP"), "TCP", "UDP"]
            }

            QQC2.TextField {
                id: subnetField
                Kirigami.FormData.label: i18n("Subnet:")
                visible: kindCombo.currentIndex === 2
                placeholderText: "192.168.1.0/24"
            }
        }
    }
}
