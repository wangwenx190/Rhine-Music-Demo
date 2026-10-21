pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    readonly property real outputDpr: Window.window ? Window.window.devicePixelRatio : 1
    property bool hdrActive: false
    required property Item sceneSource
    required property Item contentSource
    required property real sceneX
    required property real sceneY
    required property real contentY
    readonly property real targetWidth: width + 108
    readonly property real targetHeight: height + 108
    clip: true
    BackdropTexture {
        id: scene
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.sceneSource
        sourceRect: Qt.rect(root.sceneX - 54, root.sceneY - 54, root.targetWidth, root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
    }
    BackdropTexture {
        id: content
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.contentSource
        sourceRect: Qt.rect(-54, root.contentY - 54, root.targetWidth, root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
    }
    ShaderEffect {
        id: horizontal
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        property var source: scene
        property var overlay: content
        property vector2d sampleStep: Qt.vector2d(1 / width, 0)
        property real composite: 1
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }
    BackdropTexture {
        id: horizontalTexture
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: horizontal
        width: root.targetWidth
        height: root.targetHeight
        hideSource: true
    }
    ShaderEffect {
        x: -54
        y: -54
        width: root.targetWidth
        height: root.targetHeight
        property var source: horizontalTexture
        property var overlay: content
        property vector2d sampleStep: Qt.vector2d(0, 1 / height)
        property real composite: 0
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }
}
