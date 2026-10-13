pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "components"
import "pages"

ApplicationWindow {
    id: window
    required property LibrarySession librarySession
    required property MotionController motionController
    required property SceneAdapter sceneAdapter
    property bool verificationRun: false
    property int verificationFlags: Qt.Window
    property bool geometryProbe: false
    property string preferencesError: ""
    width: 1280
    height: 720
    minimumWidth: 320
    minimumHeight: 240
    visible: !verificationRun
    flags: verificationRun ? verificationFlags : Qt.Window
    title: design.displayName
    color: design.page
    DesignTokens {
        id: design
        night: window.sceneAdapter.lighting.night
        reducedMotion: window.motionController.reducedMotion
        motionSpeed: window.motionController.clock.speed
    }
    FrameAnimation {
        running: window.visible && window.visibility !== Window.Minimized
                 && window.motionController.clock.applicationVisible
        onRunningChanged: window.motionController.clock.active = running
        Component.onCompleted: window.motionController.clock.active = running
        onTriggered: window.motionController.clock.advance(frameTime)
    }
    SceneItem {
        id: nativeScene
        objectName: "nativeScene"
        anchors.fill: parent
        adapter: window.sceneAdapter
    }
    Browse {
        anchors.fill: parent
        metrics: window.sceneAdapter.layout
        tokens: design
        album: window.librarySession.browseInfo
        albumCount: window.librarySession.albums.count
        onThemeRequested: night => {
            window.sceneAdapter.recordInput();
            window.sceneAdapter.lighting.night = night;
        }
    }
    ArchiveText {
        tokens: design
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        visible: window.preferencesError.length > 0 || nativeScene.errorText.length > 0
                 || window.sceneAdapter.quality.limited
        text: window.preferencesError.length > 0 ? window.preferencesError :
                                                   nativeScene.errorText.length > 0 ? "图形配置不可用："
                                                                                      + nativeScene.errorText :
                                                                                      "输出达到尺寸保护上限，画质设置保持原值。"
        color: design.accent
    }
    onWidthChanged: sceneAdapter.layout.setViewport(width, height)
    onHeightChanged: sceneAdapter.layout.setViewport(width, height)
    Component.onCompleted: sceneAdapter.layout.setViewport(width, height)
}
