pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"
import "."

Item {
    id: stage
    required property LibrarySession librarySession
    required property MotionController motionController
    property OpeningController openingController: null
    property Item openingReturnFocus: null
    property bool offscreenRendering: false
    readonly property Opening openingOverlay: openingLoader.item as Opening
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    required property SceneAdapter sceneAdapter
    property PresentationController presentationController: null
    property string reviewPlayingTitle: ""
    property DetailModel detailModel: null
    property OverviewController overviewController: null
    property Item presentationReturnFocus: null
    property bool geometryProbe: false
    property string preferencesError: ""
    focus: true
    width: 1280
    height: 720
    DesignTokens {
        id: design
        offscreenRendering: stage.offscreenRendering
        night: stage.sceneAdapter.lighting.night
        reducedMotion: stage.motionController.reducedMotion
        motionSpeed: stage.motionController.clock.speed
    }
    FrameAnimation {
        running: !stage.offscreenRendering && stage.Window.window !== null
                 && stage.Window.window.visible && stage.Window.window.visibility
                 !== Window.Minimized && stage.motionController.clock.applicationVisible
        onRunningChanged: stage.motionController.clock.active = running
        Component.onCompleted: stage.motionController.clock.active = running
        onTriggered: stage.motionController.clock.advance(frameTime)
    }
    SceneItem {
        id: nativeScene
        objectName: "nativeScene"
        anchors.fill: parent
        adapter: stage.sceneAdapter
        enabled: !stage.openingController || !stage.openingController.active
        sceneOccluded: stage.openingController !== null && stage.openingController.sceneOccluded
        preparing: stage.openingController !== null && stage.openingController.preparing
        openingLayerSize: stage.openingOverlay ? stage.openingOverlay.canvasSize : Qt.size(0, 0)
        onScenePrepared: {
            if (stage.openingController)
                stage.openingController.scenePrepared();
        }
        inputRouter: stage.inputRouter
        inspectionEnabled: stage.presentationController !== null
                           && stage.presentationController.detailEnabled
        titleReel: browse.titleController
        transportReel: browse.transportController
    }
    Loader {
        id: openingLoader
        anchors.fill: parent
        active: stage.openingController !== null
        sourceComponent: Opening {
            sceneSource: nativeScene
            controller: stage.openingController
            tokens: design
            errorText: nativeScene.errorText
        }
    }
    Browse {
        id: browse
        anchors.fill: parent
        metrics: stage.sceneAdapter.layout
        tokens: design
        opening: stage.openingController
        enabled: !stage.openingController || !stage.openingController.active
        playingTitle: stage.reviewPlayingTitle
        album: stage.librarySession.browseInfo
        albumCount: stage.librarySession.albums.count
        overviewController: stage.overviewController
        rulerController: stage.rulerController
        inputRouter: stage.inputRouter
        clock: stage.motionController.clock
        surface: stage.presentationController ? stage.presentationController.browseSurface : null
        contentEnabled: !stage.presentationController || stage.presentationController.archiveEnabled
        selection: stage.sceneAdapter.selection
        onActionRequested: action => {
            if (action === "open" && stage.presentationController)
                stage.presentationController.open();
        }
        onThemeRequested: night => {
            stage.sceneAdapter.recordInput();
            stage.sceneAdapter.lighting.night = night;
        }
    }
    Loader {
        anchors.fill: parent
        active: stage.overviewController !== null
        sourceComponent: Overview {
            sceneSource: nativeScene
            controller: stage.overviewController
            tokens: design
            inputRouter: stage.inputRouter
        }
    }
    Connections {
        target: stage.openingController
        function onStarted() {
            nativeScene.restartPreparation();
            if (!stage.openingReturnFocus)
                stage.openingReturnFocus = (stage.Window.window
                                            ? stage.Window.window.activeFocusItem : null);
            if (stage.openingOverlay)
                stage.openingOverlay.focusSkip();
        }
        function onCompleted() {
            const target = stage.openingReturnFocus;
            if (target && target.visible && target.enabled)
                target.forceActiveFocus(Qt.OtherFocusReason);
            else
                browse.forceActiveFocus(Qt.OtherFocusReason);
            stage.openingReturnFocus = null;
        }
    }
    Connections {
        target: stage.overviewController
        function onFocusRequested(overview: bool) {
            if (!overview)
                browse.focusOverviewToggle();
        }
    }
    Loader {
        anchors.fill: parent
        active: stage.presentationController !== null && stage.detailModel !== null
        sourceComponent: Detail {
            sceneSource: nativeScene
            metrics: stage.sceneAdapter.layout
            tokens: design
            presentation: stage.presentationController
            detailModel: stage.detailModel
            playingTrackId: stage.librarySession.playbackTrackId
            onBackRequested: stage.presentationController.back()
        }
    }
    Connections {
        target: stage.presentationController
        function onPrepared() {
            if (stage.presentationController.phase === "opening" && !stage.presentationReturnFocus)
                stage.presentationReturnFocus = (stage.Window.window
                                                 ? stage.Window.window.activeFocusItem : null);
        }
        function onFocusRequested(detail) {
            if (!detail) {
                Qt.callLater(() => {
                    if (stage.presentationController.archiveEnabled) {
                        const target = stage.presentationReturnFocus;
                        if (target && target.visible && target.enabled)
                            target.forceActiveFocus(Qt.OtherFocusReason);
                        else
                            browse.forceActiveFocus(Qt.OtherFocusReason);
                        stage.presentationReturnFocus = null;
                    }
                });
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: (!stage.openingController || !stage.openingController.active)
                 && stage.presentationController !== null &&
                 !stage.presentationController.archiveEnabled
        onActivated: stage.presentationController.back()
    }
    ArchiveText {
        tokens: design
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        visible: stage.preferencesError.length > 0 || nativeScene.errorText.length > 0
                 || stage.sceneAdapter.quality.limited
        text: stage.preferencesError.length > 0 ? stage.preferencesError :
                                                  nativeScene.errorText.length > 0 ? "图形配置不可用："
                                                                                     + nativeScene.errorText :
                                                                                     "输出达到尺寸保护上限，画质设置保持原值。"
        color: design.accent
    }
    onWidthChanged: sceneAdapter.layout.setViewport(width, height)
    onHeightChanged: sceneAdapter.layout.setViewport(width, height)
    Component.onCompleted: sceneAdapter.layout.setViewport(width, height)
}
