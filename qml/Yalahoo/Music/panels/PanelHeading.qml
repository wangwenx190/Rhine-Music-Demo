pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property string title
    required property string subtitle
    property bool narrow: false
    signal closeRequested
    property real bottomSpacing: 7
    implicitHeight: copy.height + bottomSpacing
    Column {
        id: copy
        spacing: 7
        width: Math.max(0, parent.width - 32)
        ArchiveText {
            tokens: root.tokens
            text: root.subtitle
            width: parent.width
            wrapMode: Text.WrapAnywhere
            cssPixelSize: 9
            font.letterSpacing: 2
            color: root.tokens.muted
        }
        ArchiveText {
            tokens: root.tokens
            text: root.title
            width: parent.width
            wrapMode: Text.WrapAnywhere
            cssPixelSize: root.narrow ? 21 : 23
        }
    }
    ArchiveButton {
        anchors.right: parent.right
        tokens: root.tokens
        text: "\u00d7"
        cssPixelSize: 29
        Accessible.name: qsTr("Close")
        onClicked: root.closeRequested()
    }
}
