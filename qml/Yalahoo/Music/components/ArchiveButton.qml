pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import QtQuick.Shapes

T.Button {
    id: control
    required property DesignTokens tokens
    property bool framed: false
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
    leftPadding: framed ? 10 : 0
    rightPadding: framed ? 10 : 0
    topPadding: framed ? 6 : 0
    bottomPadding: framed ? 6 : 0
    spacing: 7
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: framed ? cssPixelSize * 1.5 + topPadding + bottomPadding + 2 : 29
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.38
    Accessible.name: text
    background: Rectangle {
        color: control.framed && control.pressed ? Qt.rgba(control.tokens.accent.r, control.tokens.accent.g,
                                                           control.tokens.accent.b, .08) :
                                                   "transparent"
        radius: control.framed ? 2 : 0
        border.color: control.pressed ? control.tokens.accent : control.framed ? control.tokens.line :
                                                                                 "transparent"
        border.width: control.framed ? 1 : 0
        Rectangle {
            objectName: "buttonFocusOutline"
            x: -6
            y: -6
            width: control.width + 12
            height: control.height + 12
            visible: control.visualFocus
            color: "transparent"
            radius: 0
            border.width: 2
            border.color: control.tokens.accent
            antialiasing: true
        }
        Rectangle {
            antialiasing: true
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
                ArchiveVectorImage {
                    anchors.fill: parent
                    visible: control.iconName !== "settings"
                    source: control.iconName !== "settings" ? iconSlot.asset : ""
                }
                Item {
                    visible: control.iconName === "settings"
                    width: 24
                    height: 24
                    transform: Scale {
                        xScale: control.iconSize / 24
                        yScale: control.iconSize / 24
                    }
                    SceneGraphVisual {
                        width: 24
                        height: 24
                        sourceComponent: Shape {
                            antialiasing: true
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
                    }
                    SceneGraphVisual {
                        z: 1
                        width: 24
                        height: 24
                        sourceComponent: Shape {
                            antialiasing: true
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
            }
            ArchiveText {
                tokens: control.tokens
                visible: control.showText
                anchors.verticalCenter: parent.verticalCenter
                text: control.showText ? control.text : ""
                cssPixelSize: control.cssPixelSize
                cssLineHeight: control.framed ? control.cssPixelSize * 1.5 : Math.round(cssAscent)
                                                + Math.round(cssDescent)
                color: control.hovered || (control.framed && control.pressed)
                       ? control.tokens.accent : control.tokens.ink
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
