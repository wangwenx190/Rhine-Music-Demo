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
        width: parent.width - 32
        ArchiveText {
            tokens: root.tokens
            text: root.subtitle
            cssPixelSize: 9
            font.letterSpacing: 2
            color: root.tokens.muted
        }
        ArchiveText {
            tokens: root.tokens
            text: root.title
            cssPixelSize: root.narrow ? 21 : 23
        }
    }
    ArchiveButton {
        anchors.right: parent.right
        tokens: root.tokens
        text: "×"
        cssPixelSize: 29
        Accessible.name: "关闭"
        onClicked: root.closeRequested()
    }
}
