pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property real calloutTop: 0
    property real calloutHeight: 0
    property real sideVeilOpacity: metrics.portrait ? 0 : 1
    Behavior on sideVeilOpacity {
        enabled: !root.tokens.reducedMotion && root.visible
        NumberAnimation {
            duration: 650 / root.tokens.motionSpeed
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.metrics.portrait
        gradient: Gradient {
            GradientStop {
                position: Math.max(0, (root.calloutTop - root.height * 0.12) / root.height)
                color: "transparent"
            }
            GradientStop {
                position: Math.max(0, (root.calloutTop + root.height * 0.07) / root.height)
                color: root.tokens.veil
            }
            GradientStop {
                position: Math.min(1, (root.calloutTop + root.calloutHeight) / root.height)
                color: root.tokens.page
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: !root.metrics.portrait
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.42
                color: "transparent"
            }
            GradientStop {
                position: 0.66
                color: root.tokens.veil
            }
            GradientStop {
                position: 0.96
                color: root.tokens.page
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        opacity: 1 - root.tokens.nightMix
        visible: opacity > 0
        gradient: Gradient {
            GradientStop {
                position: 0
                color: root.tokens.veil
            }
            GradientStop {
                position: 0.23
                color: "transparent"
            }
            GradientStop {
                position: 0.75
                color: "transparent"
            }
            GradientStop {
                position: 1
                color: root.tokens.veil
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        opacity: root.tokens.nightMix
        visible: opacity > 0
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.rgba(5 / 255, 13 / 255, 25 / 255, 0.4)
            }
            GradientStop {
                position: 0.24
                color: "transparent"
            }
            GradientStop {
                position: 0.73
                color: "transparent"
            }
            GradientStop {
                position: 1
                color: Qt.rgba(5 / 255, 13 / 255, 25 / 255, 0.72)
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.tokens.nightMix * root.sideVeilOpacity
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.32
                color: "transparent"
            }
            GradientStop {
                position: 0.58
                color: Qt.rgba(5 / 255, 13 / 255, 25 / 255, 0.28)
            }
            GradientStop {
                position: 0.78
                color: Qt.rgba(5 / 255, 13 / 255, 25 / 255, 0.78)
            }
            GradientStop {
                position: 1
                color: Qt.rgba(5 / 255, 13 / 255, 25 / 255, 0.84)
            }
        }
    }
    Rectangle {
        x: root.metrics.informationX
        y: root.calloutTop
        width: root.metrics.width - x - root.metrics.informationRight
        height: root.calloutHeight
        visible: root.metrics.portrait
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: root.tokens.veil
            }
            GradientStop {
                position: 1
                color: "transparent"
            }
        }
    }
}
