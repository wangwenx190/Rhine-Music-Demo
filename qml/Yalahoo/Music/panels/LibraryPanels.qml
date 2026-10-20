pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

T.Popup {
    id: root
    readonly property real outputDpr: sceneSource.Window.window
                                      ? sceneSource.Window.window.devicePixelRatio : 1
    required property DesignTokens tokens
    required property LibrarySession librarySession
    required property PanelController controller
    required property TrackFocusController trackFocus
    property PlaybackController playback: null
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property SettingsController settings: null
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    property LibraryStore store: null
    required property Item sceneSource
    property string pendingAlbum: ""
    property string pendingTrack: ""
    readonly property bool narrow: width <= 700
    readonly property real outerPadding: narrow ? 12 : 28
    readonly property real insetX: narrow ? 21 : 35
    readonly property real insetY: narrow ? 23 : 30
    objectName: "libraryPanels"
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720
    padding: 0
    popupType: T.Popup.Item
    modal: true
    dim: false
    focus: true
    visible: controller.surface.shown
    closePolicy: T.Popup.NoAutoClose
    enter: null
    exit: null
    background: Item {}
    onOpened: Qt.callLater(focusContent)
    function focusContent(): void {
        if (controller.panel === "search")
            search.focusSearch();
        else if (controller.panel === "library")
            library.focusEditor();
        else if (settingsLoader.item)
            (settingsLoader.item as SettingsPanel).focusControl();
        else if (playbackLoader.item)
            (playbackLoader.item as PlaybackPanel).focusControl();
    }
    function dismiss(): void {
        pendingAlbum = "";
        pendingTrack = "";
        controller.close();
    }
    function selectResult(albumId: string, trackId: string): void {
        pendingAlbum = albumId;
        pendingTrack = trackId;
        controller.close();
    }
    contentItem: Item {
        property bool navigationInputOwner: true
        opacity: root.controller.surface.opacity
        Keys.onEscapePressed: root.dismiss()
        Loader {
            objectName: "panelBackdropLoader"
            anchors.fill: parent
            active: root.visible
            sourceComponent: Item {
                ShaderEffectSource {
                    id: backdropTexture
                    format: root.tokens.hdrActive ? ShaderEffectSource.RGBA16F :
                                                    ShaderEffectSource.RGBA8
                    mipmap: true
                    sourceItem: root.sceneSource
                    anchors.fill: parent
                    textureSize: Qt.size(Math.round(width * root.outputDpr), Math.round(height
                                                                                        * root.outputDpr))
                    visible: false
                    smooth: true
                }
                ShaderEffect {
                    id: horizontal
                    anchors.fill: parent
                    visible: false
                    property var source: backdropTexture
                    property vector2d sampleStep: Qt.vector2d(1 / width, 0)
                    fragmentShader: "qrc:/assets/shaders/panel-blur.frag.qsb"
                }
                ShaderEffectSource {
                    id: intermediate
                    format: root.tokens.hdrActive ? ShaderEffectSource.RGBA16F :
                                                    ShaderEffectSource.RGBA8
                    mipmap: true
                    sourceItem: horizontal
                    anchors.fill: parent
                    textureSize: Qt.size(Math.round(width * root.outputDpr), Math.round(height
                                                                                        * root.outputDpr))
                    visible: false
                    hideSource: true
                    smooth: true
                }
                ShaderEffect {
                    anchors.fill: parent
                    property var source: intermediate
                    property vector2d sampleStep: Qt.vector2d(0, 1 / height)
                    fragmentShader: "qrc:/assets/shaders/panel-blur.frag.qsb"
                }
            }
        }
        Rectangle {
            antialiasing: true
            anchors.fill: parent
            color: Qt.rgba(4 / 255, 12 / 255, 19 / 255, .25)
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.dismiss()
        }
        ShaderEffect {
            anchors.fill: parent
            property vector4d bounds: Qt.vector4d(pane.x, pane.y + 24, pane.width, pane.height)
            property vector2d viewportSize: Qt.vector2d(width, height)
            fragmentShader: "qrc:/assets/shaders/panel-shadow.frag.qsb"
        }
        Rectangle {
            id: pane
            antialiasing: true
            objectName: "libraryPanelSurface"
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2 + root.controller.surface.offset
            width: Math.min(650, Math.max(0, parent.width - root.outerPadding * 2))
            height: Math.min(710, Math.max(0, parent.height - root.outerPadding * 2 - (root.narrow
                                                                                       ? 16 : 20)))
            color: root.tokens.night ? Qt.rgba(17 / 255, 27 / 255, 42 / 255, .97) : Qt.rgba(245 / 255,
                                                                                            244 / 255,
                                                                                            239 / 255,
                                                                                            .97)
            border.color: root.tokens.line
            MouseArea {
                anchors.fill: parent
            }
            Item {
                anchors.fill: parent
                anchors.leftMargin: root.insetX
                anchors.rightMargin: root.insetX
                anchors.topMargin: root.insetY
                anchors.bottomMargin: root.insetY
                SearchPanel {
                    id: search
                    anchors.fill: parent
                    visible: root.controller.panel === "search"
                    enabled: visible
                    heading: PanelHeading {
                        width: search.width
                        tokens: root.tokens
                        title: qsTr("Search albums and tracks")
                        subtitle: qsTr("FIND MUSIC")
                        narrow: root.narrow
                        onCloseRequested: root.dismiss()
                    }
                    tokens: root.tokens
                    librarySession: root.librarySession
                    onResultRequested: (albumId, trackId) => root.selectResult(albumId, trackId)
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
                            title: qsTr("Playback and sound")
                            subtitle: qsTr("PLAYBACK")
                            bottomSpacing: 5
                            narrow: root.narrow
                            onCloseRequested: root.dismiss()
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
                            title: qsTr("Playback and quality")
                            subtitle: qsTr("SYSTEM SETTINGS")
                            narrow: root.narrow
                            onCloseRequested: root.dismiss()
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
                        title: qsTr("Local music library")
                        bottomSpacing: 8
                        subtitle: qsTr("MUSIC LIBRARY")
                        narrow: root.narrow
                        onCloseRequested: root.dismiss()
                    }
                    tokens: root.tokens
                    librarySession: root.librarySession
                    store: root.store
                }
            }
            Accessible.role: Accessible.Dialog
            Accessible.name: root.controller.panel === "settings" ? qsTr("Playback and quality") :
                                                                    root.controller.panel
                                                                    === "search" ? qsTr(
                                                                                       "Search albums and tracks") :
                                                                                   root.controller.panel
                                                                                   === "playback"
                                                                                   ? qsTr("Playback and sound") :
                                                                                     qsTr("Local music library")
        }
    }
    Connections {
        target: root.controller
        function onChanged() {
            if (root.controller.panel) {
                root.pendingAlbum = root.pendingTrack = "";
                root.trackFocus.cancel();
                if (root.controller.panel === "library")
                    library.loadRoots();
                else if (root.controller.panel === "search")
                    search.prepare();
                Qt.callLater(root.focusContent);
            }
        }
        function onClosed() {
            const album = root.pendingAlbum;
            const track = root.pendingTrack;
            root.pendingAlbum = root.pendingTrack = "";
            if (album)
                root.trackFocus.request(album, track);
        }
    }
}
