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
    function settleLayout(item: Item): void {
        // Preserve child-first, sibling-order polishing without a recursive JS call stack.
        const ancestors = [
                  {
                      node: item,
                      children: item.children,
                      next: 0
                  }
              ];
        while (ancestors.length > 0) {
            const branch = ancestors[ancestors.length - 1];
            if (branch.next < branch.children.length) {
                const child = branch.children[branch.next++];
                ancestors.push({
                                   node: child,
                                   children: child.children,
                                   next: 0
                               });
            } else {
                branch.node.ensurePolished();
                ancestors.pop();
            }
        }
    }
    function reveal(revision: double, index: int): void {
        if (!trackFocus || revision !== trackFocus.revision || !currentView || about)
            return;
        preserveBeginning = false;
        currentView.forceLayout();
        root.settleLayout(root);
        currentView.forceLayout();
        // Native virtual lists may rebase originY as variable-height rows are realized.
        // Query their actual extents, then animate in the same origin-relative space.
        const startRelative = currentView.contentY - currentView.originY;
        currentView.positionViewAtBeginning();
        currentView.forceLayout();
        root.settleLayout(root);
        currentView.forceLayout();
        const low = currentView.contentY - currentView.originY;
        currentView.positionViewAtEnd();
        currentView.forceLayout();
        root.settleLayout(root);
        currentView.forceLayout();
        const high = currentView.contentY - currentView.originY;
        currentView.positionViewAtIndex(index, ListView.Center);
        currentView.forceLayout();
        root.settleLayout(root);
        currentView.forceLayout();
        const row = currentView.itemAtIndex(index) as TrackRow;
        if (!row) {
            trackFocus.cancel();
            return;
        }
        currentView.contentY = startRelative + currentView.originY;
        trackFocus.beginListReveal(revision, startRelative, currentView.originY, low, high, row.y + row.trackTop,
                                   row.trackHeight, height, layoutHeader ? layoutHeader.tabsHeight :
                                                                           0);
    }
    function applyRevealScroll(): void {
        if (!trackFocus || !currentView)
            return;
        currentView.forceLayout();
        const row = currentView.itemAtIndex(trackFocus.row) as TrackRow;
        if (row) {
            const last = currentView.itemAtIndex(detailModel.count - 1) as TrackRow;
            trackFocus.refineListReveal(currentView.originY, row.y + row.trackTop, row.trackHeight,
                                        height, layoutHeader ? layoutHeader.tabsHeight : 0, last
                                        ? last.y + last.height - height - currentView.originY :
                                          null);
        }
        currentView.contentY = trackFocus.listScrollY(currentView.originY);
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
    function focusRevealedRow(revision: double, index: int): void {
        if (!trackFocus || revision !== trackFocus.revision || !currentView || about)
            return;
        currentView.forceLayout();
        applyRevealScroll();
        currentView.forceLayout();
        const row = currentView.itemAtIndex(index);
        if (row)
            row.forceActiveFocus(Qt.OtherFocusReason);
    }
    Connections {
        target: root.trackFocus
        function onScrollChanged() {
            root.applyRevealScroll();
        }
        function onFocusRequested(index) {
            Qt.callLater(root.focusRevealedRow, root.trackFocus.revision, index);
        }
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
