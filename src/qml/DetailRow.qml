import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/*! One label/value pair in the connection detail list. Collapses away when the
 *  value is empty so the card does not show blank rows. */
RowLayout {
    id: root

    property string label
    property string value
    property bool monospace: false

    spacing: Kirigami.Units.largeSpacing
    visible: root.value.length > 0

    QQC2.Label {
        text: root.label
        color: Kirigami.Theme.disabledTextColor
        Layout.minimumWidth: Kirigami.Units.gridUnit * 6
    }

    QQC2.Label {
        text: root.value
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignRight
        font.family: root.monospace ? "monospace" : Kirigami.Theme.defaultFont.family
        Layout.fillWidth: true
    }
}
