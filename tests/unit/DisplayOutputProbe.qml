pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.VectorImage
import "components"

Window {
    id: root
    required property DisplayOutput output
    width: 480
    height: 160
    visible: false
    color: "black"
    title: "yalahOO!!! output verification"
    DisplaySurface {
        anchors.fill: parent
        output: root.output
        backgroundColor: "#162332"
        Rectangle {
            antialiasing: true
            x: 16
            y: 16
            width: 80
            height: 80
            radius: 40
            color: "#eeeeee"
        }
        Text {
            x: 116
            y: 30
            color: "white"
            text: "yalahOO!!! / HDR / SDR"
            font.pixelSize: 21
            font.hintingPreference: Font.PreferNoHinting
            renderType: Text.CurveRendering
            antialiasing: true
        }
        VectorImage {
            x: 116
            y: 78
            width: 230
            height: 50
            source: "qrc:/display/logo-light.svg"
            preferredRendererType: VectorImage.CurveRenderer
            fillMode: VectorImage.PreserveAspectFit
        }
    }
}
