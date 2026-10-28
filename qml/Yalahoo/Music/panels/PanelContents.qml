// Hosts panel contents and their local focus protocol; the popup shell owns decoration.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property LibrarySession librarySession
    required property PanelController controller
    required property TrackFocusController trackFocus
    required property bool narrow
    signal closeRequested
    property PlaybackController playback: null
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property SettingsController settings: null
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    property LibraryStore store: null
    function focusContent(): void {
        if (!visible || !controller.surface.shown)
            return;
        switch (controller.panel) {
        case "search":
            search.focusSearch();
            break;
        case "library":
            library.focusEditor();
            break;
        case "settings":
            if (settingsLoader.item)
                (settingsLoader.item as SettingsPanel).focusControl();
            break;
        case "playback":
            if (playbackLoader.item)
                (playbackLoader.item as PlaybackPanel).focusControl();
            break;
        }
    }

    SearchPanel {
        id: search
        anchors.fill: parent
        visible: root.controller.panel === "search"
        enabled: visible
        heading: PanelHeading {
            width: search.width
            tokens: root.tokens
            title: qsTranslate("LibraryPanels", "Search albums and tracks")
            subtitle: qsTranslate("LibraryPanels", "FIND MUSIC")
            narrow: root.narrow
            onCloseRequested: root.closeRequested()
        }
        tokens: root.tokens
        librarySession: root.librarySession
        onResultRequested: (albumId, trackId) => root.controller.selectResult(albumId, trackId)
    }
    Loader {
        id: playbackLoader
        anchors.fill: parent
        active: root.controller.panel === "playback" && root.playback !== null
        sourceComponent: PlaybackPanel {
            tokens: root.tokens
            playback: root.playback
            backgroundMusic: root.backgroundMusic
            interfaceSound: root.interfaceSound
            heading: PanelHeading {
                width: playbackLoader.width
                tokens: root.tokens
                title: qsTranslate("LibraryPanels", "Playback and sound")
                subtitle: qsTranslate("LibraryPanels", "PLAYBACK")
                bottomSpacing: 5
                narrow: root.narrow
                onCloseRequested: root.closeRequested()
            }
        }
    }
    Loader {
        id: settingsLoader
        anchors.fill: parent
        active: root.controller.panel === "settings" && root.settings !== null
        sourceComponent: SettingsPanel {
            narrow: root.narrow
            tokens: root.tokens
            settings: root.settings
            librarySession: root.librarySession
            store: root.store
            playback: root.playback
            backgroundMusic: root.backgroundMusic
            interfaceSound: root.interfaceSound
            onReplayRequested: root.replayRequested()
            onExperimentsRequested: root.experimentsRequested()
            onFullscreenRequested: root.fullscreenRequested()
            heading: PanelHeading {
                width: settingsLoader.width
                tokens: root.tokens
                title: qsTranslate("LibraryPanels", "Playback and quality")
                subtitle: qsTranslate("LibraryPanels", "SYSTEM SETTINGS")
                narrow: root.narrow
                onCloseRequested: root.closeRequested()
            }
        }
    }
    LibraryPanel {
        id: library
        anchors.fill: parent
        visible: root.controller.panel === "library"
        enabled: visible
        heading: PanelHeading {
            width: library.width
            tokens: root.tokens
            title: qsTranslate("LibraryPanels", "Local music library")
            bottomSpacing: 8
            subtitle: qsTranslate("LibraryPanels", "MUSIC LIBRARY")
            narrow: root.narrow
            onCloseRequested: root.closeRequested()
        }
        tokens: root.tokens
        librarySession: root.librarySession
        store: root.store
    }
    Connections {
        target: root.controller
        function onChanged() {
            if (root.controller.panel) {
                root.trackFocus.cancel();
                if (root.controller.panel === "library")
                    library.loadRoots();
                else if (root.controller.panel === "search")
                    search.prepare();
                Qt.callLater(root.focusContent);
            }
        }
        function onResultRequested(albumId: string, trackId: string) {
            root.trackFocus.request(albumId, trackId);
        }
    }
}
