pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property string playingTitle: ""
    property bool playing: false
    property OpeningController opening: null
    property FrameClock clock: null
    property OverviewController overviewController: null
    readonly property real chromeOpacity: overviewController ? overviewController.navigationOpacity :
                                                               1
    readonly property real chromeOffset: overviewController ? overviewController.navigationOffset :
                                                              0
    readonly property bool chromeEnabled: !overviewController || !overviewController.active
    readonly property TitleReelMotion transportController: song.reelController
    signal actionRequested(string action)
    signal themeRequested(bool night)
    readonly property bool narrow: metrics.width <= 1100
    readonly property real margin: metrics.portrait ? 20 : metrics.width >= 1700 ? 60 : narrow ? 25 :
                                                                                                 40

    readonly property real brandHeight: metrics.portrait ? 24 : metrics.width >= 1700 ? 39 : narrow
                                                                                        ? 25 : 31
    readonly property real navGap: metrics.portrait ? 6 : narrow ? 15 : metrics.width >= 1700 ? 30 :
                                                                                                25

    x: margin
    y: metrics.portrait ? 22 : metrics.width >= 1700 ? 48 : narrow ? 26 : 33
    width: metrics.width - margin * 2
    height: metrics.portrait ? 84 : brand.height
    objectName: "archiveHeader"
    Item {
        id: brand
        objectName: "brandContainer"
        width: root.brandHeight * 816 / 104
        height: root.brandHeight + 5 + (root.narrow ? 7 : root.metrics.width >= 1700 ? 11 : 9) * 1.5
        ArchiveVectorImage {
            objectName: "brandLogo"
            // Keep the capital-height slot and full SVG descent; no crop of the accents.
            y: root.brandHeight + 3 - root.brandHeight * 188 / 104
            width: parent.width
            height: root.brandHeight * 208 / 104
            source: "qrc:/assets/branding/logo-" + (root.tokens.night ? "dark" : "light") + ".svg"
        }
        ArchiveText {
            tokens: root.tokens
            y: root.brandHeight + 5
            text: qsTr("MUSIC ARCHIVE / PERSONAL MUSIC TERMINAL")
            cssPixelSize: root.narrow ? 7 : root.metrics.width >= 1700 ? 11 : 9
            cssLineHeight: cssPixelSize * 1.5
            font.letterSpacing: root.metrics.portrait ? 1 : 1.8
        }
        Accessible.role: Accessible.StaticText
        Accessible.name: root.tokens.displayName
    }
    Row {
        id: nav
        objectName: "headerNavigation"
        visible: !root.opening || root.opening.revealTime > 0
        opacity: root.opening ? root.opening.chrome(root.opening.revealTime, 0) : 1
        x: root.metrics.portrait ? 0 : root.width - width
        y: root.metrics.portrait ? brand.height + 12 : root.narrow ? 3 : 7
        spacing: root.navGap
        height: root.metrics.portrait ? 32 : root.narrow ? 30 : 31
        ArchiveButton {
            opacity: root.chromeOpacity
            enabled: root.chromeEnabled
            transform: Translate {
                y: root.chromeOffset
            }
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: qsTr("Library")
            iconName: "folder"
            width: root.metrics.portrait ? 28 : implicitWidth
            showText: !root.metrics.portrait
            cssPixelSize: root.narrow ? 11 : root.metrics.width >= 1700 ? 14 : 12
            onClicked: root.actionRequested("library")
        }
        ArchiveButton {
            opacity: root.chromeOpacity
            enabled: root.chromeEnabled
            transform: Translate {
                y: root.chromeOffset
            }
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: qsTr("Search")
            iconName: "search"
            width: root.metrics.portrait ? 28 : implicitWidth
            showText: !root.metrics.portrait
            cssPixelSize: root.narrow ? 11 : root.metrics.width >= 1700 ? 14 : 12
            onClicked: root.actionRequested("search")
        }
        ThemeSwitch {
            objectName: "themeSwitch"
            opacity: root.chromeOpacity
            enabled: root.chromeEnabled
            transform: Translate {
                y: root.chromeOffset
            }
            tokens: root.tokens
            width: root.metrics.portrait ? 104 : root.narrow ? 100 : 108
            height: root.metrics.portrait || !root.narrow ? 31 : 30
            y: (nav.height - height) / 2
            onThemeRequested: night => root.themeRequested(night)
        }
        ArchiveButton {
            opacity: root.chromeOpacity
            enabled: root.chromeEnabled
            transform: Translate {
                y: root.chromeOffset
            }
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: qsTr("Playback and quality settings")
            iconName: "settings"
            width: root.metrics.portrait ? 28 : 17
            showText: false
            onClicked: root.actionRequested("settings")
        }
        Item {
            id: transport
            objectName: "minimalTransport"
            readonly property real songCap: root.metrics.portrait ? Math.max(0, Math.min(280,
                                                                                         root.metrics.width
                                                                                         - 320)) : root.metrics.width
                                                                    <= 700 ? Math.max(0, Math.min(
                                                                                          100, root.metrics.width
                                                                                          - 340)) : root.narrow
                                                                             ? Math.max(0, Math.min(
                                                                                            180, root.metrics.width
                                                                                            - 610)) : Math.max(
                                                                                   0, Math.min(240,
                                                                                               root.metrics.width
                                                                                               - 760))
            readonly property real songWidth: song.width
            width: root.metrics.portrait ? root.width - 28 - 28 - 104 - 28 - root.navGap * 4 : 12
                                           + songWidth + 29 * 2 + 7
            height: 29
            y: (nav.height - (height - (root.metrics.portrait ? 0 : 5))) / 2 - (
                   root.metrics.portrait ? 0 : 5)
            Rectangle {
                antialiasing: true
                width: 1
                height: parent.height
                color: root.tokens.line
            }
            TransportTitle {
                id: song
                objectName: "playingTitle"
                tokens: root.tokens
                clock: root.clock
                x: root.metrics.portrait ? parent.width - 49 - transport.songWidth : 13
                title: root.playingTitle
                cap: transport.songCap
                pixelSize: root.metrics.width <= 700 ? 10 : root.narrow ? 11 : 12
                onLocateRequested: root.actionRequested("locate-song")
            }
            TransportMotion {
                id: iconMotion
                clock: root.clock
                playing: root.playing
                reducedMotion: root.tokens.reducedMotion
                active: root.visible && (root.tokens.offscreenRendering || !root.Window.window
                                         || root.Window.window.visible)
            }
            ArchiveButton {
                objectName: "playButton"
                iconSize: root.metrics.width <= 700 ? 15 : 17
                opacity: hovered || root.playing ? 1 : 0.8
                tokens: root.tokens
                x: parent.width - width * 2 - (root.metrics.portrait ? 1 : 6)
                width: root.metrics.portrait ? 24 : 29
                iconName: ""
                text: root.playing ? qsTr("Pause") : qsTr("Play")
                showText: false
                contentItem: Item {
                    ArchiveVectorImage {
                        anchors.centerIn: parent
                        width: root.metrics.width <= 700 ? 15 : 17
                        height: width
                        opacity: 1 - iconMotion.pauseOpacity
                        scale: .7 + .3 * opacity
                        source: "qrc:/assets/icons/play-" + (root.tokens.night ? "night" : "day")
                                + ".svg"
                    }
                    ArchiveVectorImage {
                        anchors.centerIn: parent
                        width: root.metrics.width <= 700 ? 15 : 17
                        height: width
                        opacity: iconMotion.pauseOpacity
                        scale: .7 + .3 * opacity
                        source: "qrc:/assets/icons/pause-" + (root.tokens.night ? "night" : "day")
                                + ".svg"
                    }
                }
                onClicked: root.actionRequested("play-pause")
            }
            ArchiveButton {
                objectName: "stopButton"
                iconSize: root.metrics.width <= 700 ? 15 : 17
                opacity: hovered ? 1 : 0.8
                tokens: root.tokens
                x: parent.width - width
                width: root.metrics.portrait ? 24 : 29
                iconName: "stop"
                text: qsTr("Stop")
                showText: false
                onClicked: root.actionRequested("stop")
            }
        }
    }
}
