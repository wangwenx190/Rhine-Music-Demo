import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import QtMultimedia

ApplicationWindow {
    id: window
    width: 960
    height: 640
    minimumWidth: 560
    minimumHeight: 440
    visible: true
    title: "yalahOO!!!"
    property int interactionCount: 0

    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/assets/shaders/probe.frag.qsb"
    }
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 18
        Label {
            text: "yalahOO!!!"
            font.pixelSize: 36
            font.bold: true
            color: "#253e37"
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "一张一张，慢慢听"
            font.pixelSize: 22
            color: "#3d544c"
            Layout.alignment: Qt.AlignHCenter
        }
        View3D {
            objectName: "quick3DProbe"
            Layout.preferredWidth: 300
            Layout.preferredHeight: 220
            environment: SceneEnvironment {
                backgroundMode: SceneEnvironment.Transparent
                antialiasingMode: SceneEnvironment.MSAA
                antialiasingQuality: SceneEnvironment.High
            }
            PerspectiveCamera { position: Qt.vector3d(0, 0, 350) }
            DirectionalLight { eulerRotation: Qt.vector3d(-25, -25, 0) }
            Model {
                source: "#Cube"
                scale: Qt.vector3d(1.25, 1.25, 0.28)
                eulerRotation: Qt.vector3d(-12, 28, 0)
                materials: PrincipledMaterial {
                    baseColor: "#668674"
                    roughness: 0.38
                }
            }
        }
        Label {
            text: "Qt 原生运行探针 · QML / 3D / ShaderTools / Multimedia"
            font.pixelSize: 14
            color: "#526259"
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            objectName: "interactionProbe"
            text: window.interactionCount === 0 ? "验证窗口交互" : "交互已响应 " + window.interactionCount
            Layout.alignment: Qt.AlignHCenter
            onClicked: window.interactionCount++
        }
    }
    // Import and instantiate the public QML media types without opening a source.
    MediaPlayer { objectName: "qmlMediaProbe" }
}
