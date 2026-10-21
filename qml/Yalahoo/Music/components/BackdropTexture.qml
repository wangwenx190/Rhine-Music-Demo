// Captures a hidden backdrop at the owning window's actual output DPR.
pragma ComponentBehavior: Bound
import QtQuick

ShaderEffectSource {
    id: root
    property bool hdrActive: false
    property real outputDpr: Window.window ? Window.window.devicePixelRatio : 1
    format: hdrActive ? ShaderEffectSource.RGBA16F : ShaderEffectSource.RGBA8
    mipmap: true
    textureSize: Qt.size(Math.round(width * outputDpr), Math.round(height * outputDpr))
    visible: false
    smooth: true
}
