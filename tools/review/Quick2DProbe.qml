pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import Yalahoo.Music

Item {
    id: root
    property bool night: false
    readonly property color ink: night ? "#e9eef2" : "#232722"
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.ink
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg {
                path: "M16 60 C40 12 76 108 112 60"
            }
        }
    }
    Shape {
        x: 136
        y: 20
        width: 96
        height: 60
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0
                y1: 0
                x2: 96
                y2: 60
                GradientStop {
                    position: 0
                    color: "#b56834"
                }
                GradientStop {
                    position: 1
                    color: "#64d9ad7d"
                }
            }
            startX: 0
            startY: 0
            PathLine {
                x: 96
                y: 0
            }
            PathLine {
                x: 96
                y: 60
            }
            PathLine {
                x: 0
                y: 60
            }
            PathLine {
                x: 0
                y: 0
            }
        }
    }
    Item {
        x: 256
        y: 20
        width: 80
        height: 60
        clip: true
        Rectangle {
            x: -12
            y: -10
            width: 110
            height: 100
            color: root.ink
            opacity: 128 / 255
        }
    }
    Text {
        x: 16
        y: 128 - baselineOffset
        text: "\u97f3\u4e50\u6536\u85cf  gyp  e\u0301  \ud83c\udfb5"
        font.family: MusicFonts.regular.family
        font.styleName: MusicFonts.regular.styleName
        font.weight: MusicFonts.regular.weight
        font.pixelSize: 20
        color: root.ink
    }
}
