pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music

Item {
    id: root
    clip: true
    property real contentOpacity: 1
    required property DesignTokens tokens
    required property DetailModel detailModel
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
    function updateScrollbar() {
        scrollable = currentView && currentView.contentHeight > currentView.height;
    }
    readonly property real tabsY: layoutHeader ? layoutHeader.y - currentView.contentY
                                                 + layoutHeader.tabsY : 0
    readonly property var producerGroups: detailModel.album.producerGroups || []
    readonly property int creditColumns: Math.max(1, Math.min(producerGroups.length, Math.floor((contentWidth
                                                                                                 + 26) / 236)))
    function saveScroll() {
        savedScroll = currentView ? currentView.contentY - currentView.originY : 0;
    }
    function restoreScroll() {
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
    function forceLayout() {
        if (currentView)
            currentView.forceLayout();
    }
    function positionViewAtBeginning() {
        preserveBeginning = true;
        restoreBeginning();
    }
    function restoreBeginning() {
        if (!preserveBeginning)
            return;
        if (currentView)
            currentView.positionViewAtBeginning();
    }
    function positionViewAtIndex(index, mode) {
        preserveBeginning = false;
        if (currentView)
            currentView.positionViewAtIndex(index, mode);
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
            onMovementStarted: root.preserveBeginning = false
            onCurrentIndexChanged: if (currentIndex > 0)
                                       root.preserveBeginning = false
            delegate: trackDelegate
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
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
            onMovementStarted: root.preserveBeginning = false
            onCurrentIndexChanged: if (currentIndex > 0)
                                       root.preserveBeginning = false
            delegate: creditDelegate
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                onPressedChanged: if (pressed)
                                      root.preserveBeginning = false
            }
        }
    }
    Component {
        id: trackDelegate
        Item {
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
            required property bool discHeading
            width: root.contentWidth
            height: names.implicitHeight + (root.viewportWidth >= 1700 ? 28 : 22) + 1 + (
                        discHeading ? discLabel.implicitHeight + 28 : 0)
            ArchiveText {
                id: discLabel
                tokens: root.tokens
                visible: row.discHeading
                text: "DISC " + row.disc.toString().padStart(2, "0")
                cssPixelSize: 8
                color: root.tokens.muted
                y: 20
            }
            Rectangle {
                y: row.discHeading ? discLabel.implicitHeight + 28 : 0
                width: parent.width
                height: parent.height - y - 1
                color: hit.hovered ? root.tokens.veil : "transparent"
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
                    width: Math.max(0, parent.width - x - root.formatWidth - root.durationWidth
                                    - root.columnGap * 2)
                    spacing: 5
                    ArchiveText {
                        id: trackTitleText
                        tokens: root.tokens
                        width: parent.width
                        objectName: "detailTrackTitle"
                        text: row.trackTitle
                        cssPixelSize: root.viewportWidth >= 1700 ? 14 : root.viewportWidth <= 1100
                                                                   && root.viewportWidth > 700 ? 10 :
                                                                                                 11
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
                        text: person.credit.roles.join(" · ")
                        cssPixelSize: 12
                        cssLineHeight: cssPixelSize * 1.7
                        wrapMode: Text.WrapAnywhere
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        text: "来源：" + person.credit.sources.join(" · ")
                        cssPixelSize: 10
                        cssLineHeight: cssPixelSize * 1.5
                        wrapMode: Text.WrapAnywhere
                        color: root.tokens.muted
                    }
                    ArchiveButton {
                        objectName: "detailCreditExpand"
                        tokens: root.tokens
                        visible: person.credit.tracks.length > 1
                        text: (person.expanded ? "收起参与曲目 −" : "展开参与曲目 ＋")
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
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
                        visible: person.credit.unscoped.length > 0
                        text: "未细分曲目的署名 · " + person.credit.unscoped.join(" · ")
                        cssPixelSize: 11
                        cssLineHeight: cssPixelSize * 1.7
                        wrapMode: Text.WrapAnywhere
                        color: root.tokens.muted
                    }
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: root.tokens.line
                    }
                }
            }
        }
    }
}
