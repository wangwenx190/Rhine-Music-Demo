pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    required property DesignTokens tokens
    signal themeRequested(bool night)
    width: 116
    height: 32
    radius: 20
    color: "transparent"
    border.color: tokens.line
    Rectangle {
        // CSS absolute offsets are measured inside the 1px border.
        x: root.tokens.night ? root.width / 2 + 2 : 4
        y: 4
        width: (root.width - 12) / 2
        height: root.height - 8
        radius: 16
        color: root.tokens.ink
        Behavior on x {
            enabled: !root.tokens.reducedMotion && root.visible
            NumberAnimation {
                duration: 380 / root.tokens.motionSpeed
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.22, 1, 0.36, 1, 1, 1]
            }
        }
    }
    Repeater {
        model: ["暖昼", "深夜"]
        delegate: Button {
            id: option
            required property string modelData
            required property int index
            objectName: index === 0 ? "dayButton" : "nightButton"
            x: index === 0 ? 4 : root.width / 2 + 2
            y: 4
            width: (root.width - 12) / 2
            height: root.height - 8
            padding: 0
            opacity: checked ? 1 : 0.6
            text: modelData
            checked: root.tokens.night === (index === 1)
            Accessible.name: text + "主题"
            background: Rectangle {
                color: "transparent"
                radius: 16
                border.color: option.activeFocus ? root.tokens.accent : "transparent"
            }
            contentItem: Item {
                Row {
                    anchors.centerIn: parent
                    anchors.alignWhenCentered: false
                    spacing: root.width === 104 ? 4 : 5
                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.alignWhenCentered: false
                        width: 7
                        height: 7
                        Rectangle {
                            x: -1
                            y: -1
                            width: 9
                            height: 9
                            radius: 4.5
                            color: "transparent"
                            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.35)
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 3.5
                            color: option.index === 0 ? "#e9c492" : "#415577"
                        }
                    }
                    ArchiveText {
                        tokens: root.tokens
                        anchors.verticalCenter: parent.verticalCenter
                        text: option.text
                        cssPixelSize: 10
                        color: option.checked ? root.tokens.surface : root.tokens.ink
                    }
                }
            }
            onClicked: root.themeRequested(index === 1)
        }
    }
}
