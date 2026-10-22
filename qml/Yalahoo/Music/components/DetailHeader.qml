// Defines the scrolling header and the natural slot used by sticky tabs.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Column {
    id: root
    required property DesignTokens tokens
    required property PresentationController presentation
    required property var album
    required property real viewportWidth
    required property real tabHeight
    required property int activeTab
    required property int trackCount

    readonly property real tabsY: tabSpace.y
    readonly property real tabsHeight: root.tabHeight
    spacing: 0
    DetailAlbumInfo {
        width: parent.width
        tokens: root.tokens
        presentation: root.presentation
        album: root.album
        viewportWidth: root.viewportWidth
    }
    Item {
        id: tabSpace
        width: parent.width
        height: root.tabHeight
    }
    DetailIntroduction {
        width: parent.width
        tokens: root.tokens
        album: root.album
        visible: root.activeTab === 1
        opacity: root.presentation.tabOpacity
    }
    ArchiveText {
        tokens: root.tokens
        visible: root.activeTab === 0 && root.trackCount === 0
        width: parent.width
        topPadding: 25
        text: qsTranslate("Detail", "This album has no playable tracks")
        cssPixelSize: 13
        wrapMode: Text.Wrap
    }
}
