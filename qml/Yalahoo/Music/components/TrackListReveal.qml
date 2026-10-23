// Owns the virtual list's realized-item layout and reveal handoff. Identity, revision,
// scrolling trajectories and highlight state remain in TrackFocusController.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property ListView view
    property TrackFocusController trackFocus: null
    required property int trackCount
    required property real headerHeight
    signal revealStarted
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
        if (!trackFocus || revision !== trackFocus.revision || !view || !enabled)
            return;
        revealStarted();
        view.forceLayout();
        root.settleLayout(view);
        view.forceLayout();
        // Native virtual lists may rebase originY as variable-height rows are realized.
        // Query their actual extents, then animate in the same origin-relative space.
        const startRelative = view.contentY - view.originY;
        view.positionViewAtBeginning();
        view.forceLayout();
        root.settleLayout(view);
        view.forceLayout();
        const low = view.contentY - view.originY;
        view.positionViewAtEnd();
        view.forceLayout();
        root.settleLayout(view);
        view.forceLayout();
        const high = view.contentY - view.originY;
        view.positionViewAtIndex(index, ListView.Center);
        view.forceLayout();
        root.settleLayout(view);
        view.forceLayout();
        const row = view.itemAtIndex(index) as TrackRow;
        if (!row) {
            trackFocus.cancel();
            return;
        }
        view.contentY = startRelative + view.originY;
        trackFocus.beginListReveal(revision, startRelative, view.originY, low, high, row.y + row.trackTop,
                                   row.trackHeight, view.height, headerHeight);
    }
    function applyRevealScroll(): void {
        if (!trackFocus || !view || !enabled)
            return;
        view.forceLayout();
        const row = view.itemAtIndex(trackFocus.row) as TrackRow;
        if (row) {
            const last = view.itemAtIndex(trackCount - 1) as TrackRow;
            trackFocus.refineListReveal(view.originY, row.y + row.trackTop, row.trackHeight,
                                        view.height, headerHeight, last ? last.y + last.height
                                                                          - view.height
                                                                          - view.originY : null);
        }
        view.contentY = trackFocus.listScrollY(view.originY);
    }
    function focusRevealedRow(revision: double, index: int): void {
        if (!trackFocus || revision !== trackFocus.revision || !view || !enabled)
            return;
        view.forceLayout();
        applyRevealScroll();
        view.forceLayout();
        const row = view.itemAtIndex(index);
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
}
