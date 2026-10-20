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
    property Item openingReturnFocus: null
    readonly property var hostWindow: Window.window
    property bool offscreenRendering: false
    readonly property Opening openingOverlay: openingLoader.item as Opening
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    required property SceneAdapter sceneAdapter
    property PresentationController presentationController: null
    property string reviewPlayingTitle: ""
    property PlaybackController playbackController: null
    property BackgroundMusicController backgroundMusicController: null
    property InterfaceSoundController interfaceSoundController: null
    property TrackFocusController trackFocusController: null
    property SettingsController settingsController: null
    property PanelController panelController: null
    property LibraryStore libraryStore: null
    property DetailModel detailModel: null
    property OverviewController overviewController: null
    property Item presentationReturnFocus: null
    property bool geometryProbe: false
    property real hdrHeadroom: 1
    property bool hdrActive: false
    property string preferencesError: ""
    property string navigationError: ""
    readonly property ExperimentPanels experimentPanel: experiments.item as ExperimentPanels
    property bool experimentsVisible: false
    signal experimentsVisibilityRequested(bool shown)
    onExperimentsVisibilityRequested: shown => experimentsVisible = shown
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
    function restorePresentationFocus(): void {
        if (!presentationController || !presentationController.archiveEnabled)
            return;
        const target = presentationReturnFocus;
        if (target && target.visible && target.enabled)
            target.forceActiveFocus(Qt.OtherFocusReason);
        else
            browse.forceActiveFocus(Qt.OtherFocusReason);
        presentationReturnFocus = null;
    }
    Connections {
        target: stage.sceneAdapter
        function onFrameChanged() {
            stage.motionController.clock.requestVisualFrame();
        }
    }
    Connections {
        target: stage.motionController
        function onStateChanged() {
            stage.motionController.clock.requestVisualFrame();
        }
        function onReducedMotionChanged() {
            stage.motionController.clock.requestVisualFrame();
        }
    }
    Connections {
        target: stage.inputRouter
        function onInputReceived() {
            stage.motionController.clock.requestVisualFrame();
        }
    }
    Binding {
        target: stage.startupCoordinator
        property: "visualActive"
        value: stage.visualActive
        when: stage.startupCoordinator !== null
    }
    Connections {
        target: stage.startupCoordinator
        function onPreparationInvalidated() {
            nativeScene.restartPreparation();
        }
    }
    FrameAnimation {
        running: !stage.offscreenRendering && stage.visualActive && (
                     !stage.motionController.reducedMotion
                     || stage.motionController.clock.workPending || (stage.trackFocusController
                                                                     && stage.trackFocusController.active)
                     || (stage.presentationController && stage.presentationController.phase
                         !== "archive" && stage.presentationController.phase !== "detail") || (
                         stage.openingController && stage.openingController.active &&
                         !stage.openingController.preparing))
        onRunningChanged: stage.motionController.clock.active = running
        Component.onCompleted: stage.motionController.clock.active = running
        onTriggered: stage.motionController.clock.advance(frameTime)
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
            playingTitle: stage.playbackController && stage.playbackController.trackId
                          ? stage.playbackController.title : stage.reviewPlayingTitle
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
            onActionRequested: action => {
                if (stage.playbackController && action === "play-pause") {
                    stage.playbackController.toggle();
                } else if (stage.playbackController && action === "stop") {
                    stage.playbackController.stop();
                } else if (action === "settings" && stage.panelController) {
                    stage.panelController.open(stage.settingsController ? "settings" : "playback");
                } else if ((action === "search" || action === "library") && stage.panelController) {
                    stage.panelController.open(action);
                } else if (action === "locate-song" && stage.trackFocusController) {
                    stage.trackFocusController.locatePlayback();
                } else if (action === "open" && stage.presentationController)
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
                    stage.openingReturnFocus = (stage.hostWindow ? stage.hostWindow.activeFocusItem :
                                                                   null);
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
        Connections {
            target: stage.presentationController
            function onPrepared() {
                if (stage.presentationController.phase === "opening" &&
                        !stage.presentationReturnFocus)
                    stage.presentationReturnFocus = (stage.hostWindow
                                                     ? stage.hostWindow.activeFocusItem : null);
            }
            function onFocusRequested(detail) {
                if (!detail)
                    Qt.callLater(stage.restorePresentationFocus);
            }
        }
    }
    Loader {
        anchors.fill: parent
        active: stage.panelController !== null && stage.trackFocusController !== null
        sourceComponent: LibraryPanels {
            tokens: design
            librarySession: stage.librarySession
            controller: stage.panelController
            trackFocus: stage.trackFocusController
            playback: stage.playbackController
            backgroundMusic: stage.backgroundMusicController
            interfaceSound: stage.interfaceSoundController
            settings: stage.settingsController
            onExperimentsRequested: {
                stage.panelController.close();
                stage.experimentsVisibilityRequested(true);
            }
            onReplayRequested: {
                stage.panelController.close();
                if (stage.openingController)
                    stage.openingController.replay();
            }
            onFullscreenRequested: {
                const host = stage.hostWindow;
                if (host) {
                    if (host.visibility === Window.FullScreen)
                        host.showNormal();
                    else
                        host.showFullScreen();
                }
            }
            store: stage.libraryStore
            sceneSource: visualContent
        }
    }
    Loader {
        id: experiments
        objectName: "experimentsLoader"
        x: stage.width <= 540 ? 20 : stage.width <= 1100 ? 25 : 40
        y: stage.width / stage.height < 1.05 ? 150 : stage.width >= 1800 ? 190 : stage.width <= 540
                                                                           ? 125 : stage.width
                                                                             <= 1100 ? 140 : 160
        width: Math.max(0, Math.min(stage.width <= 760 || stage.height <= 650 ? 248 : stage.width
                                                                                <= 1100 ? 254 : 272,
                                    stage.width - 2 * x))
        height: Math.max(0, stage.height - y - (stage.width / stage.height < 1.05 ? 85 : 100))
        active: stage.experimentsVisible && stage.settingsController !== null
        visible: active && (!stage.openingController || !stage.openingController.active) && (
                     !stage.panelController || !stage.panelController.surface.shown)
        sourceComponent: ExperimentPanels {
            compact: stage.width <= 760 || stage.height <= 650
            tokens: design
            settings: stage.settingsController
            lighting: stage.sceneAdapter.lighting
            sceneSource: nativeScene
            onCloseRequested: {
                stage.experimentsVisibilityRequested(false);
                browse.forceActiveFocus(Qt.OtherFocusReason);
            }
        }
    }
    Connections {
        target: stage.playbackController
        function onRejected(message) {
            stage.navigationError = message;
        }
        function onCurrentChanged() {
            stage.navigationError = "";
        }
    }
    Connections {
        target: stage.trackFocusController
        function onRejected(message) {
            stage.navigationError = message;
        }
        function onNavigationRequested() {
            stage.navigationError = "";
        }
    }
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: stage.panelController !== null && (!stage.openingController ||
                                                    !stage.openingController.active)
        onActivated: stage.panelController.open("search")
    }
    Shortcut {
        sequence: "Escape"
        enabled: (!stage.openingController || !stage.openingController.active) && (
                     !stage.panelController || !stage.panelController.surface.shown)
                 && stage.presentationController !== null &&
                 !stage.presentationController.archiveEnabled
        onActivated: stage.presentationController.back()
    }
    ArchiveText {
        tokens: design
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        visible: stage.navigationError.length > 0 || stage.preferencesError.length > 0
                 || nativeScene.errorText.length > 0
        text: stage.navigationError.length > 0 ? stage.navigationError :
                                                 stage.preferencesError.length > 0
                                                 ? stage.preferencesError : qsTr(
                                                       "Graphics configuration unavailable: ")
                                                   + nativeScene.errorText
        color: design.accent
    }
    onWidthChanged: sceneAdapter.layout.setViewport(width, height)
    onHeightChanged: sceneAdapter.layout.setViewport(width, height)
    Component.onCompleted: sceneAdapter.layout.setViewport(width, height)
}
