pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import QtQuick.Shapes

Button {
    id: control
    required property DesignTokens tokens
    property string glyph: ""
    property string iconName: ""
    property real iconSize: 17
    property bool underline: false
    property bool showText: true
    property real cssPixelSize: 16
    font.family: tokens.regular.family
    font.styleName: tokens.regular.styleName
    font.weight: tokens.regular.weight
    font.pixelSize: Math.round(cssPixelSize)
    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    spacing: 7
    implicitWidth: contentItem.implicitWidth
    implicitHeight: 29
    opacity: enabled ? 1 : 0.38
    Accessible.name: text
    background: Rectangle {
        color: "transparent"
        border.color: control.activeFocus ? control.tokens.accent : "transparent"
        border.width: 2
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: control.underline ? 1 : 0
            color: control.tokens.line
        }
    }
    contentItem: Item {
        implicitWidth: buttonRow.implicitWidth
        implicitHeight: buttonRow.implicitHeight
        Row {
            id: buttonRow
            anchors.centerIn: parent
            anchors.alignWhenCentered: false
            spacing: control.spacing
            Item {
                id: iconSlot
                objectName: control.iconName + 'Icon'
                visible: control.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.alignWhenCentered: false
                width: visible ? control.iconSize : 0
                height: control.iconSize
                readonly property string asset: control.iconName.length > 0 ? "qrc:/assets/icons/"
                                                                              + control.iconName + (
                                                                                  control.tokens.night
                                                                                  ? "-night.svg" :
                                                                                    "-day.svg") : ""
                Image {
                    anchors.fill: parent
                    visible: control.iconName !== "settings"
                    source: control.iconName !== "settings" ? iconSlot.asset : ""
                    readonly property real outputDpr: Window.window
                                                      ? Window.window.devicePixelRatio :
                                                        Screen.devicePixelRatio
                    // Preserve tiny circles and rounded caps before filtering
                    // the SVG to the physical output extent.
                    sourceSize: Qt.size(width * outputDpr * 4, height * outputDpr * 4)
                    mipmap: true
                }
                Item {
                    visible: control.iconName === "settings"
                    width: 24
                    height: 24
                    transform: Scale {
                        xScale: control.iconSize / 24
                        yScale: control.iconSize / 24
                    }
                    Shape {
                        width: 24
                        height: 24
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: control.tokens.ink
                            strokeWidth: 1.5
                            capStyle: ShapePath.RoundCap
                            fillColor: "transparent"
                            PathSvg {
                                path: "M4 7h16M4 17h16"
                            }
                        }
                    }
                    Shape {
                        z: 1
                        width: 24
                        height: 24
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: control.tokens.ink
                            strokeWidth: 1.5
                            fillColor: control.tokens.surface
                            PathAngleArc {
                                centerX: 9
                                centerY: 7
                                radiusX: 2.5
                                radiusY: 2.5
                                startAngle: 0
                                sweepAngle: 360
                            }
                        }
                        ShapePath {
                            strokeColor: control.tokens.ink
                            strokeWidth: 1.5
                            fillColor: control.tokens.surface
                            PathAngleArc {
                                centerX: 15
                                centerY: 17
                                radiusX: 2.5
                                radiusY: 2.5
                                startAngle: 0
                                sweepAngle: 360
                            }
                        }
                    }
                }
            }
            ArchiveText {
                tokens: control.tokens
                visible: control.showText
                anchors.verticalCenter: parent.verticalCenter
                text: control.showText ? control.text : ""
                cssPixelSize: control.cssPixelSize
                color: control.hovered ? control.tokens.accent : control.tokens.ink
            }
            ArchiveText {
                tokens: control.tokens
                visible: control.glyph.length > 0
                anchors.verticalCenter: parent.verticalCenter
                text: control.glyph
                cssPixelSize: 24
            }
        }
    }
}
