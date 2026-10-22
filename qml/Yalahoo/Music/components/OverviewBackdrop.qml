pragma ComponentBehavior: Bound
import QtQuick

BackdropBlur {
    id: root
    required property real labelWidth
    // Future entry width is reserved throughout animation, so its render targets stay fixed.
    targetWidth: labelWidth + 76 + 72
    targetHeight: 57 + 72
    blurRadius: 36
    horizontalPass: horizontal
    cropName: "overviewCrop"
    intermediateName: "overviewHorizontalTexture"
    ShaderEffect {
        id: horizontal
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        property var source: root.sceneTexture
        property vector2d sampleStep: Qt.vector2d(1 / root.targetWidth, 0)
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }

    ShaderEffect {
        x: -root.blurRadius
        y: -root.blurRadius
        width: root.targetWidth
        height: root.targetHeight
        property var source: root.horizontalTexture
        property vector2d sampleStep: Qt.vector2d(0, 1 / root.targetHeight)
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }
}
