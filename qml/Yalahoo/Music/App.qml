pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "pages"

ApplicationWindow {
    id: window
    required property LibrarySession librarySession
    required property MotionController motionController
    required property SceneAdapter sceneAdapter
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
    property int verificationFlags: Qt.Window
    width: 1280
    height: 720
    minimumWidth: 320
    minimumHeight: 240
    visible: !verificationRun
    flags: verificationRun ? verificationFlags : Qt.Window
    title: "yalahOO!!!"
    color: sceneAdapter.lighting.night ? "#08121f" : "#e8e5e1"
    Loader {
        id: visualTree
        anchors.fill: parent
        sourceComponent: MusicStage {
            anchors.fill: parent
            librarySession: window.librarySession
            motionController: window.motionController
            sceneAdapter: window.sceneAdapter
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
    function rebuildVisualTree() {
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
