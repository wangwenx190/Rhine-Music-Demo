// Shares the target geometry and sampling axes of separable backdrop shaders.
pragma ComponentBehavior: Bound
import QtQuick

ShaderEffect {
    id: root
    required property BackdropBlur backdrop
    property bool horizontal: false
    x: horizontal ? 0 : -backdrop.blurRadius
    y: horizontal ? 0 : -backdrop.blurRadius
    width: backdrop.targetWidth
    height: backdrop.targetHeight
    visible: !horizontal
    property var source: horizontal ? backdrop.sceneTexture : backdrop.horizontalTexture
    property vector2d sampleStep: horizontal ? Qt.vector2d(1 / Math.max(1, width), 0) : Qt.vector2d(
                                                   0, 1 / Math.max(1, height))
}
