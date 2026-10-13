pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property string playingTitle: ""
    property bool playing: false
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
        Image {
            objectName: "brandLogo"
            // Keep the capital-height slot and full SVG descent; no crop of the accents.
            y: root.brandHeight + 3 - root.brandHeight * 188 / 104
            width: parent.width
            height: root.brandHeight * 208 / 104
            source: "qrc:/assets/branding/logo-" + (root.tokens.night ? "dark" : "light") + ".svg"
            sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
        }
        ArchiveText {
            tokens: root.tokens
            y: root.brandHeight + 5
            text: "MUSIC ARCHIVE  ／  私人音乐终端"
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
        x: root.metrics.portrait ? 0 : root.width - width
        y: root.metrics.portrait ? brand.height + 12 : root.narrow ? 3 : 7
        spacing: root.navGap
        height: root.metrics.portrait ? 32 : root.narrow ? 30 : 31
        ArchiveButton {
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: "音乐库"
            iconName: "folder"
            width: root.metrics.portrait ? 28 : implicitWidth
            showText: !root.metrics.portrait
            cssPixelSize: root.narrow ? 11 : root.metrics.width >= 1700 ? 14 : 12
            onClicked: root.actionRequested("library")
        }
        ArchiveButton {
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: "搜索"
            iconName: "search"
            width: root.metrics.portrait ? 28 : implicitWidth
            showText: !root.metrics.portrait
            cssPixelSize: root.narrow ? 11 : root.metrics.width >= 1700 ? 14 : 12
            onClicked: root.actionRequested("search")
        }
        ThemeSwitch {
            objectName: "themeSwitch"
            tokens: root.tokens
            width: root.metrics.portrait ? 104 : root.narrow ? 100 : 108
            height: root.metrics.portrait || !root.narrow ? 31 : 30
            y: (nav.height - height) / 2
            onThemeRequested: night => root.themeRequested(night)
        }
        ArchiveButton {
            tokens: root.tokens
            height: root.metrics.portrait ? 32 : 17
            y: (nav.height - height) / 2
            text: "播放与画质设置"
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
                                                                                         - 320)) : root.narrow
                                                                    ? Math.max(0, Math.min(180,
                                                                                           root.metrics.width
                                                                                           - 610)) : Math.max(
                                                                          0, Math.min(240,
                                                                                      root.metrics.width
                                                                                      - 760))
            readonly property real songWidth: root.playingTitle.length ? Math.min(songCap,
                                                                                  song.implicitWidth
                                                                                  + 14) : 0
            width: root.metrics.portrait ? root.width - 28 - 28 - 104 - 28 - root.navGap * 4 : 12
                                           + songWidth + 29 * 2 + 7
            height: 29
            y: (nav.height - (height - (root.metrics.portrait ? 0 : 5))) / 2 - (
                   root.metrics.portrait ? 0 : 5)
            Rectangle {
                width: 1
                height: parent.height
                color: root.tokens.line
            }
            ArchiveText {
                id: song
                objectName: "playingTitle"
                tokens: root.tokens
                x: root.metrics.portrait ? parent.width - 48 - transport.songWidth : 12
                width: Math.max(0, transport.songWidth - 14)
                height: 29
                y: (nav.height - (height - (root.metrics.portrait ? 0 : 5))) / 2 - (
                       root.metrics.portrait ? 0 : 5)
                verticalAlignment: Text.AlignVCenter
                text: root.playingTitle
                elide: Text.ElideRight
                cssPixelSize: root.metrics.portrait ? 10 : root.narrow ? 11 : 12
            }
            ArchiveButton {
                objectName: "playButton"
                iconSize: root.metrics.width <= 700 ? 15 : 17
                opacity: hovered ? 1 : 0.8
                tokens: root.tokens
                x: parent.width - width * 2 - (root.metrics.portrait ? 1 : 6)
                width: root.metrics.portrait ? 24 : 29
                iconName: root.playing ? "pause" : "play"
                text: root.playing ? "暂停" : "播放"
                showText: false
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
                text: "停止"
                showText: false
                onClicked: root.actionRequested("stop")
            }
        }
    }
}
