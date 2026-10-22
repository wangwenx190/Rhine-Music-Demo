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
    BackdropBlurPass {
        id: horizontal
        backdrop: root
        horizontal: true
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }

    BackdropBlurPass {
        backdrop: root
        fragmentShader: "qrc:/assets/shaders/overview-blur.frag.qsb"
    }
}
