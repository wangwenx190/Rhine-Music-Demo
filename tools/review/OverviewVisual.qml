pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "components"
import "pages"

Item {
    id: root
    required property OverviewController overview
    required property InputRouter inputRouter
    property bool night: false
    property url backgroundUrl
    DesignTokens {
        id: design
        night: root.night
        reducedMotion: true
        offscreenRendering: true
    }
    Image {
        id: backdrop
        anchors.fill: parent
        source: root.backgroundUrl
        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
    }
    Overview {
        anchors.fill: parent
        controller: root.overview
        inputRouter: root.inputRouter
        sceneSource: backdrop
        tokens: design
    }
}
