// Connects one visual frame source to the shared C++ clock. Hidden windows stop visual updates
// while the independent media controllers preserve playback intent.

pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"
import "../panels"
import "."

Item {
    id: stage
    required property LibrarySession librarySession
    required property MotionController motionController
    property StartupCoordinator startupCoordinator: null
    property OpeningController openingController: null
    readonly property var hostWindow: Window.window
    property bool offscreenRendering: false
    readonly property Opening openingOverlay: openingLoader.item as Opening
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    required property SceneAdapter sceneAdapter
    property PresentationController presentationController: null
    property string fallbackPlayingTitle: ""
    property PlaybackController playbackController: null
    property BackgroundMusicController backgroundMusicController: null
    property InterfaceSoundController interfaceSoundController: null
    property TrackFocusController trackFocusController: null
    property SettingsController settingsController: null
    property PanelController panelController: null
    property LibraryStore libraryStore: null
    property DetailModel detailModel: null
    property OverviewController overviewController: null
    property real hdrHeadroom: 1
    property bool hdrActive: false
    property string preferencesError: ""
    readonly property ExperimentPanels experimentPanel: panels.experimentPanel
    property bool experimentsVisible: false
    signal experimentsVisibilityRequested(bool shown)
    onExperimentsVisibilityRequested: shown => experimentsVisible = shown
    function dispatchHeaderAction(action: string): void {
        switch (action) {
        case "play-pause":
            if (playbackController)
                playbackController.toggle();
            break;
        case "stop":
            if (playbackController)
                playbackController.stop();
            break;
        case "settings":
            if (panelController)
                panelController.open(settingsController ? "settings" : "playback");
            break;
        case "search":
        case "library":
            if (panelController)
                panelController.open(action);
            break;
        case "locate-song":
            if (trackFocusController)
                trackFocusController.locatePlayback();
            break;
        case "open":
            if (presentationController)
                presentationController.open();
            break;
        }
    }
    focus: true
    width: 1280
    height: 720
    Keys.onSpacePressed: event => {
        if (stage.playbackController && event.modifiers === Qt.NoModifier && (
                    !stage.panelController || !stage.panelController.surface.shown) && (
                    !stage.openingController || !stage.openingController.active) && (
                    !stage.inputRouter || !stage.inputRouter.ownsFocus())) {
            stage.playbackController.toggle();
            event.accepted = true;
        } else
            event.accepted = false;
    }
    DesignTokens {
        id: design
        hdrActive: stage.hdrActive
        offscreenRendering: stage.offscreenRendering
        night: stage.sceneAdapter.lighting.night
        reducedMotion: stage.motionController.reducedMotion
        motionSpeed: stage.motionController.clock.speed
    }
    readonly property bool visualActive: stage.motionController.clock.applicationVisible && (
                                             stage.offscreenRendering || (stage.hostWindow !== null
                                                                          && stage.hostWindow.visible
                                                                          && stage.hostWindow.visibility
                                                                          !== Window.Minimized))

    StageFocus {
        openingController: stage.openingController
        presentationController: stage.presentationController
        overviewController: stage.overviewController
        openingOverlay: stage.openingOverlay
        browse: browsePage
        onPreparationRequested: nativeScene.restartPreparation()
    }
    StageFrameDriver {
        sceneAdapter: stage.sceneAdapter
        motionController: stage.motionController
        inputRouter: stage.inputRouter
        startupCoordinator: stage.startupCoordinator
        trackFocusController: stage.trackFocusController
        presentationController: stage.presentationController
        openingController: stage.openingController
        visualActive: stage.visualActive
        offscreenRendering: stage.offscreenRendering
    }
    Connections {
        target: stage.startupCoordinator
        function onPreparationInvalidated() {
            nativeScene.restartPreparation();
        }
    }
    Item {
        id: visualContent
        anchors.fill: parent
        SceneItem {
            id: nativeScene
            hdrHeadroom: stage.hdrHeadroom
            hdrActive: stage.hdrActive
            objectName: "nativeScene"
            diagnosticsEnabled: stage.experimentPanel !== null && stage.experimentPanel.diagnostics
                                && stage.experimentsVisible && (!stage.openingController ||
                                                                !stage.openingController.active)
            anchors.fill: parent
            adapter: stage.sceneAdapter
            renderingActive: stage.visualActive
            enabled: (!stage.openingController || !stage.openingController.active) && (
                         !stage.panelController || !stage.panelController.surface.shown)
            sceneOccluded: stage.openingController !== null && stage.openingController.sceneOccluded
            preparing: stage.openingController !== null && stage.openingController.preparing
            panelLayerSize: stage.panelController && stage.panelController.surface.shown ? Qt.size(
                                                                                               stage.width,
                                                                                               stage.height) :
                                                                                           Qt.size(0,
                                                                                                   0)
            openingLayerSize: stage.openingOverlay ? stage.openingOverlay.canvasSize : Qt.size(0, 0)
            onScenePrepared: {
                if (stage.startupCoordinator)
                    stage.startupCoordinator.scenePrepared();
                else if (stage.openingController)
                    stage.openingController.scenePrepared();
            }
            inputRouter: stage.inputRouter
            inspectionEnabled: stage.presentationController !== null
                               && stage.presentationController.detailEnabled
            titleReel: browsePage.titleController
            transportReel: browsePage.transportController
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
            id: browsePage
            anchors.fill: parent
            metrics: stage.sceneAdapter.layout
            tokens: design
            opening: stage.openingController
            enabled: !stage.openingController || !stage.openingController.active
            playingTitle: stage.playbackController && stage.playbackController.trackId
                          ? stage.playbackController.title : stage.fallbackPlayingTitle
            playing: stage.playbackController ? stage.playbackController.playing : false
            album: stage.librarySession.browseInfo
            albumCount: stage.librarySession.albums.count
            overviewController: stage.overviewController
            rulerController: stage.rulerController
            inputRouter: stage.inputRouter
            clock: stage.motionController.clock
            surface: stage.presentationController ? stage.presentationController.browseSurface :
                                                    null
            contentEnabled: !stage.presentationController
                            || stage.presentationController.archiveEnabled
            selection: stage.sceneAdapter.selection
            onActionRequested: action => stage.dispatchHeaderAction(action)
            onThemeRequested: night => {
                stage.sceneAdapter.recordInput();
                stage.sceneAdapter.lighting.night = night;
            }
        }
        Loader {
            anchors.fill: parent
            active: stage.overviewController !== null
            sourceComponent: Overview {
                objectName: "collectionOverview"
                sceneSource: nativeScene
                controller: stage.overviewController
                columnKind: stage.librarySession.albums.columnKind
                tokens: design
                inputRouter: stage.inputRouter
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
                trackFocus: stage.trackFocusController
                playingTrackId: stage.detailModel && stage.detailModel.album.id
                                === stage.librarySession.playbackAlbumId
                                ? stage.librarySession.playbackTrackId : ""
                onBackRequested: stage.presentationController.back()
                onTrackRequested: trackId => {
                    if (stage.playbackController)
                        stage.playbackController.playTrack(stage.librarySession.navigationAlbumId,
                                                           trackId);
                }
            }
        }
    }
    StagePanels {
        id: panels
        anchors.fill: parent
        tokens: design
        librarySession: stage.librarySession
        panelController: stage.panelController
        trackFocusController: stage.trackFocusController
        playbackController: stage.playbackController
        backgroundMusicController: stage.backgroundMusicController
        interfaceSoundController: stage.interfaceSoundController
        settingsController: stage.settingsController
        libraryStore: stage.libraryStore
        openingController: stage.openingController
        presentationController: stage.presentationController
        sceneSource: nativeScene
        backdropSource: visualContent
        preferencesError: stage.preferencesError
        experimentsVisible: stage.experimentsVisible
        onExperimentsVisibilityRequested: shown => stage.experimentsVisibilityRequested(shown)
        onReturnFocusRequested: browsePage.forceActiveFocus(Qt.OtherFocusReason)
    }
    onWidthChanged: sceneAdapter.layout.setViewport(width, height)
    onHeightChanged: sceneAdapter.layout.setViewport(width, height)
    Component.onCompleted: sceneAdapter.layout.setViewport(width, height)
}
