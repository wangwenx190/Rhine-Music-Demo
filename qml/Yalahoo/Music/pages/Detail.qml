// Composes the selected album presentation from the C++ detail model. Readiness gates
// visibility; tab geometry and virtualized content remain tied to the active presentation
// revision.

pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

FocusScope {
    id: root
    // Responsive geometry follows the final source detail CSS and actual logical content width.
    readonly property real portraitInsetRatio: 0.04
    readonly property real portraitTitleHeightRatio: 0.54
    readonly property real wideWidthPixels: 1700

    required property LayoutMetrics metrics
    required property DesignTokens tokens
    required property PresentationController presentation
    required property DetailModel detailModel
    property TrackFocusController trackFocus: null
    property string playingTrackId: ""
    property Item sceneSource: null
    property int activeTab: 0
    property int queuedLayoutRevision: -1
    property bool layoutPending: false
    function setTab(tab: int): void {
        changeTab(tab, false);
    }
    function changeTab(tab: int, reveal: bool): void {
        if (activeTab === tab)
            return;
        if (!reveal && trackFocus)
            trackFocus.cancel();
        tracks.saveScroll();
        presentation.revealTabContent();
        activeTab = tab;
    }
    function focusDetailIfReady(): void {
        if (presentation.detailEnabled)
            forceActiveFocus(Qt.OtherFocusReason);
    }
    function revealAfterLayout(revision: double, index: int): void {
        if (trackFocus && revision === trackFocus.revision)
            tracks.reveal(revision, index);
    }
    function finishLayout(): void {
        const revision = queuedLayoutRevision;
        layoutPending = false;
        tracks.forceLayout();
        tracks.positionViewAtBeginning();
        presentation.layoutReady(revision);
    }
    readonly property var album: detailModel.album
    readonly property real contentLeft: metrics.portrait ? Math.max(22, Math.min(42, width
                                                                                 * root.portraitInsetRatio)) :
                                                           width * 0.52
    readonly property real contentRight: metrics.portrait ? Math.max(18, Math.min(42, width * root.portraitInsetRatio)) : width
                                                            * (width <= 1100
                                                               ? root.portraitInsetRatio : 0.045)
    readonly property real contentTop: metrics.portrait ? height * 0.5 : width
                                                          >= root.wideWidthPixels ? 210 : width
                                                                                    <= 1100 ? 132 :
                                                                                              170
    visible: presentation.detailSurface.shown
    opacity: presentation.detailSurface.opacity
    enabled: presentation.detailEnabled
    signal backRequested
    signal trackRequested(string trackId)
    Keys.onEscapePressed: event => {
        root.backRequested();
        event.accepted = true;
    }

    Connections {
        target: root.presentation
        function onPrepared() {
            root.activeTab = 0;
            tracks.positionViewAtBeginning();
            root.queuedLayoutRevision = root.presentation.layoutRevision;
            // Keep one layout callback; new preparations replace its revision.
            if (!root.layoutPending) {
                root.layoutPending = true;
                Qt.callLater(root.finishLayout);
            }
        }
        function onFocusRequested(detail) {
            if (detail) {
                Qt.callLater(root.focusDetailIfReady);
            }
        }
    }
    Connections {
        target: root.trackFocus
        function onRevealRequested(revision, index) {
            root.changeTab(0, true);
            Qt.callLater(root.revealAfterLayout, revision, index);
        }
    }
    Rectangle {
        id: portraitBackdrop
        antialiasing: true
        visible: root.metrics.portrait
        y: root.height * 0.46
        width: root.width
        height: root.height - y
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.rgba(root.tokens.page.r, root.tokens.page.g, root.tokens.page.b, 0)
            }
            GradientStop {
                position: Math.min(1, 34 / Math.max(1, root.height * root.portraitTitleHeightRatio))
                color: root.tokens.page
            }
        }
    }
    ArchiveButton {
        objectName: "detailBack"
        tokens: root.tokens
        x: {
            if (root.metrics.portrait || root.width <= 700)
                return 22;
            if (root.width >= root.wideWidthPixels)
                return 62;
            return root.width <= 1100 ? 27 : 42;
        }
        y: {
            if (root.metrics.portrait)
                return 118;
            if (root.width <= 700)
                return 87;
            if (root.width >= root.wideWidthPixels)
                return 144;
            return root.width <= 1100 ? 91 : 112;
        }
        text: qsTr("\u2190 Back to shelf     Esc")
        cssPixelSize: root.width <= 700 ? 9 : root.width >= root.wideWidthPixels ? 12 : 10
        onClicked: root.backRequested()
    }
    Column {
        objectName: "detailCaption"
        x: root.metrics.portrait ? root.contentLeft : root.width <= 1100 ? 27 : 42
        y: root.metrics.portrait ? root.height * 0.43 : root.height - 82 - height
        spacing: root.width <= 700 || root.metrics.portrait ? 5 : 10
        ArchiveText {
            tokens: root.tokens
            text: qsTr("ALBUM %1").arg((root.album.code || 1).toString().padStart(3, "0"))
            cssPixelSize: root.width <= 700 ? 9 : 12
            font.letterSpacing: 2
        }
        ArchiveText {
            visible: !root.metrics.portrait && root.width > 700
            tokens: root.tokens
            text: qsTr("Drag the card to view the full cover")
            cssPixelSize: root.width <= 1100 ? 8 : 9
            color: root.tokens.muted
        }
    }
    TrackList {
        id: tracks
        objectName: "detailTrackList"
        tokens: root.tokens
        detailModel: root.detailModel
        trackFocus: root.trackFocus
        viewportWidth: root.width
        playingTrackId: root.playingTrackId
        about: root.activeTab === 1
        contentOpacity: root.presentation.tabOpacity
        readonly property real tabHeight: tabBar.height
        x: root.contentLeft + root.presentation.detailSurface.offset
        y: root.contentTop
        width: Math.max(0, root.width - root.contentLeft - root.contentRight - (root.metrics.portrait
                                                                                ? 5 : 9))
        height: Math.max(0, root.height - y - (root.metrics.portrait ? 28 : root.width
                                                                       >= root.wideWidthPixels ? 75 :
                                                                                                 root.width
                                                                                                 <= 1100 ? 40 :
                                                                                                           52))
        onTrackRequested: trackId => root.trackRequested(trackId)
        header: DetailHeader {
            width: tracks.contentWidth
            tokens: root.tokens
            presentation: root.presentation
            album: root.album
            viewportWidth: root.width
            tabHeight: tabBar.height
            activeTab: root.activeTab
            trackCount: root.detailModel.count
            onHeightChanged: {
                if (tracks.preserveBeginning)
                    Qt.callLater(tracks.restoreBeginning);
            }
        }
        DetailTabs {
            id: tabBar
            tokens: root.tokens
            presentation: root.presentation
            activeTab: root.activeTab
            viewportWidth: root.width
            viewportHeight: tracks.height
            tabsY: tracks.tabsY
            width: tracks.contentWidth
            portrait: root.metrics.portrait
            portraitSource: portraitBackdrop
            portraitY: portraitBackdrop.y
            sceneSource: root.sceneSource
            contentSource: tracks.currentView
            sceneX: tracks.x
            sceneY: tracks.y
            onTabRequested: tab => root.setTab(tab)
        }
    }
}
