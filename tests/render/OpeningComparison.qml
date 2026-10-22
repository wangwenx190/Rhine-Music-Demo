pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import Yalahoo.Music

Item {
    id: root
    required property OpeningInk source
    property var paths: source.comparisonPaths()
    width: source.width
    height: source.height
    Item {
        readonly property real fit: Math.min(root.width / 816, root.height / 208)
        x: (root.width - 816 * fit) / 2
        y: (root.height - 208 * fit) / 2
        width: 816
        height: 208
        scale: fit
        transformOrigin: Item.TopLeft
        Repeater {
            model: root.paths
            delegate: Shape {
                id: shape
                required property var modelData
                width: 816
                height: 208
                preferredRendererType: Shape.CurveRenderer
                opacity: modelData.alpha
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: shape.modelData.color
                    strokeWidth: shape.modelData.width
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg {
                        path: shape.modelData.d
                    }
                }
            }
        }
    }
}
