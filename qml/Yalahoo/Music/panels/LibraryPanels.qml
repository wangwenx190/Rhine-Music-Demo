// Lazily hosts library/search panels and routes their modal input ownership. Provider and
// library work remain in C++ services instead of delegate callbacks.

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

T.Popup {
    id: root
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
    onOpened: Qt.callLater(contents.focusContent)
    function dismiss(): void {
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
            sourceComponent: BackdropBlur {
                anchors.fill: parent
                hdrActive: root.tokens.hdrActive
                sceneSource: root.sceneSource
                sceneX: 0
                sceneY: 0
                blurRadius: 0
                horizontalPass: horizontal
                BackdropBlurPass {
                    id: horizontal
                    backdrop: parent
                    horizontal: true
                    fragmentShader: "qrc:/assets/shaders/panel-blur.frag.qsb"
                }
                BackdropBlurPass {
                    backdrop: parent
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
            PanelContents {
                id: contents
                anchors.fill: parent
                anchors.leftMargin: root.insetX
                anchors.rightMargin: root.insetX
                anchors.topMargin: root.insetY
                anchors.bottomMargin: root.insetY
                tokens: root.tokens
                librarySession: root.librarySession
                controller: root.controller
                trackFocus: root.trackFocus
                playback: root.playback
                backgroundMusic: root.backgroundMusic
                interfaceSound: root.interfaceSound
                settings: root.settings
                store: root.store
                narrow: root.narrow
                onCloseRequested: root.dismiss()
                onReplayRequested: root.replayRequested()
                onExperimentsRequested: root.experimentsRequested()
                onFullscreenRequested: root.fullscreenRequested()
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
}
