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
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    required property SceneAdapter sceneAdapter
    property PresentationController presentationController: null
    property DetailModel detailModel: null
    property Item presentationReturnFocus: null
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
        inputRouter: window.inputRouter
        inspectionEnabled: window.presentationController !== null
                           && window.presentationController.detailEnabled
        titleReel: browse.titleController
        transportReel: browse.transportController
    }
    Browse {
        id: browse
        anchors.fill: parent
        metrics: window.sceneAdapter.layout
        tokens: design
        album: window.librarySession.browseInfo
        albumCount: window.librarySession.albums.count
        rulerController: window.rulerController
        inputRouter: window.inputRouter
        clock: window.motionController.clock
        surface: window.presentationController ? window.presentationController.browseSurface : null
        contentEnabled: !window.presentationController
                        || window.presentationController.archiveEnabled
        selection: window.sceneAdapter.selection
        onActionRequested: action => {
            if (action === "open" && window.presentationController)
                window.presentationController.open();
        }
        onThemeRequested: night => {
            window.sceneAdapter.recordInput();
            window.sceneAdapter.lighting.night = night;
        }
    }
    Loader {
        anchors.fill: parent
        active: window.presentationController !== null && window.detailModel !== null
        sourceComponent: Detail {
            metrics: window.sceneAdapter.layout
            tokens: design
            presentation: window.presentationController
            detailModel: window.detailModel
            playingTrackId: window.librarySession.playbackTrackId
            onBackRequested: window.presentationController.back()
        }
    }
    Connections {
        target: window.presentationController
        function onPrepared() {
            if (window.presentationController.phase === "opening" &&
                    !window.presentationReturnFocus)
                window.presentationReturnFocus = window.activeFocusItem;
        }
        function onFocusRequested(detail) {
            if (!detail) {
                Qt.callLater(() => {
                    if (window.presentationController.archiveEnabled) {
                        const target = window.presentationReturnFocus;
                        if (target && target.visible && target.enabled)
                            target.forceActiveFocus(Qt.OtherFocusReason);
                        else
                            browse.forceActiveFocus(Qt.OtherFocusReason);
                        window.presentationReturnFocus = null;
                    }
                });
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: window.presentationController !== null &&
                 !window.presentationController.archiveEnabled
        onActivated: window.presentationController.back()
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
