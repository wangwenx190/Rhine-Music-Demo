pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.Slider {
    id: control
    required property DesignTokens tokens
    implicitWidth: horizontal ? 150 : 16
    implicitHeight: horizontal ? 16 : 150
    property real disabledOpacity: .38
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    wheelEnabled: false
    opacity: enabled ? 1 : disabledOpacity
    background: Item {
        Rectangle {
            id: rail
            x: control.horizontal ? control.leftPadding + control.handle.width / 2 : (control.width
                                                                                      - width) / 2
            y: control.horizontal ? (control.height - height) / 2 : control.topPadding
                                    + control.handle.height / 2
            width: control.horizontal ? Math.max(0, control.availableWidth - control.handle.width) :
                                        2

            height: control.horizontal ? 2 : Math.max(0, control.availableHeight
                                                      - control.handle.height)
            color: control.tokens.line
            radius: 1
            Rectangle {
                x: control.horizontal && control.mirrored ? parent.width - width : 0
                y: control.horizontal ? 0 : parent.height - height
                width: control.horizontal ? parent.width * control.position : parent.width
                height: control.horizontal ? parent.height : parent.height * control.position
                color: control.tokens.accent
                radius: 1
            }
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: control.visualFocus ? 1 : 0
            border.color: control.tokens.accent
            radius: 2
        }
    }
    handle: Rectangle {
        x: control.leftPadding + (control.horizontal ? control.visualPosition * (
                                                           control.availableWidth - width) : (
                                                           control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 :
                                                      control.visualPosition * (
                                                          control.availableHeight - height))
        width: 16
        height: 16
        radius: 8
        antialiasing: true
        color: control.pressed ? control.tokens.accent : control.tokens.surface
        border.color: control.pressed || control.hovered || control.activeFocus
                      ? control.tokens.accent : control.tokens.muted
        border.width: 1
    }
}
