import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/*! A single connectable place. Clicking the row connects; countries also get a
 *  chevron that drills into their cities instead. */
QQC2.ItemDelegate {
    id: delegate

    required property string label
    required property string flag
    required property bool hasCities

    signal drillDown()

    // Width is deliberately left to the caller. Deriving it from ListView.view
    // breaks outside a view, where it falls back to implicitWidth and loops
    // against the content it is supposed to be measuring.

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            text: delegate.flag
            visible: delegate.flag.length > 0
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.3
        }

        QQC2.Label {
            text: delegate.label
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        QQC2.ToolButton {
            icon.name: "go-next-symbolic"
            visible: delegate.hasCities
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: i18n("Show cities")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: delegate.drillDown()
        }
    }
}
