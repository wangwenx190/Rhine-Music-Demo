pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

QtObject {
    id: root
    property bool night: false
    property bool hdrActive: false
    property bool reducedMotion: false
    property bool offscreenRendering: false
    property real motionSpeed: 1
    readonly property string displayName: ApplicationIdentity.displayName
    property color ink: night ? "#e9eef2" : "#232722"
    property color muted: night ? "#9caabd" : "#74786f"
    property color line: night ? Qt.rgba(208 / 255, 226 / 255, 243 / 255, 0.2) : Qt.rgba(42 / 255,
                                                                                         49 / 255, 42
                                                                                         / 255, 0.18)
    property color accent: night ? "#d9ad7d" : "#b56834"
    property color surface: night ? "#162234" : "#efede7"
    property color page: night ? "#08121f" : "#e8e5e1"
    property color veil: night ? Qt.rgba(10 / 255, 21 / 255, 37 / 255, 0.75) : Qt.rgba(236 / 255,
                                                                                       233 / 255,
                                                                                       225 / 255,
                                                                                       0.72)
    readonly property font regular: MusicFonts.regular
    readonly property font demibold: MusicFonts.demibold
    readonly property font bold: MusicFonts.bold
    readonly property real paletteDuration: reducedMotion ? 0 : 650 / motionSpeed
    property real nightMix: night ? 1 : 0
    Behavior on ink {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on muted {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on line {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on accent {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on surface {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on page {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on veil {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
    Behavior on nightMix {
        NumberAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1]
        }
    }
}
