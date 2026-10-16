pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property OpeningController opening: null
    property RulerController rulerController: null
    property InputRouter inputRouter: null
    property FrameClock clock: null
    property SelectionController selection: null
    property OverviewController overviewController: null
    function focusOverviewToggle() {
        overviewToggle.forceActiveFocus(Qt.OtherFocusReason);
    }
    property var album: ({})
    property int albumCount: 0
    property bool playing: false
    property string playingTitle: ""
    property bool showNavigation: true
    property bool contentEnabled: true
    property SurfaceTransition surface: null
    readonly property TitleReelMotion titleController: info.titleController
    readonly property TitleReelMotion transportController: header.transportController
    signal actionRequested(string action)
    signal themeRequested(bool night)
    ReadingVeil {
        opacity: (root.surface ? root.surface.opacity : 1) * (root.opening ? root.opening.chrome(
                                                                                 root.opening.revealTime,
                                                                                 0) : 1)
        enabled: root.contentEnabled && (!root.overviewController ||
                                         !root.overviewController.active)
        visible: (!root.surface || root.surface.shown) && (!root.opening || root.opening.revealTime
                                                           > 0)

        anchors.fill: parent
        metrics: root.metrics
        tokens: root.tokens
        calloutTop: info.y
        calloutHeight: info.height
    }
    ArchiveButton {
        id: overviewToggle
        objectName: "overviewToggle"
        tokens: root.tokens
        x: root.metrics.width <= 600 ? 22 : 40
        y: (root.metrics.width <= 600 ? 164 : 150) + (root.overviewController
                                                      ? root.overviewController.navigationOffset :
                                                        0)
        text: "缩略图模式     V"
        iconName: "overview"
        cssPixelSize: 12
        height: 37
        visible: (!root.opening || root.opening.revealTime > 0) && root.overviewController !== null
                 && root.albumCount > 0 && root.overviewController.navigationOpacity > 0
        enabled: root.contentEnabled && root.overviewController !== null &&
                 !root.overviewController.active
        opacity: (root.overviewController ? root.overviewController.navigationOpacity : 1) * (
                     root.opening ? root.opening.chrome(root.opening.revealTime, 0.11) : 1)
        onClicked: root.overviewController.toggle()
        Rectangle {
            y: parent.height - 1
            width: parent.width
            height: 1
            color: root.tokens.line
        }
    }
    Header {
        id: header
        metrics: root.metrics
        tokens: root.tokens
        opening: root.opening
        clock: root.clock
        playingTitle: root.playingTitle
        playing: root.playing
        overviewController: root.overviewController
        onActionRequested: action => root.actionRequested(action)
        onThemeRequested: night => root.themeRequested(night)
    }
    BrowseInfo {
        id: info
        transform: Translate {
            y: root.opening ? 8 * (1 - root.opening.chrome(root.opening.revealTime, 0.06)) : 0
        }
        opacity: (root.surface ? root.surface.opacity : 1) * (root.opening ? root.opening.chrome(
                                                                                 root.opening.revealTime,
                                                                                 0.06) : 1)
        enabled: root.contentEnabled && (!root.overviewController ||
                                         !root.overviewController.active)
        visible: (!root.surface || root.surface.shown) && (!root.opening || root.opening.revealTime
                                                           > 0)
        metrics: root.metrics
        tokens: root.tokens
        album: root.album
        ordinal: (root.album.code || 1).toString().padStart(3, "0")
        clock: root.clock
        ordinalDirection: root.inputRouter ? root.inputRouter.navigationDirection : 0
        calloutBottom: navigation.bottomGap + navigation.height + 28
        onOpenRequested: root.actionRequested("open")
    }
    Footer {
        opacity: (root.surface ? root.surface.opacity : 1) * (root.opening ? root.opening.chrome(
                                                                                 root.opening.revealTime,
                                                                                 0.09) : 1)
        enabled: root.contentEnabled && (!root.overviewController ||
                                         !root.overviewController.active)
        visible: (!root.surface || root.surface.shown) && (!root.opening || root.opening.revealTime
                                                           > 0)

        metrics: root.metrics
        tokens: root.tokens
        albumCount: root.albumCount
    }
    NavigationFrame {
        id: navigation
        opacity: (root.surface ? root.surface.opacity : 1) * (root.opening ? root.opening.chrome(
                                                                                 root.opening.revealTime,
                                                                                 0.11) : 1)
        enabled: root.contentEnabled && (!root.overviewController ||
                                         !root.overviewController.active)
        metrics: root.metrics
        tokens: root.tokens
        albumCount: root.album.columnCount || root.albumCount
        ordinal: (root.album.number || 1).toString().padStart(2, "0")
        rulerController: root.rulerController
        inputRouter: root.inputRouter
        clock: root.clock
        selection: root.selection
        columnLabel: root.album.columnLabel || ""
        columnNumber: root.album.columnNumber || 1
        columnCount: root.album.columnTotal || 1
        visible: root.showNavigation && (!root.surface || root.surface.shown) && (!root.opening
                                                                                  || root.opening.revealTime
                                                                                  > 0)
    }
}
