pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T
import Yalahoo.Music

T.CheckBox {
    id: control
    required property DesignTokens tokens
    property real cssPixelSize: 12
    implicitWidth: implicitContentWidth
    implicitHeight: Math.max(22, implicitContentHeight)
    padding: 0
    spacing: 8
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : .38
    Accessible.name: text
    background: Rectangle {
        antialiasing: true
        color: "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: control.tokens.accent
    }
    indicator: Rectangle {
        x: 0
        y: (control.height - height) / 2
        width: 16
        height: 16
        radius: 1
        color: control.checkState !== Qt.Unchecked ? control.tokens.accent : "transparent"
        border.color: control.hovered || control.activeFocus || control.checked
                      ? control.tokens.accent : control.tokens.line
        SceneGraphVisual {
            anchors.fill: parent
            visible: control.checkState === Qt.Checked
            sourceComponent: Shape {
                antialiasing: true
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: control.tokens.surface
                    strokeWidth: 1.8
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    fillColor: "transparent"
                    startX: 3.5
                    startY: 8
                    PathLine {
                        x: 6.5
                        y: 11
                    }
                    PathLine {
                        x: 12.5
                        y: 5
                    }
                }
            }
        }
        Rectangle {
            antialiasing: true
            anchors.centerIn: parent
            width: 8
            height: 2
            color: control.tokens.surface
            visible: control.checkState === Qt.PartiallyChecked
        }
    }
    contentItem: Item {
        implicitWidth: caption.implicitWidth + 24
        implicitHeight: caption.implicitHeight
        ArchiveText {
            id: caption
            objectName: "checkBoxCaption"
            x: 24
            y: (parent.height - height) / 2
            width: Math.max(0, parent.width - x)
            tokens: control.tokens
            cssPixelSize: control.cssPixelSize
            text: control.text
            wrapMode: Text.Wrap
        }
    }
}
