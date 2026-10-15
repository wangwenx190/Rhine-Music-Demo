pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    required property Item sceneSource
    required property real sceneX
    required property real sceneY
    required property real labelWidth
    // Future entry width is reserved throughout animation, so its render targets stay fixed.
    readonly property real targetWidth: labelWidth + 76 + 72
    readonly property real targetHeight: 57 + 72
    clip: true
    ShaderEffectSource {
        id: crop
        sourceItem: root.sceneSource
        sourceRect: Qt.rect(root.sceneX - 36, root.sceneY - 36, root.targetWidth, root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
        textureSize: Qt.size(root.targetWidth, root.targetHeight)
        visible: false
        hideSource: false
        smooth: true
        recursive: false
    }
    ShaderEffect {
        id: horizontal
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        property var source: crop
        property vector2d sampleStep: Qt.vector2d(1 / root.targetWidth, 0)
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }
    ShaderEffectSource {
        id: horizontalTexture
        sourceItem: horizontal
        textureSize: Qt.size(root.targetWidth, root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        hideSource: true
        smooth: true
    }
    ShaderEffect {
        x: -36
        y: -36
        width: root.targetWidth
        height: root.targetHeight
        property var source: horizontalTexture
        property vector2d sampleStep: Qt.vector2d(0, 1 / root.targetHeight)
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }
}
