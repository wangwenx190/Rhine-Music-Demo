pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.ScrollBar {
    id: control
    required property DesignTokens tokens
    implicitWidth: 8
    implicitHeight: 8
    padding: 2
    minimumSize: orientation === Qt.Horizontal ? Math.min(1, 20 / Math.max(1, availableWidth)) :
                                                 Math.min(1, 20 / Math.max(1, availableHeight))
    visible: policy === T.ScrollBar.AlwaysOn || (policy === T.ScrollBar.AsNeeded && size < 1)
    background: Item {}
    contentItem: Rectangle {
        antialiasing: true
        implicitWidth: 4
        implicitHeight: 4
        radius: 2
        color: control.pressed ? control.tokens.accent : control.tokens.muted
        opacity: control.hovered || control.pressed ? .85 : .5
    }
}
