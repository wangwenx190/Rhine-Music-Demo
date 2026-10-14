pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "components"

Rectangle {
    id: root
    required property TitleReelMotion controller
    required property LayoutMetrics metrics
    required property FrameClock clock
    property int mode: 0
    property bool night: false
    property int marker: 0
    property bool headerMode: false
    property bool testReduced: true
    property string song: ""
    property bool playing: false
    property string lastAction: ""
    function fullTitleNames(control) {
        return control.ToolTip.text === root.song && control.Accessible.name === "定位歌曲："
                + root.song;
    }
    color: titleDesign.page
    DesignTokens {
        id: titleDesign
        night: root.night
        reducedMotion: root.testReduced
        motionSpeed: root.clock.speed
    }
    TitleReel {
        objectName: "reviewTitle"
        tokens: titleDesign
        sharedController: root.controller
        width: root.width - 2
        height: root.height - 2
        visible: !root.headerMode && root.mode !== 2
        quickTextCandidate: root.mode === 1
    }
    CanvasTitleProbe {
        controller: root.controller
        ink: titleDesign.ink
        muted: titleDesign.muted
        width: root.width - 2
        height: root.height - 2
        visible: !root.headerMode && root.mode === 2
    }
    Header {
        visible: root.headerMode
        metrics: root.metrics
        tokens: titleDesign
        clock: root.clock
        playingTitle: root.song
        playing: root.playing
        onActionRequested: action => root.lastAction = action
    }
    Rectangle {
        x: root.width - 1
        y: root.height - 1
        width: 1
        height: 1
        color: root.marker % 2 ? "red" : "blue"
    }
}
