pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    objectName: "audioSettingsPanel"
    required property PlaybackController playback
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property Component heading: null
    property bool navigationInputOwner: true
    contentWidth: availableWidth

    function focusControl(): void {
        playToggle.forceActiveFocus();
    }
    Column {
        width: root.availableWidth
        spacing: 18
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: root.playback.title || qsTr("No track selected")
            cssPixelSize: 16
            wrapMode: Text.WrapAnywhere
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: !root.playback.outputAvailable ? (root.playback.requested ? qsTr(
                                                                                  "Waiting for an audio output device") :
                                                                              qsTr("No audio output devices")) :
                                                   root.playback.loading ? qsTr("Loading track...") :
                                                                           root.playback.playing
                                                                           ? qsTr("Playing") :
                                                                             root.playback.state
                                                                             === "paused" ? qsTr(
                                                                                                "Paused") :
                                                                                            qsTr("Stopped")
            cssPixelSize: 11
            color: root.tokens.muted
        }
        PlaybackControls {
            width: parent.width
            tokens: root.tokens
            playback: root.playback
        }
        Row {
            spacing: 20
            ArchiveButton {
                id: playToggle
                tokens: root.tokens
                text: root.playback.requested ? qsTr("Pause") : qsTr("Play")
                onClicked: root.playback.toggle()
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Stop")
                onClicked: root.playback.stop()
            }
        }
        SoundSettings {
            tokens: root.tokens
            playback: root.playback
            backgroundMusic: root.backgroundMusic
            interfaceSound: root.interfaceSound
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: root.playback.notice
            visible: text.length > 0
            cssPixelSize: 11
            wrapMode: Text.Wrap
            color: root.tokens.accent
        }
        ArchiveText {
            objectName: "songPlaybackError"
            tokens: root.tokens
            width: parent.width
            text: root.playback.error
            visible: text.length > 0
            cssPixelSize: 11
            wrapMode: Text.WrapAnywhere
            color: root.tokens.accent
        }
        Row {
            visible: root.playback.error.length > 0
            spacing: 20
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Retry")
                onClicked: root.playback.retry()
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Next track")
                enabled: root.playback.canNext
                onClicked: root.playback.next()
            }
        }
        PanelDescription {
            tokens: root.tokens
            text: qsTr(
                      "Plays local files through the system audio output. Choose another track or retry if the format is unavailable.")
        }
    }
}
