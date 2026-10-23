// Virtualized tracks and credits use C++ identity and focus state. Layout completion precedes
// saved-scroll restoration so estimated delegate heights cannot move the requested row.

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

Item {
    id: root
    // Credit packing and scrollbar occupancy are logical CSS geometry, not native control metrics.
    readonly property real creditColumnStridePixels: 236
    readonly property real creditColumnGapPixels: 26
    readonly property real scrollbarOccupancyPixels: 11

    clip: true
    property real contentOpacity: 1
    required property DesignTokens tokens
    required property DetailModel detailModel
    property TrackFocusController trackFocus: null
    property bool about: false
    property real viewportWidth: width
    readonly property real columnGap: viewportWidth <= 1100 ? 6 : 9
    readonly property real numberWidth: viewportWidth <= 700 ? 21 : viewportWidth <= 1100 ? 20 : 23
    readonly property real formatWidth: viewportWidth <= 700 ? 31 : viewportWidth <= 1100 ? 30 : 40
    readonly property real durationWidth: viewportWidth <= 1100 ? 34 : 38
    property string playingTrackId: ""
    signal trackRequested(string trackId)
    property Component header: null
    property bool preserveBeginning: true
    readonly property ListView currentView: view.item as ListView
    property real savedScroll: 0
    readonly property var layoutHeader: currentView ? currentView.headerItem : null
    // Resolve thin-scrollbar occupancy after layout, outside the width binding.
    property bool scrollable: false
    readonly property real contentWidth: Math.max(0, width - (scrollable
                                                              ? root.scrollbarOccupancyPixels : 0))
    function updateScrollbar(): void {
        scrollable = currentView && currentView.contentHeight > currentView.height;
    }
    readonly property real tabsY: layoutHeader ? layoutHeader.y - currentView.contentY
                                                 + layoutHeader.tabsY : 0
    readonly property var producerGroups: detailModel.album.producerGroups || []
    readonly property int creditColumns: Math.max(1, Math.min(producerGroups.length, Math.floor((contentWidth
                                                                                                 + root.creditColumnGapPixels)
                                                                                                / root.creditColumnStridePixels)))
    function saveScroll(): void {
        savedScroll = currentView ? currentView.contentY - currentView.originY : 0;
    }
    // Reconcile originY after forceLayout: virtualized rows may change the estimated content extent.
    function restoreScroll(): void {
        if (!currentView)
            return;
        currentView.forceLayout();
        if (preserveBeginning)
            currentView.positionViewAtBeginning();
        else
            currentView.contentY = currentView.originY + Math.min(savedScroll, Math.max(0,
                                                                                        currentView.contentHeight
                                                                                        - currentView.height));
    }
    function forceLayout(): void {
        if (currentView)
            currentView.forceLayout();
    }
    function positionViewAtBeginning(): void {
        preserveBeginning = true;
        restoreBeginning();
    }
    function restoreBeginning(): void {
        if (!preserveBeginning)
            return;
        if (currentView)
            currentView.positionViewAtBeginning();
    }
    function positionViewAtIndex(index: int, mode: int): void {
        preserveBeginning = false;
        if (currentView)
            currentView.positionViewAtIndex(index, mode);
    }
    function reveal(revision: double, index: int): void {
        revealProtocol.reveal(revision, index);
    }
    TrackListReveal {
        id: revealProtocol
        objectName: "trackListReveal"
        view: root.currentView
        trackFocus: root.trackFocus
        trackCount: root.detailModel.count
        headerHeight: root.layoutHeader ? root.layoutHeader.tabsHeight : 0
        enabled: !root.about && root.visible
        onRevealStarted: root.preserveBeginning = false
    }
    function cancelReveal(): void {
        if (trackFocus)
            trackFocus.cancel();
    }
    onWidthChanged: if (trackFocus && trackFocus.revealing)
                        cancelReveal()
    onHeightChanged: if (trackFocus && trackFocus.revealing)
                         cancelReveal()
    onVisibleChanged: if (!visible && trackFocus && trackFocus.revealing)
                          cancelReveal()
    Keys.onPressed: event => {
        cancelReveal();
        event.accepted = false;
    }
    WheelHandler {
        onWheel: event => {
            root.cancelReveal();
            event.accepted = false;
        }
    }
    TapHandler {
        onPressedChanged: if (pressed)
                              root.cancelReveal()
        gesturePolicy: TapHandler.DragThreshold
    }
    Loader {
        id: view
        anchors.fill: parent
        sourceComponent: root.about ? aboutView : tracksView
        onLoaded: Qt.callLater(root.restoreScroll)
    }
    component ContentList: ListView {
        onContentHeightChanged: Qt.callLater(root.updateScrollbar)
        onHeightChanged: Qt.callLater(root.updateScrollbar)
        clip: true
        reuseItems: true
        cacheBuffer: 100
        boundsBehavior: Flickable.StopAtBounds
        header: root.header
        onMovementStarted: {
            root.preserveBeginning = false;
            root.cancelReveal();
        }
        onCurrentIndexChanged: if (currentIndex > 0)
                                   root.preserveBeginning = false
        T.ScrollBar.vertical: ArchiveScrollBar {
            tokens: root.tokens
            policy: T.ScrollBar.AsNeeded
            onPressedChanged: if (pressed)
                                  root.preserveBeginning = false
        }
    }
    Component {
        id: tracksView
        ContentList {
            model: root.detailModel
            delegate: trackDelegate
        }
    }
    Component {
        id: aboutView
        ContentList {
            model: Math.ceil(root.producerGroups.length / root.creditColumns)
            delegate: creditDelegate
        }
    }
    Component {
        id: trackDelegate
        TrackRow {
            tokens: root.tokens
            detailModel: root.detailModel
            trackFocus: root.trackFocus
            playingTrackId: root.playingTrackId
            viewportWidth: root.viewportWidth
            contentWidth: root.contentWidth
            columnGap: root.columnGap
            numberWidth: root.numberWidth
            formatWidth: root.formatWidth
            durationWidth: root.durationWidth
            opacity: root.contentOpacity
            onTrackRequested: trackId => root.trackRequested(trackId)
        }
    }

    Component {
        id: creditDelegate
        Row {
            id: creditRow
            opacity: root.contentOpacity
            required property int index
            width: root.contentWidth
            spacing: root.creditColumnGapPixels
            Repeater {
                model: Math.min(root.creditColumns, root.producerGroups.length - creditRow.index
                                * root.creditColumns)
                delegate: CreditCard {
                    required property int index
                    tokens: root.tokens
                    credit: root.producerGroups[creditRow.index * root.creditColumns + index]
                    viewportHeight: root.height
                    width: (root.contentWidth - root.creditColumnGapPixels * (root.creditColumns
                                                                              - 1)) / root.creditColumns
                }
            }
        }
    }
}
