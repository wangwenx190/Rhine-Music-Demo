pragma ComponentBehavior: Bound
import QtQuick

BackdropBlur {
    id: root
    required property Item contentSource
    required property real contentY
    blurRadius: 54
    horizontalPass: horizontal
    BackdropTexture {
        id: content
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.contentSource
        sourceRect: Qt.rect(-root.blurRadius, root.contentY - root.blurRadius, root.targetWidth,
                            root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
    }
    ShaderEffect {
        id: horizontal
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        property var source: root.sceneTexture
        property var overlay: content
        property vector2d sampleStep: Qt.vector2d(1 / width, 0)
        property real composite: 1
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }

    ShaderEffect {
        x: -root.blurRadius
        y: -root.blurRadius
        width: root.targetWidth
        height: root.targetHeight
        property var source: root.horizontalTexture
        property var overlay: content
        property vector2d sampleStep: Qt.vector2d(0, 1 / height)
        property real composite: 0
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }
}
