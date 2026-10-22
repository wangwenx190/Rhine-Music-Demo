// Hosts modal panels, experiment tools, shortcuts and navigation feedback above the scene.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property LibrarySession librarySession
    property PanelController panelController: null
    property TrackFocusController trackFocusController: null
    property PlaybackController playbackController: null
    property BackgroundMusicController backgroundMusicController: null
    property InterfaceSoundController interfaceSoundController: null
    property SettingsController settingsController: null
    property LibraryStore libraryStore: null
    property OpeningController openingController: null
    property PresentationController presentationController: null
    required property SceneItem sceneSource
    required property Item backdropSource
    property string preferencesError: ""
    property string navigationError: ""
    required property bool experimentsVisible
    readonly property ExperimentPanels experimentPanel: experiments.panel
    readonly property var hostWindow: Window.window
    signal experimentsVisibilityRequested(bool shown)
    signal returnFocusRequested

    Loader {
        anchors.fill: parent
        active: root.panelController !== null && root.trackFocusController !== null
        sourceComponent: LibraryPanels {
            tokens: root.tokens
            librarySession: root.librarySession
            controller: root.panelController
            trackFocus: root.trackFocusController
            playback: root.playbackController
            backgroundMusic: root.backgroundMusicController
            interfaceSound: root.interfaceSoundController
            settings: root.settingsController
            onExperimentsRequested: {
                root.panelController.close();
                root.experimentsVisibilityRequested(true);
            }
            onReplayRequested: {
                root.panelController.close();
                if (root.openingController)
                    root.openingController.replay();
            }
            onFullscreenRequested: {
                const host = root.hostWindow;
                if (host) {
                    if (host.visibility === Window.FullScreen)
                        host.showNormal();
                    else
                        host.showFullScreen();
                }
            }
            store: root.libraryStore
            sceneSource: root.backdropSource
        }
    }
    ExperimentOverlay {
        id: experiments
        tokens: root.tokens
        settingsController: root.settingsController
        lighting: root.sceneSource.adapter.lighting
        openingController: root.openingController
        panelController: root.panelController
        sceneSource: root.sceneSource
        viewportWidth: root.width
        viewportHeight: root.height
        experimentsVisible: root.experimentsVisible
        onCloseRequested: {
            root.experimentsVisibilityRequested(false);
            root.returnFocusRequested();
        }
    }
    Connections {
        target: root.playbackController
        function onRejected(message) {
            root.navigationError = message;
        }
        function onCurrentChanged() {
            root.navigationError = "";
        }
    }
    Connections {
        target: root.trackFocusController
        function onRejected(message) {
            root.navigationError = message;
        }
        function onNavigationRequested() {
            root.navigationError = "";
        }
    }
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.panelController !== null && (!root.openingController ||
                                                   !root.openingController.active)
        onActivated: root.panelController.open("search")
    }
    Shortcut {
        sequence: "Escape"
        enabled: (!root.openingController || !root.openingController.active) && (
                     !root.panelController || !root.panelController.surface.shown)
                 && root.presentationController !== null &&
                 !root.presentationController.archiveEnabled
        onActivated: root.presentationController.back()
    }
    ArchiveText {
        tokens: root.tokens
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        visible: root.navigationError.length > 0 || root.preferencesError.length > 0
                 || root.sceneSource.errorText.length > 0
        text: root.navigationError.length > 0 ? root.navigationError : root.preferencesError.length
                                                > 0 ? root.preferencesError : qsTranslate(
                                                          "MusicStage",
                                                          "Graphics configuration unavailable: ")
                                                      + root.sceneSource.errorText
        color: root.tokens.accent
    }
}
