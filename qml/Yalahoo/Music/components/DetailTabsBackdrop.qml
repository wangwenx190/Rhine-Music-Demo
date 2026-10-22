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
    BackdropBlurPass {
        id: horizontal
        backdrop: root
        horizontal: true
        property var overlay: content
        property real composite: 1
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }

    BackdropBlurPass {
        backdrop: root
        property var overlay: content
        property real composite: 0
        fragmentShader: "qrc:/assets/shaders/detail-tabs-blur.frag.qsb"
    }
}
