pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.ProgressBar {
    id: control
    required property DesignTokens tokens
    implicitWidth: 150
    implicitHeight: 3
    padding: 0
    background: Rectangle {
        color: control.tokens.line
    }
    contentItem: Item {
        Rectangle {
            width: parent.width * control.position
            height: parent.height
            color: control.tokens.accent
        }
    }
}
