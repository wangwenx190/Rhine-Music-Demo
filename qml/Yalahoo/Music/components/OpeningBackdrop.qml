pragma ComponentBehavior: Bound
import QtQuick

BackdropBlur {
    id: root
    required property size viewport
    required property color pageColor
    required property color surfaceColor
    required property real paperAlpha
    blurRadius: 30
    horizontalPass: horizontal
    BackdropBlurPass {
        id: horizontal
        backdrop: root
        horizontal: true
        property vector4d region: Qt.vector4d(root.sceneX - root.blurRadius, root.sceneY - root.blurRadius,
                                              root.viewport.width, root.viewport.height)
        property vector2d targetSize: Qt.vector2d(width, height)
        property color pageColor: root.pageColor
        property color surfaceColor: root.surfaceColor
        property real paperAlpha: root.paperAlpha
        fragmentShader: "qrc:/assets/shaders/opening-blur.frag.qsb"
    }

    BackdropBlurPass {
        backdrop: root
        property vector4d region: Qt.vector4d(0, 0, 1, 1)
        property vector2d targetSize: Qt.vector2d(width, height)
        property color pageColor: root.pageColor
        property color surfaceColor: root.surfaceColor
        property real paperAlpha: 0
        fragmentShader: "qrc:/assets/shaders/opening-blur.frag.qsb"
    }
}
