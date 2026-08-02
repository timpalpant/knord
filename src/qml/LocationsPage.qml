import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.timpalpant.knord

Kirigami.ScrollablePage {
    id: page

    title: i18n("Locations")
    titleDelegate: applicationWindow().navigationTitleDelegate

    header: Kirigami.SearchField {
        placeholderText: i18n("Search countries and groups…")
        onTextChanged: {
            NordVpn.countries.filter = text;
            NordVpn.groups.filter = text;
        }
    }

    Component.onCompleted: {
        if (NordVpn.countries.count === 0) {
            NordVpn.refreshLocations();
        }
    }

    ListView {
        id: view

        model: NordVpn.countries
        currentIndex: -1
        reuseItems: true

        // Plain Column, not a Layout: mixing Layout.fillWidth with the explicit
        // widths the delegates need is what made this list loop while scrolling.
        header: Column {
            width: view.width

            Kirigami.InlineViewHeader {
                width: parent.width
                text: i18n("Specialty Servers")
                visible: groupRepeater.count > 0
            }

            Repeater {
                id: groupRepeater
                model: NordVpn.groups

                delegate: LocationDelegate {
                    required property var connectArgs

                    width: view.width
                    enabled: NordVpn.daemonAvailable && NordVpn.loggedIn && !NordVpn.busy
                    onClicked: NordVpn.connectTo(connectArgs, label)
                }
            }

            Kirigami.InlineViewHeader {
                width: parent.width
                text: i18n("Countries")
                visible: view.count > 0
            }
        }

        delegate: LocationDelegate {
            required property string name
            required property var connectArgs

            width: view.width
            enabled: NordVpn.daemonAvailable && NordVpn.loggedIn && !NordVpn.busy
            onClicked: NordVpn.connectTo(connectArgs, label)
            onDrillDown: {
                NordVpn.loadCities(name);
                applicationWindow().pageStack.push(citiesPageComponent, {
                    countryName: name,
                    countryDisplay: label
                });
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: view.count === 0 && groupRepeater.count === 0
            icon.name: "globe"
            text: i18n("Loading locations…")
        }
    }

    // Owned by this page, but only ever used while this page is alive and on
    // top of the stack, so it cannot be destroyed mid-incubation.
    Component {
        id: citiesPageComponent
        CitiesPage {}
    }
}
