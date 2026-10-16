pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    default property alias content: body.data
    property bool last: false
    width: parent ? parent.width : 300
    implicitHeight: body.implicitHeight + 40
    Column {
        id: body
        y: 20
        width: parent.width
        spacing: 11
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: root.tokens.line
        visible: !root.last
    }
}
