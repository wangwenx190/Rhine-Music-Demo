pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    property DisplayOutput output: null
    property color backgroundColor: "black"
    visible: !output || output.ready
    layer.enabled: output ? output.hdrActive : false
    layer.format: ShaderEffectSource.RGBA16F
    layer.smooth: true
    layer.effect: ShaderEffect {
        property var source
        property real whiteScale: root.output ? root.output.whiteScale : 1
        property int hdr10: root.output && root.output.hdr10 ? 1 : 0
        fragmentShader: "qrc:/assets/shaders/display-output.frag.qsb"
    }
    Rectangle {
        antialiasing: true
        anchors.fill: parent
        z: -1
        color: root.backgroundColor
    }
}
