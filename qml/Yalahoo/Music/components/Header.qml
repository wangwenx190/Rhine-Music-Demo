// Header geometry follows the final source CSS breakpoints in logical pixels. The transport
// title owns its width animation while play/stop hit targets stay anchored.

pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    // Source breakpoints and SVG view-box coordinates are independent of actual window DPR.
    readonly property real compactWidthPixels: 1100
    readonly property real wideWidthPixels: 1700
    readonly property real brandViewWidth: 816
    readonly property real brandCapitalHeight: 104
    readonly property real brandBaseline: 188
    readonly property real brandViewHeight: 208

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
    readonly property TitleReelMotion transportController: transport.transportController
    signal actionRequested(string action)
    signal themeRequested(bool night)
    readonly property bool narrow: metrics.width <= root.compactWidthPixels
    readonly property real margin: metrics.portrait ? 20 : metrics.width >= root.wideWidthPixels
                                                      ? 60 : narrow ? 25 : 40

    readonly property real brandHeight: metrics.portrait ? 24 : metrics.width
                                                           >= root.wideWidthPixels ? 39 : narrow
                                                                                     ? 25 : 31
    readonly property real navGap: metrics.portrait ? 6 : narrow ? 15 : metrics.width
                                                                   >= root.wideWidthPixels ? 30 : 25

    x: margin
    y: metrics.portrait ? 22 : metrics.width >= root.wideWidthPixels ? 48 : narrow ? 26 : 33
    width: metrics.width - margin * 2
    height: metrics.portrait ? 84 : brand.height
    objectName: "archiveHeader"
    Item {
        id: brand
        objectName: "brandContainer"
        width: root.brandHeight * root.brandViewWidth / root.brandCapitalHeight
        height: root.brandHeight + 5 + (root.narrow ? 7 : root.metrics.width >= root.wideWidthPixels
                                                      ? 11 : 9) * 1.5
        ArchiveVectorImage {
            objectName: "brandLogo"
            // Keep the capital-height slot and full SVG descent; no crop of the accents.
            y: root.brandHeight + 3 - root.brandHeight * root.brandBaseline
               / root.brandCapitalHeight

            width: parent.width
            height: root.brandHeight * root.brandViewHeight / root.brandCapitalHeight
            source: "qrc:/assets/branding/logo-" + (root.tokens.night ? "dark" : "light") + ".svg"
        }
        ArchiveText {
            tokens: root.tokens
            y: root.brandHeight + 5
            text: qsTr("MUSIC ARCHIVE / PERSONAL MUSIC TERMINAL")
            cssPixelSize: root.narrow ? 7 : root.metrics.width >= root.wideWidthPixels ? 11 : 9
            cssLineHeight: cssPixelSize * 1.5
            font.letterSpacing: root.metrics.portrait ? 1 : 1.8
        }
        Accessible.role: Accessible.StaticText
        Accessible.name: root.tokens.displayName
    }
    component NavigationButton: ArchiveButton {
        opacity: root.chromeOpacity
        enabled: root.chromeEnabled
        transform: Translate {
            y: root.chromeOffset
        }
        tokens: root.tokens
        height: root.metrics.portrait ? 32 : 17
        y: (nav.height - height) / 2
        width: root.metrics.portrait ? 28 : implicitWidth
        showText: !root.metrics.portrait
        cssPixelSize: root.narrow ? 11 : root.metrics.width >= root.wideWidthPixels ? 14 : 12
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
        NavigationButton {
            text: qsTr("Library")
            iconName: "folder"
            onClicked: root.actionRequested("library")
        }
        NavigationButton {
            text: qsTr("Search")
            iconName: "search"
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
            width: root.metrics.portrait ? root.brandCapitalHeight : root.narrow ? 100 : 108
            height: root.metrics.portrait || !root.narrow ? 31 : 30
            y: (nav.height - height) / 2
            onThemeRequested: night => root.themeRequested(night)
        }
        NavigationButton {
            text: qsTr("Playback and quality settings")
            iconName: "settings"
            width: root.metrics.portrait ? 28 : 17
            showText: false
            cssPixelSize: 16
            onClicked: root.actionRequested("settings")
        }
        HeaderTransport {
            id: transport
            tokens: root.tokens
            clock: root.clock
            portrait: root.metrics.portrait
            narrow: root.narrow
            viewportWidth: root.metrics.width
            headerWidth: root.width
            navigationGap: root.navGap
            navigationHeight: nav.height
            playingTitle: root.playingTitle
            playing: root.playing
            onActionRequested: action => root.actionRequested(action)
        }
    }
}
