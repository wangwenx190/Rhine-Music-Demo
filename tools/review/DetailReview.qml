pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "components"
import "pages"

Rectangle {
    id: root
    required property LayoutMetrics metrics
    required property PresentationController presentation
    required property DetailModel detailModel
    property bool night: false
    property bool offscreenRendering: false
    color: design.page
    DesignTokens {
        id: design
        night: root.night
        reducedMotion: true
        offscreenRendering: root.offscreenRendering
    }
    Detail {
        objectName: "detailPage"
        anchors.fill: parent
        metrics: root.metrics
        tokens: design
        presentation: root.presentation
        detailModel: root.detailModel
        onBackRequested: root.presentation.back()
    }
}
