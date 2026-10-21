pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    readonly property real outputDpr: Window.window ? Window.window.devicePixelRatio : 1
    property bool hdrActive: false
    required property Item sceneSource
    required property real sceneX
    required property real sceneY
    required property size viewport
    required property color pageColor
    required property color surfaceColor
    required property real paperAlpha
    readonly property real targetWidth: width + 60
    readonly property real targetHeight: height + 60
    clip: true
    BackdropTexture {
        id: crop
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.sceneSource
        sourceRect: Qt.rect(root.sceneX - 30, root.sceneY - 30, root.targetWidth, root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
    }
    ShaderEffect {
        id: horizontal
        width: root.targetWidth
        height: root.targetHeight
        visible: false
        property var source: crop
        property vector2d sampleStep: Qt.vector2d(1 / width, 0)
        property vector4d region: Qt.vector4d(root.sceneX - 30, root.sceneY - 30,
                                              root.viewport.width, root.viewport.height)
        property vector2d targetSize: Qt.vector2d(width, height)
        property color pageColor: root.pageColor
        property color surfaceColor: root.surfaceColor
        property real paperAlpha: root.paperAlpha
        fragmentShader: "qrc:/assets/shaders/opening-blur.frag.qsb"
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
        x: -30
        y: -30
        width: root.targetWidth
        height: root.targetHeight
        property var source: horizontalTexture
        property vector2d sampleStep: Qt.vector2d(0, 1 / height)
        property vector4d region: Qt.vector4d(0, 0, 1, 1)
        property vector2d targetSize: Qt.vector2d(width, height)
        property color pageColor: root.pageColor
        property color surfaceColor: root.surfaceColor
        property real paperAlpha: 0
        fragmentShader: "qrc:/assets/shaders/opening-blur.frag.qsb"
    }
}
