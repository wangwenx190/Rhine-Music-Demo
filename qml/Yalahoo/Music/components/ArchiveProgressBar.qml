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
        antialiasing: true
        color: control.tokens.line
    }
    contentItem: Item {
        Rectangle {
            antialiasing: true
            width: parent.width * control.position
            height: parent.height
            color: control.tokens.accent
        }
    }
}
