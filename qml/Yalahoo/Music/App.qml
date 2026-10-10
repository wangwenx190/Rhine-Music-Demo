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
            font.family: MusicFonts.bold.family
            font.styleName: MusicFonts.bold.styleName
            font.weight: MusicFonts.bold.weight
            font.pixelSize: 36
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
            Node {
                scale: Qt.vector3d(50, 50, 50)
                eulerRotation: Qt.vector3d(-12, 28, 0)
                Node {
                    y: -1.85
                    Model {
                        objectName: "caseLidProbe"
                        source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"
                        materials: PrincipledMaterial {
                            id: probeMaterial
                            baseColor: "#668674"
                            roughness: 0.38
                        }
                    }
                    Model {
                        objectName: "caseSpineProbe"
                        source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"
                        materials: probeMaterial
                    }
                    Model {
                        objectName: "caseTrayProbe"
                        source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"
                        materials: probeMaterial
                    }
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
            icon.source: "qrc:/assets/icons/play-day.svg"
        }
    }
    // Import and instantiate the public QML media types without opening a source.
    MediaPlayer { objectName: "qmlMediaProbe" }
}
