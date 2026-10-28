// Composes the transport title and playback buttons with stable source hit targets.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: transport
    required property DesignTokens tokens
    property FrameClock clock: null
    required property bool portrait
    required property bool narrow
    required property real viewportWidth
    required property real headerWidth
    required property real navigationGap
    required property real navigationHeight
    property string playingTitle: ""
    property bool playing: false
    readonly property TitleReelMotion transportController: song.reelController
    readonly property real smallWidthPixels: 700
    readonly property real compactTransportReservedPixels: 610
    readonly property real desktopTransportReservedPixels: 760
    readonly property real smallTransportReservedPixels: 340
    readonly property real portraitTransportReservedPixels: 320
    readonly property real brandCapitalHeight: 104
    signal actionRequested(string action)

    objectName: "minimalTransport"
    readonly property real songCap: transport.portrait ? Math.max(0, Math.min(280,
                                                                              transport.viewportWidth
                                                                              - transport.portraitTransportReservedPixels)) :
                                                         transport.viewportWidth
                                                         <= transport.smallWidthPixels ? Math.max(0,
                                                                                                  Math.min(100,
                                                                                                           transport.viewportWidth
                                                                                                           - transport.smallTransportReservedPixels)) :
                                                                                         transport.narrow
                                                                                         ? Math.max(
                                                                                               0, Math.min(
                                                                                                   180, transport.viewportWidth
                                                                                                   - transport.compactTransportReservedPixels)) :
                                                                                           Math.max(0,
                                                                                                    Math.min(240,
                                                                                                             transport.viewportWidth
                                                                                                             - transport.desktopTransportReservedPixels))
    readonly property real songWidth: song.width
    width: transport.portrait ? transport.headerWidth - 28 - 28 - transport.brandCapitalHeight - 28
                                - transport.navigationGap * 4 : 12 + songWidth + 29 * 2 + 7
    height: 29
    y: (transport.navigationHeight - (height - (transport.portrait ? 0 : 5))) / 2 - (
           transport.portrait ? 0 : 5)
    Rectangle {
        antialiasing: true
        width: 1
        height: parent.height
        color: transport.tokens.line
    }
    TransportTitle {
        id: song
        objectName: "playingTitle"
        tokens: transport.tokens
        clock: transport.clock
        x: transport.portrait ? parent.width - 49 - transport.songWidth : 13
        title: transport.playingTitle
        cap: transport.songCap
        pixelSize: transport.viewportWidth <= transport.smallWidthPixels ? 10 : transport.narrow
                                                                           ? 11 : 12
        onLocateRequested: transport.actionRequested("locate-song")
    }
    TransportMotion {
        id: iconMotion
        clock: transport.clock
        playing: transport.playing
        reducedMotion: transport.tokens.reducedMotion
        active: transport.visible && (transport.tokens.offscreenRendering ||
                                      !transport.Window.window || transport.Window.window.visible)
    }
    ArchiveButton {
        objectName: "playButton"
        iconSize: transport.viewportWidth <= transport.smallWidthPixels ? 15 : 17
        opacity: hovered || transport.playing ? 1 : 0.8
        tokens: transport.tokens
        x: parent.width - width * 2 - (transport.portrait ? 1 : 6)
        width: transport.portrait ? 24 : 29
        iconName: ""
        text: transport.playing ? qsTranslate("Header", "Pause") : qsTranslate("Header", "Play")
        showText: false
        contentItem: Item {
            ArchiveVectorImage {
                anchors.centerIn: parent
                width: transport.viewportWidth <= transport.smallWidthPixels ? 15 : 17
                height: width
                opacity: 1 - iconMotion.pauseOpacity
                scale: .7 + .3 * opacity
                source: "qrc:/assets/icons/play-" + (transport.tokens.night ? "night" : "day")
                        + ".svg"
            }
            ArchiveVectorImage {
                anchors.centerIn: parent
                width: transport.viewportWidth <= transport.smallWidthPixels ? 15 : 17
                height: width
                opacity: iconMotion.pauseOpacity
                scale: .7 + .3 * opacity
                source: "qrc:/assets/icons/pause-" + (transport.tokens.night ? "night" : "day")
                        + ".svg"
            }
        }
        onClicked: transport.actionRequested("play-pause")
    }
    ArchiveButton {
        objectName: "stopButton"
        iconSize: transport.viewportWidth <= transport.smallWidthPixels ? 15 : 17
        opacity: hovered ? 1 : 0.8
        tokens: transport.tokens
        x: parent.width - width
        width: transport.portrait ? 24 : 29
        iconName: "stop"
        text: qsTranslate("Header", "Stop")
        showText: false
        onClicked: transport.actionRequested("stop")
    }
}
