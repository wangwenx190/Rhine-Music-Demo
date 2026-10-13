pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property var album: ({})
    property int albumCount: 0
    property string playingTitle: ""
    property bool showNavigation: true
    signal actionRequested(string action)
    signal themeRequested(bool night)
    ReadingVeil {
        anchors.fill: parent
        metrics: root.metrics
        tokens: root.tokens
        calloutTop: info.y
        calloutHeight: info.height
    }
    Header {
        metrics: root.metrics
        tokens: root.tokens
        playingTitle: root.playingTitle
        onActionRequested: action => root.actionRequested(action)
        onThemeRequested: night => root.themeRequested(night)
    }
    BrowseInfo {
        id: info
        metrics: root.metrics
        tokens: root.tokens
        album: root.album
        ordinal: (root.album.code || 1).toString().padStart(3, "0")
        calloutBottom: navigation.bottomGap + navigation.height + 28
        onOpenRequested: root.actionRequested("open")
    }
    Footer {
        metrics: root.metrics
        tokens: root.tokens
        albumCount: root.albumCount
    }
    NavigationFrame {
        id: navigation
        metrics: root.metrics
        tokens: root.tokens
        albumCount: root.album.columnCount || root.albumCount
        ordinal: (root.album.number || 1).toString().padStart(2, "0")
        visible: root.showNavigation
    }
}
