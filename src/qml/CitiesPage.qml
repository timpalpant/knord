import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ScrollablePage {
    id: page

    property string countryName
    property string countryDisplay

    title: page.countryDisplay
    titleDelegate: applicationWindow().navigationTitleDelegate

    header: Kirigami.SearchField {
        placeholderText: i18n("Search cities…")
        onTextChanged: NordVpn.cities.filter = text
    }

    ListView {
        id: view

        model: NordVpn.cities
        currentIndex: -1
        reuseItems: true

        header: QQC2.ItemDelegate {
            width: view.width
            enabled: NordVpn.daemonAvailable && NordVpn.loggedIn && !NordVpn.busy
            onClicked: NordVpn.connectTo([page.countryName], page.countryDisplay)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: "network-connect"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                }

                QQC2.Label {
                    text: i18n("Best server in %1", page.countryDisplay)
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        delegate: LocationDelegate {
            required property var connectArgs

            width: view.width
            enabled: NordVpn.daemonAvailable && NordVpn.loggedIn && !NordVpn.busy
            onClicked: NordVpn.connectTo(connectArgs,
                                         i18nc("city, country", "%1, %2", label, page.countryDisplay))
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: view.count === 0
            icon.name: "mark-location"
            text: i18n("Loading cities…")
        }
    }
}
