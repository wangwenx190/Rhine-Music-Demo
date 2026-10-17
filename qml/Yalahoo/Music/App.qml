pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "pages"
import "components"

T.ApplicationWindow {
    id: window
    required property LibrarySession librarySession
    required property MotionController motionController
    required property SceneAdapter sceneAdapter
    property StartupCoordinator startupCoordinator: null
    property OpeningController openingController: null
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    property PresentationController presentationController: null
    property PlaybackController playbackController: null
    property BackgroundMusicController backgroundMusicController: null
    property InterfaceSoundController interfaceSoundController: null
    property TrackFocusController trackFocusController: null
    property SettingsController settingsController: null
    property PanelController panelController: null
    property LibraryStore libraryStore: null
    property DetailModel detailModel: null
    property OverviewController overviewController: null
    property string reviewPlayingTitle: ""
    property string preferencesError: ""
    property bool lightingLab: false
    property bool geometryProbe: false
    property bool verificationRun: false
    property DisplayOutput displayOutput: null
    property int verificationFlags: Qt.Window
    width: 1280
    height: 720
    minimumWidth: 320
    minimumHeight: 240
    visible: false
    flags: verificationRun ? verificationFlags : Qt.Window
    title: "yalahOO!!!"
    color: displayOutput && (!displayOutput.ready || displayOutput.hdrActive) ? "black" :
                                                                                sceneAdapter.lighting.night
                                                                                ? "#08121f" :
                                                                                  "#e8e5e1"
    contentItem: DisplaySurface {
        output: window.displayOutput
        backgroundColor: window.sceneAdapter.lighting.night ? "#08121f" : "#e8e5e1"
    }
    Loader {
        id: visualTree
        anchors.fill: parent
        sourceComponent: MusicStage {
            hdrHeadroom: window.displayOutput ? window.displayOutput.headroom : 1
            hdrActive: window.displayOutput ? window.displayOutput.hdrActive : false
            anchors.fill: parent
            librarySession: window.librarySession
            motionController: window.motionController
            sceneAdapter: window.sceneAdapter
            startupCoordinator: window.startupCoordinator
            openingController: window.openingController
            inputRouter: window.inputRouter
            rulerController: window.rulerController
            presentationController: window.presentationController
            playbackController: window.playbackController
            backgroundMusicController: window.backgroundMusicController
            interfaceSoundController: window.interfaceSoundController
            trackFocusController: window.trackFocusController
            settingsController: window.settingsController
            panelController: window.panelController
            libraryStore: window.libraryStore
            detailModel: window.detailModel
            overviewController: window.overviewController
            reviewPlayingTitle: window.reviewPlayingTitle
            preferencesError: window.preferencesError
            experimentsVisible: window.lightingLab
            onExperimentsVisibilityRequested: shown => window.lightingLab = shown
            geometryProbe: window.geometryProbe
        }
    }
    function rebuildVisualTree(): void {
        visualTree.active = true;
    }
    Connections {
        target: window.settingsController
        function onReloadRequested() {
            if (window.panelController)
                window.panelController.reset();
            visualTree.active = false;
            Qt.callLater(window.rebuildVisualTree);
        }
    }
}
