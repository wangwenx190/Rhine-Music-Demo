import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "scene"

ApplicationWindow {
    id: window
    required property LibrarySession librarySession
    required property MotionController motionController
    required property SceneAdapter sceneAdapter
    property bool sceneReview: false
    width: 960
    height: 640
    minimumWidth: 1
    minimumHeight: 1
    visible: true
    title: "yalahOO!!!"
    property int interactionCount: 0

    FrameAnimation {
        id: visualFrames
        running: !window.sceneReview && window.visible && window.visibility !== Window.Minimized
            && window.motionController.clock.applicationVisible
        onRunningChanged: window.motionController.clock.active = running
        Component.onCompleted: window.motionController.clock.active = running
        onTriggered: window.motionController.clock.advance(frameTime)
    }

    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/assets/shaders/probe.frag.qsb"
    }
    GeometryScene {
        anchors.fill: parent
        adapter: window.sceneAdapter
    }
    onWidthChanged: sceneAdapter.layout.setViewport(width, height)
    onHeightChanged: sceneAdapter.layout.setViewport(width, height)
    Component.onCompleted: sceneAdapter.layout.setViewport(width, height)
    ColumnLayout {
        visible: !window.sceneReview
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
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
        Label {
            text: "Qt 原生运行探针 · QML / 3D / ShaderTools / Multimedia"
            font.pixelSize: 14
            color: "#526259"
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            objectName: "libraryDataProbe"
            text: "匿名库模型 · " + window.librarySession.albums.count + " 张专辑"
            color: "#526259"
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            objectName: "interactionProbe"
            text: window.interactionCount === 0 ? "验证窗口交互" : "交互已响应 " + window.interactionCount
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                window.interactionCount++
                window.sceneAdapter.detail = window.sceneAdapter.detail === 0 ? 1 : 0
            }
            icon.source: "qrc:/assets/icons/play-day.svg"
        }
    }
    Label {
        objectName: "layoutProbe"
        visible: window.sceneReview
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        text: window.sceneAdapter.layout.kind + " · " + window.sceneAdapter.frameRevision
        color: "#253e37"
    }
    // Import and instantiate the public QML media types without opening a source.
    MediaPlayer { objectName: "qmlMediaProbe" }
}
