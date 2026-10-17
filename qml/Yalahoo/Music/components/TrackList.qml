pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

Item {
    id: root
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
    readonly property real contentWidth: Math.max(0, width - (scrollable ? 11 : 0))
    function updateScrollbar(): void {
        scrollable = currentView && currentView.contentHeight > currentView.height;
    }
    readonly property real tabsY: layoutHeader ? layoutHeader.y - currentView.contentY
                                                 + layoutHeader.tabsY : 0
    readonly property var producerGroups: detailModel.album.producerGroups || []
    readonly property int creditColumns: Math.max(1, Math.min(producerGroups.length, Math.floor((contentWidth
                                                                                                 + 26) / 236)))
    function saveScroll(): void {
        savedScroll = currentView ? currentView.contentY - currentView.originY : 0;
    }
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
        for (const child of item.children)
            root.settleLayout(child);
        item.ensurePolished();
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
                                          Number.NaN);
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
    Component {
        id: tracksView
        ListView {
            onContentHeightChanged: Qt.callLater(root.updateScrollbar)
            onHeightChanged: Qt.callLater(root.updateScrollbar)
            clip: true
            reuseItems: true
            cacheBuffer: 100
            boundsBehavior: Flickable.StopAtBounds
            model: root.detailModel
            header: root.header
            onMovementStarted: {
                root.preserveBeginning = false;
                root.cancelReveal();
            }
            onCurrentIndexChanged: if (currentIndex > 0)
                                       root.preserveBeginning = false
            delegate: trackDelegate
            T.ScrollBar.vertical: ArchiveScrollBar {
                tokens: root.tokens
                policy: T.ScrollBar.AsNeeded
                onPressedChanged: if (pressed)
                                      root.preserveBeginning = false
            }
        }
    }
    Component {
        id: aboutView
        ListView {
            onContentHeightChanged: Qt.callLater(root.updateScrollbar)
            onHeightChanged: Qt.callLater(root.updateScrollbar)
            clip: true
            reuseItems: true
            cacheBuffer: 100
            boundsBehavior: Flickable.StopAtBounds
            model: Math.ceil(root.producerGroups.length / root.creditColumns)
            header: root.header
            onMovementStarted: {
                root.preserveBeginning = false;
                root.cancelReveal();
            }
            onCurrentIndexChanged: if (currentIndex > 0)
                                       root.preserveBeginning = false
            delegate: creditDelegate
            T.ScrollBar.vertical: ArchiveScrollBar {
                tokens: root.tokens
                policy: T.ScrollBar.AsNeeded
                onPressedChanged: if (pressed)
                                      root.preserveBeginning = false
            }
        }
    }
    Component {
        id: trackDelegate
        TrackRow {}
    }
    component TrackRow: Item {
        id: row
        objectName: "trackDelegate"
        opacity: root.contentOpacity
        required property int index
        required property string trackId
        required property string number
        required property string trackTitle
        required property string artist
        required property string format
        required property string duration
        required property int disc
        readonly property real trackTop: discHeading ? discLabel.implicitHeight + 28 : 0
        readonly property real trackHeight: height - trackTop - 1
        required property bool discHeading
        activeFocusOnTab: true
        Keys.onReturnPressed: root.trackRequested(trackId)
        Keys.onSpacePressed: root.trackRequested(trackId)
        Accessible.role: Accessible.ListItem
        Accessible.name: number + " " + trackTitle + " " + duration
        width: root.contentWidth
        height: names.implicitHeight + (root.viewportWidth >= 1700 ? 28 : 22) + 1 + (discHeading
                                                                                     ? discLabel.implicitHeight
                                                                                       + 28 : 0)
        ArchiveText {
            id: discLabel
            tokens: root.tokens
            visible: row.discHeading
            text: qsTr("DISC %1").arg(row.disc.toString().padStart(2, "0"))
            cssPixelSize: 8
            color: root.tokens.muted
            y: 20
        }
        Rectangle {
            y: row.discHeading ? discLabel.implicitHeight + 28 : 0
            width: parent.width
            height: parent.height - y - 1
            color: row.trackId === (root.trackFocus ? root.trackFocus.trackId : "") ? Qt.rgba(
                                                                                          root.tokens.accent.r,
                                                                                          root.tokens.accent.g,
                                                                                          root.tokens.accent.b,
                                                                                          .2 * root.trackFocus.highlight) :
                                                                                      hit.hovered
                                                                                      ? root.tokens.veil :
                                                                                        "transparent"
            Rectangle {
                antialiasing: true
                width: 3
                height: parent.height
                color: root.tokens.accent
                opacity: root.trackFocus && row.trackId === root.trackFocus.trackId
                         ? root.trackFocus.highlight : 0
            }
            ArchiveText {
                tokens: root.tokens
                x: 0
                anchors.verticalCenter: parent.verticalCenter
                text: row.number
                cssPixelSize: root.viewportWidth >= 1700 ? 11 : 9
                color: root.tokens.muted
            }
            Column {
                id: names
                x: root.numberWidth + root.columnGap
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - x - root.formatWidth - root.durationWidth - root.columnGap
                                * 2)
                spacing: 5
                ArchiveText {
                    id: trackTitleText
                    tokens: root.tokens
                    width: parent.width
                    objectName: "detailTrackTitle"
                    text: row.trackTitle
                    cssPixelSize: root.viewportWidth >= 1700 ? 14 : root.viewportWidth <= 1100
                                                               && root.viewportWidth > 700 ? 10 : 11
                    cssLineHeight: root.detailModel.normalLineHeight(row.trackTitle, font)
                                   * fontScale
                    elide: Text.ElideRight
                    color: row.trackId === root.playingTrackId ? root.tokens.accent :
                                                                 root.tokens.ink
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    objectName: "detailTrackArtist"
                    text: row.artist
                    cssPixelSize: root.viewportWidth >= 1700 ? 10 : 8
                    color: root.tokens.muted
                    elide: Text.ElideRight
                }
            }
            ArchiveText {
                tokens: root.tokens
                x: parent.width - root.durationWidth - root.columnGap - root.formatWidth
                width: root.formatWidth
                anchors.verticalCenter: parent.verticalCenter
                text: row.format
                cssPixelSize: 7
                horizontalAlignment: Text.AlignRight
                color: root.tokens.muted
                elide: Text.ElideRight
            }
            ArchiveText {
                tokens: root.tokens
                x: parent.width - root.durationWidth
                width: root.durationWidth
                anchors.verticalCenter: parent.verticalCenter
                text: row.duration
                cssPixelSize: root.viewportWidth >= 1700 ? 11 : 9
                horizontalAlignment: Text.AlignRight
                color: root.tokens.muted
            }
            Rectangle {
                antialiasing: true
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -1
                width: parent.width
                height: 1
                color: root.tokens.line
            }
            HoverHandler {
                id: hit
            }
            TapHandler {
                enabled: !root.detailModel.album.offline
                onTapped: root.trackRequested(row.trackId)
            }
            Accessible.role: Accessible.ListItem
            Accessible.name: row.number + " " + row.trackTitle + " " + row.duration
        }
    }
    Component {
        id: creditDelegate
        Row {
            id: creditRow
            opacity: root.contentOpacity
            required property int index
            width: root.contentWidth
            spacing: 26
            Repeater {
                model: Math.min(root.creditColumns, root.producerGroups.length - creditRow.index
                                * root.creditColumns)
                delegate: Column {
                    id: person
                    objectName: "detailCredit"
                    required property int index
                    readonly property var credit: root.producerGroups[creditRow.index
                                                                      * root.creditColumns + index]
                    property bool expanded: false
                    onCreditChanged: expanded = false
                    width: (root.contentWidth - 26 * (root.creditColumns - 1)) / root.creditColumns
                    spacing: 4
                    bottomPadding: 18
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        text: person.credit.name
                        cssPixelSize: 14
                        cssLineHeight: cssPixelSize * 1.6
                        wrapMode: Text.WrapAnywhere
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        text: person.credit.roles.join(" \u00b7 ")
                        cssPixelSize: 12
                        cssLineHeight: cssPixelSize * 1.7
                        wrapMode: Text.WrapAnywhere
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        text: qsTr("Source: %1").arg(person.credit.sources.join(" \u00b7 "))
                        cssPixelSize: 10
                        cssLineHeight: cssPixelSize * 1.5
                        wrapMode: Text.WrapAnywhere
                        color: root.tokens.muted
                    }
                    ArchiveButton {
                        objectName: "detailCreditExpand"
                        tokens: root.tokens
                        visible: person.credit.tracks.length > 1
                        text: (person.expanded ? qsTr("Hide credited tracks \u2212") : qsTr(
                                                     "Show credited tracks +"))
                        cssPixelSize: 11
                        onClicked: person.expanded = !person.expanded
                        Accessible.expanded: person.expanded
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        visible: person.credit.tracks.length === 1
                        text: person.credit.tracks.length === 1 ? person.credit.tracks[0] : ""
                        cssPixelSize: 11
                        cssLineHeight: cssPixelSize * 1.7
                        wrapMode: Text.WrapAnywhere
                    }
                    ListView {
                        visible: person.expanded && person.credit.tracks.length > 1
                        width: parent.width
                        height: visible ? Math.min(root.height * 0.7, person.credit.tracks.length * 80) :
                                          0
                        clip: true
                        reuseItems: true
                        cacheBuffer: 100
                        boundsBehavior: Flickable.StopAtBounds
                        model: visible ? person.credit.tracks : []
                        delegate: ArchiveText {
                            required property string modelData
                            tokens: root.tokens
                            width: ListView.view.width
                            text: modelData
                            cssPixelSize: 11
                            cssLineHeight: cssPixelSize * 1.7
                            bottomPadding: 12
                            wrapMode: Text.WrapAnywhere
                        }
                        T.ScrollBar.vertical: ArchiveScrollBar {
                            tokens: root.tokens
                            policy: T.ScrollBar.AsNeeded
                        }
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        visible: person.credit.unscoped.length > 0
                        text: qsTr("Credits without track assignments \u00b7 %1").arg(
                                  person.credit.unscoped.join(" \u00b7 "))
                        cssPixelSize: 11
                        cssLineHeight: cssPixelSize * 1.7
                        wrapMode: Text.WrapAnywhere
                        color: root.tokens.muted
                    }
                    Rectangle {
                        antialiasing: true
                        width: parent.width
                        height: 1
                        color: root.tokens.line
                    }
                }
            }
        }
    }
}
