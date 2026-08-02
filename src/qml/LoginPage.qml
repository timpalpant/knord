import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ScrollablePage {
    id: page

    title: i18n("Sign In")
    titleDelegate: applicationWindow().navigationTitleDelegate

    Connections {
        target: NordVpn

        // The CLI prints the sign-in URL a moment after starting; send the user
        // straight there rather than making them click a second time.
        function onLoginUrlReady(url) {
            Qt.openUrlExternally(url);
        }
    }

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

        Kirigami.AbstractCard {
            Layout.fillWidth: true

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: "user-identity"
                    implicitWidth: Kirigami.Units.iconSizes.enormous
                    implicitHeight: Kirigami.Units.iconSizes.enormous
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Kirigami.Units.largeSpacing
                }

                Kirigami.Heading {
                    text: NordVpn.loginInProgress ? i18n("Finish in your browser")
                                                  : i18n("Sign in to NordVPN")
                    level: 2
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                QQC2.Label {
                    text: NordVpn.loginInProgress
                        ? i18n("Log in to Nord Account in the browser window, then choose Continue. You will be brought back here automatically.")
                        : i18n("Signing in opens Nord Account in your browser. KNord never sees your password.")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    Layout.rightMargin: Kirigami.Units.gridUnit
                }

                QQC2.ProgressBar {
                    indeterminate: true
                    visible: NordVpn.loginInProgress
                    Layout.fillWidth: true
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Button {
                        text: i18n("Sign In with Browser")
                        icon.name: "internet-services"
                        visible: !NordVpn.loginInProgress
                        enabled: NordVpn.daemonAvailable
                        onClicked: NordVpn.beginLogin()
                    }

                    QQC2.Button {
                        text: i18n("Open Page Again")
                        icon.name: "internet-services"
                        visible: NordVpn.loginInProgress && NordVpn.loginUrl.length > 0
                        onClicked: Qt.openUrlExternally(NordVpn.loginUrl)
                    }

                    QQC2.Button {
                        text: i18n("Cancel")
                        icon.name: "dialog-cancel"
                        visible: NordVpn.loginInProgress
                        onClicked: NordVpn.cancelLogin()
                    }
                }
            }
        }

        // Shown only once a sign-in is underway: if the browser could not hand
        // control back, the "Continue" link finishes the job by hand.
        Kirigami.AbstractCard {
            Layout.fillWidth: true
            visible: NordVpn.loginInProgress

            header: Kirigami.Heading {
                text: i18n("Browser did not come back?")
                level: 4
                padding: Kirigami.Units.largeSpacing
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18n("Right-click the “Continue” button in the browser, copy the link address, and paste it here.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.TextField {
                        id: callbackField
                        placeholderText: "nordvpn://login?..."
                        Layout.fillWidth: true
                        onAccepted: NordVpn.completeLoginWithCallback(text)
                    }

                    QQC2.Button {
                        text: i18n("Finish")
                        icon.name: "dialog-ok"
                        enabled: callbackField.text.trim().length > 0
                        onClicked: NordVpn.completeLoginWithCallback(callbackField.text)
                    }
                }
            }
        }

        Kirigami.AbstractCard {
            Layout.fillWidth: true

            header: Kirigami.Heading {
                text: i18n("Sign in with a token")
                level: 4
                padding: Kirigami.Units.largeSpacing
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18n("Generate an access token in Nord Account under Services → NordVPN → Manual setup. Token sign-in does not support multi-factor authentication.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.TextField {
                        id: tokenField
                        placeholderText: i18n("Access token")
                        echoMode: TextInput.Password
                        Layout.fillWidth: true
                        onAccepted: NordVpn.loginWithToken(text)
                    }

                    QQC2.Button {
                        text: i18n("Sign In")
                        icon.name: "dialog-ok"
                        enabled: tokenField.text.trim().length > 0
                        onClicked: NordVpn.loginWithToken(tokenField.text)
                    }
                }
            }
        }
    }
}
