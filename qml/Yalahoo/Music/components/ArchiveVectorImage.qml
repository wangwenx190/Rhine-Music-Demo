pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.VectorImage

Item {
    id: root
    property url source
    SceneGraphVisual {
        anchors.fill: parent
        sourceComponent: VectorImage {
            source: root.source
            preferredRendererType: VectorImage.CurveRenderer
            fillMode: VectorImage.PreserveAspectFit
            smooth: true
            antialiasing: true
        }
    }
}
