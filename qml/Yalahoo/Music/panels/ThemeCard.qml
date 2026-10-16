pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

T.Button {
    id: root
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    required property DesignTokens tokens
    required property bool night
    required property bool selected
    property bool narrow: false
    padding: narrow ? 9 : 12
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    Accessible.name: night ? "夜间主题" : "日间主题"
    Accessible.checkable: true
    Accessible.checked: selected
    background: Rectangle {
        color: "transparent"
        border.color: root.selected || root.activeFocus ? root.tokens.ink : root.tokens.line
        border.width: root.selected || root.activeFocus ? 2 : 1
    }
    contentItem: Column {
        spacing: 5
        Rectangle {
            width: parent.width
            height: 48
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0
                    color: root.night ? "#0c1728" : "#ded3bd"
                }
                GradientStop {
                    position: 1
                    color: root.night ? "#293b50" : "#f0ede4"
                }
            }
            Rectangle {
                visible: root.night
                x: parent.width * .64
                y: parent.height * .29
                width: 2
                height: 2
                radius: 1
                color: "#eee9df"
            }
            Rectangle {
                visible: root.night
                x: parent.width * .34
                y: parent.height * .62
                width: 2
                height: 2
                radius: 1
                color: "#e0deca"
            }
        }
        Item {
            width: 1
            height: 5
        }
        ArchiveText {
            tokens: root.tokens
            text: root.night ? "夜间" : "日间"
            cssPixelSize: 11
            font.weight: Font.Medium
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: root.night ? "极简星空与透光白卡" : "暖白玻璃与日光"
            cssPixelSize: root.narrow ? 7 : 8
            color: root.tokens.muted
            wrapMode: Text.Wrap
        }
    }
}
