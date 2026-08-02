import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.knord

FormCard.FormCardPage {
    id: page

    title: i18n("Account")
    titleDelegate: applicationWindow().navigationTitleDelegate

    Component.onCompleted: NordVpn.refreshAccount()

    FormCard.FormHeader {
        title: i18n("Subscription")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Email address")
            description: NordVpn.account.email ?? i18n("Unknown")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Subscription")
            description: NordVpn.account.subscription ?? i18n("Unknown")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Member since")
            description: NordVpn.account.created ?? i18n("Unknown")
            visible: NordVpn.account.created !== undefined
        }
    }

    FormCard.FormHeader {
        title: i18n("Features")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Dedicated IP")
            description: NordVpn.account.dedicatedIp ?? i18n("Unknown")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Dedicated Server")
            description: NordVpn.account.dedicatedServer ?? i18n("Unknown")
            visible: NordVpn.account.dedicatedServer !== undefined
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Multi-factor authentication")
            description: NordVpn.account.mfa ?? i18n("Unknown")
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing * 2

        FormCard.FormButtonDelegate {
            text: i18n("Manage account online")
            description: i18n("Opens my.nordaccount.com in your browser")
            icon.name: "internet-services"
            onClicked: Qt.openUrlExternally("https://my.nordaccount.com/")
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing * 2

        FormCard.FormButtonDelegate {
            text: i18n("Sign out")
            description: i18n("Disconnects the VPN and detaches this device from your account")
            icon.name: "system-log-out"
            enabled: NordVpn.daemonAvailable && !NordVpn.busy
            onClicked: logoutPrompt.open()
        }
    }

    Kirigami.PromptDialog {
        id: logoutPrompt

        title: i18n("Sign Out?")
        subtitle: i18n("This disconnects the VPN. You will need to sign in through the browser again to reconnect.")
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Sign Out")
                icon.name: "system-log-out"
                onTriggered: {
                    NordVpn.logout(keepToken.checked);
                    logoutPrompt.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: logoutPrompt.close()
            }
        ]

        QQC2.CheckBox {
            id: keepToken
            text: i18n("Keep my access token valid")
            checked: false
        }
    }
}
