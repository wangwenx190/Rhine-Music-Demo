pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "components"
import "pages"

Rectangle {
    id: root
    required property LayoutMetrics metrics
    property bool night: false
    property bool probes: false
    property int probeMode: 0
    property string titleText: "收藏的声音"
    property string artistText: "本地音乐收藏"
    property string playingTitle: ""
    property int captureRevision: 0
    color: design.page
    DesignTokens {
        id: design
        night: root.night
        reducedMotion: true
    }
    Browse {
        anchors.fill: parent
        visible: !root.probes
        metrics: root.metrics
        tokens: design
        album: ({
                    title: root.titleText,
                    artist: root.artistText,
                    genre: "音乐",
                    year: "2026",
                    tracks: 12
                })
        albumCount: 27
        showNavigation: false
        playingTitle: root.playingTitle
        onThemeRequested: night => root.night = night
    }
    Loader {
        id: probeLoader
        objectName: "probeLoader"
        visible: root.probes
        active: root.probes
        width: 360
        height: 160
        sourceComponent: root.probeMode === 0 ? cpp : root.probeMode === 1 ? js : quick
    }
    // A readback needs a submitted frame even when the scene is otherwise static.
    // This marker lies outside the fixed 360x160 drawing comparison.
    Rectangle {
        visible: root.probes
        x: root.width - 1
        y: root.height - 1
        width: 1
        height: 1
        color: root.captureRevision % 2 ? "#111111" : "#222222"
    }
    Component {
        id: cpp
        CanvasProbe {
            objectName: "canvasCpp"
            night: root.night
        }
    }
    Component {
        id: js
        Canvas2DProbe {
            objectName: "canvasJs"
            night: root.night
        }
    }
    Component {
        id: quick
        Quick2DProbe {
            objectName: "canvasQuick"
            night: root.night
        }
    }
}
