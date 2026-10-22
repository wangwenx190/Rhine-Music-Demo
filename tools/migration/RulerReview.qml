pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "components"

Item {
    id: root
    required property RulerController controller
    property FrameClock clock: null
    property int ordinal: 123
    property bool offscreenRendering: false
    property bool night: false
    property int marker: 0
    width: 136
    height: 100
    DesignTokens {
        id: rulerDesign
        night: root.night
        reducedMotion: true
        offscreenRendering: root.offscreenRendering
    }
    Rectangle {
        anchors.fill: parent
        color: rulerDesign.page
    }
    Rectangle {
        x: 135
        width: 1
        height: 1
        color: root.marker % 2 ? "red" : "blue"
    }
    Ruler {
        x: 2
        y: 2
        controller: root.controller
        tokens: rulerDesign
    }
    OrdinalReel {
        objectName: "ordinalProbe"
        x: 2
        y: 40
        tokens: rulerDesign
        clock: root.clock
        value: root.ordinal
        digits: 3
        cssPixelSize: 34
        visible: root.controller.active
    }
}
