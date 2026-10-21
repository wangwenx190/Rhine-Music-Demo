// Owns the shared visual palette and source transition curve. All durations use the same motion-
// speed preference; reduced motion completes palette changes immediately.

pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

QtObject {
    id: root
    // Byte color channels are normalized once; palette duration is authored in milliseconds.
    readonly property real paletteTransitionMilliseconds: 650
    readonly property real colorChannelMaximum: 255

    readonly property list<real> paletteCurve: [0.4, 0, 0.2, 1, 1, 1]
    readonly property real editorWidth: 200
    readonly property real editorLineHeight: 20.4
    readonly property real editorHorizontalPadding: 14
    readonly property real editorVerticalPadding: 13
    readonly property real disabledControlOpacity: 0.38
    readonly property font editorFont: Qt.font({
                                                   family: regular.family,
                                                   styleName: regular.styleName,
                                                   weight: regular.weight,
                                                   pixelSize: 12
                                               })
    property bool night: false
    property bool hdrActive: false
    property bool reducedMotion: false
    property bool offscreenRendering: false
    property real motionSpeed: 1
    readonly property string displayName: ApplicationIdentity.displayName
    property color ink: night ? "#e9eef2" : "#232722"
    property color muted: night ? "#9caabd" : "#74786f"
    property color line: night ? Qt.rgba(208 / root.colorChannelMaximum, 226 / root.colorChannelMaximum,
                                         243 / root.colorChannelMaximum, 0.2) : Qt.rgba(42
                                                                                        / root.colorChannelMaximum,
                                                                                        49 / root.colorChannelMaximum,
                                                                                        42 / root.colorChannelMaximum,
                                                                                        0.18)
    property color accent: night ? "#d9ad7d" : "#b56834"
    property color surface: night ? "#162234" : "#efede7"
    property color page: night ? "#08121f" : "#e8e5e1"
    property color veil: night ? Qt.rgba(10 / root.colorChannelMaximum, 21 / root.colorChannelMaximum,
                                         37 / root.colorChannelMaximum, 0.75) : Qt.rgba(236
                                                                                        / root.colorChannelMaximum,
                                                                                        233 / root.colorChannelMaximum,
                                                                                        225 / root.colorChannelMaximum,
                                                                                        0.72)
    readonly property font regular: MusicFonts.regular
    readonly property font demibold: MusicFonts.demibold
    readonly property font bold: MusicFonts.bold
    readonly property real paletteDuration: reducedMotion ? 0 : root.paletteTransitionMilliseconds
                                                            / motionSpeed
    property real nightMix: night ? 1 : 0
    Behavior on ink {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on muted {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on line {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on accent {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on surface {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on page {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on veil {
        ColorAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
    Behavior on nightMix {
        NumberAnimation {
            duration: root.paletteDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.paletteCurve
        }
    }
}
