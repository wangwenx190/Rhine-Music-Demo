import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import QtMultimedia
import "components"
import "scene"

T.ApplicationWindow {
    id: window
    DesignTokens {
        id: design
        night: window.sceneAdapter.lighting.night
        reducedMotion: window.motionController.reducedMotion
        motionSpeed: window.motionController.clock.speed
    }
    required property LibrarySession librarySession
    required property MotionController motionController
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    required property SceneAdapter sceneAdapter
    property bool sceneReview: false
    property bool geometryProbe: false
    property bool verificationRun: false
    property string preferencesError: ""
    property int verificationFlags: Qt.Window
    flags: verificationRun ? verificationFlags : Qt.Window
    width: 960
    height: 640
    minimumWidth: 1
    minimumHeight: 1
    visible: !verificationRun
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
        geometryOnly: window.sceneReview || window.geometryProbe
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
        ArchiveText {
            tokens: design
            text: "yalahOO!!!"
            font.family: MusicFonts.bold.family
            font.styleName: MusicFonts.bold.styleName
            font.weight: MusicFonts.bold.weight
            cssPixelSize: 36
            color: "#253e37"
            Layout.alignment: Qt.AlignHCenter
        }
        ArchiveText {
            tokens: design
            text: qsTr("One album at a time")
            cssPixelSize: 22
            color: "#3d544c"
            Layout.alignment: Qt.AlignHCenter
        }
        ArchiveText {
            tokens: design
            text: qsTr("Qt native runtime probe \u00b7 QML / 3D / ShaderTools / Multimedia")
            cssPixelSize: 14
            color: "#526259"
            Layout.alignment: Qt.AlignHCenter
        }
        ArchiveText {
            tokens: design
            objectName: "libraryDataProbe"
            text: qsTr("Anonymous library model \u00b7 %1 albums").arg(
                      window.librarySession.albums.count)
            color: "#526259"
            Layout.alignment: Qt.AlignHCenter
        }
        ArchiveButton {
            tokens: design
            framed: true
            objectName: "themeProbe"
            text: window.sceneAdapter.lighting.night ? qsTr("Warm day") : qsTr("Deep night")
            onClicked: window.sceneAdapter.lighting.night = !window.sceneAdapter.lighting.night
        }
        ArchiveButton {
            tokens: design
            framed: true
            objectName: "interactionProbe"
            text: window.interactionCount === 0 ? qsTr("Verify window interaction") : qsTr(
                                                      "Interaction received %1").arg(
                                                      window.interactionCount)
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                window.interactionCount++;
                window.sceneAdapter.detail = window.sceneAdapter.detail === 0 ? 1 : 0;
            }
            iconName: "play"
        }
    }
    ArchiveText {
        tokens: design
        objectName: "layoutProbe"
        visible: window.sceneReview
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        text: window.sceneAdapter.layout.kind + " \u00b7 " + window.sceneAdapter.frameRevision
        color: "#253e37"
    }
    // Import and instantiate the public QML media types without opening a source.
    MediaPlayer {
        objectName: "qmlMediaProbe"
    }
}
