import QtQuick
import "scene"

ProbeWindow {
    id: window
    GeometryScene {
        z: -1
        anchors.fill: parent
        adapter: window.sceneAdapter
        geometryOnly: window.sceneReview || window.geometryProbe
    }
}
