pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music

Item {
    id: root
    required property DesignTokens tokens
    required property DetailModel detailModel
    property bool about: false
    property real viewportWidth: width
    readonly property real columnGap: viewportWidth <= 1100 && viewportWidth > 700 ? 6 : 9
    readonly property real numberWidth: viewportWidth <= 700 ? 21 : viewportWidth <= 1100 ? 20 : 23
    readonly property real formatWidth: viewportWidth <= 700 ? 31 : viewportWidth <= 1100 ? 30 : 40
    readonly property real durationWidth: viewportWidth <= 1100 ? 34 : 38
    property string playingTrackId: ""
    signal trackRequested(string trackId)
    property Component header: null
    readonly property ListView currentView: view.item as ListView
    function forceLayout() {
        if (currentView)
            currentView.forceLayout();
    }
    function positionViewAtBeginning() {
        if (currentView)
            currentView.positionViewAtBeginning();
    }
    function positionViewAtIndex(index, mode) {
        if (currentView)
            currentView.positionViewAtIndex(index, mode);
    }
    Loader {
        id: view
        anchors.fill: parent
        sourceComponent: root.about ? aboutView : tracksView
    }
    Component {
        id: tracksView
        ListView {
            clip: true
            reuseItems: true
            cacheBuffer: 100
            boundsBehavior: Flickable.StopAtBounds
            model: root.detailModel
            header: root.header
            delegate: trackDelegate
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
    Component {
        id: aboutView
        ListView {
            clip: true
            reuseItems: true
            cacheBuffer: 100
            boundsBehavior: Flickable.StopAtBounds
            model: root.detailModel.album.producers || []
            header: root.header
            delegate: creditDelegate
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
    Component {
        id: trackDelegate
        Item {
            id: row
            objectName: "trackDelegate"
            required property int index
            required property string trackId
            required property string number
            required property string trackTitle
            required property string artist
            required property string format
            required property string duration
            required property int disc
            required property bool discHeading
            width: ListView.view.width
            height: names.implicitHeight + (root.viewportWidth >= 1700 ? 28 : 22) + (discHeading
                                                                                     ? 36 : 0)
            ArchiveText {
                tokens: root.tokens
                visible: row.discHeading
                text: "DISC " + row.disc.toString().padStart(2, "0")
                cssPixelSize: 8
                color: root.tokens.muted
                y: 20
            }
            Rectangle {
                y: row.discHeading ? 36 : 0
                width: parent.width
                height: parent.height - y
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
                        tokens: root.tokens
                        width: parent.width
                        text: row.trackTitle
                        cssPixelSize: root.viewportWidth >= 1700 ? 14 : root.viewportWidth <= 1100
                                                                   && root.viewportWidth > 700 ? 10 :
                                                                                                 11
                        elide: Text.ElideRight
                        color: row.trackId === root.playingTrackId ? root.tokens.accent :
                                                                     root.tokens.ink
                    }
                    ArchiveText {
                        tokens: root.tokens
                        width: parent.width
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
                    cssPixelSize: 9
                    horizontalAlignment: Text.AlignRight
                    color: root.tokens.muted
                }
                Rectangle {
                    anchors.bottom: parent.bottom
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
        Column {
            required property var modelData
            width: ListView.view.width
            spacing: 6
            bottomPadding: 16
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: parent.modelData.name + " · " + parent.modelData.role
                cssPixelSize: 12
                wrapMode: Text.Wrap
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: [parent.modelData.trackTitle, parent.modelData.source,
                    parent.modelData.url].filter(Boolean).join(" · ")
                cssPixelSize: 10
                wrapMode: Text.Wrap
                color: root.tokens.muted
            }
        }
    }
}
