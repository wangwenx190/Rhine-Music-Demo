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
        ArchiveText {
            tokens: root.tokens
            text: qsTr("Track volume")
            cssPixelSize: 12
        }
        ArchiveSlider {
            tokens: root.tokens
            width: parent.width
            from: 0
            to: 1
            value: root.playback.volume
            onMoved: root.playback.volume = value
            Accessible.name: qsTr("Track volume")
        }
        Column {
            width: parent.width
            spacing: 14
            visible: root.backgroundMusic !== null
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "bgmEnabled"
                text: qsTr("Background music")
                checked: root.backgroundMusic ? root.backgroundMusic.enabled : false
                onToggled: if (root.backgroundMusic)
                               root.backgroundMusic.enabled = checked
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: qsTr(
                          "Fades out before a track starts. Pausing stays quiet; stopping or reaching the end of the queue restores it.")
                cssPixelSize: 11
                wrapMode: Text.Wrap
                color: root.tokens.muted
            }
            ArchiveSlider {
                tokens: root.tokens
                objectName: "bgmVolume"
                width: parent.width
                from: 0
                to: 1
                value: root.backgroundMusic ? root.backgroundMusic.volume : 0
                onMoved: if (root.backgroundMusic)
                             root.backgroundMusic.volume = value
                Accessible.name: qsTr("BGM volume")
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: root.backgroundMusic ? root.backgroundMusic.error : ""
                visible: text.length > 0
                cssPixelSize: 11
                wrapMode: Text.WrapAnywhere
                color: root.tokens.accent
            }
        }
        Column {
            width: parent.width
            spacing: 14
            visible: root.interfaceSound !== null
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "interfaceSoundEnabled"
                text: qsTr("Interface sounds")
                checked: root.interfaceSound ? root.interfaceSound.enabled : false
                onToggled: if (root.interfaceSound)
                               root.interfaceSound.enabled = checked
            }
            ArchiveSlider {
                tokens: root.tokens
                objectName: "interfaceSoundVolume"
                width: parent.width
                from: 0
                to: 1
                value: root.interfaceSound ? root.interfaceSound.volume : 0
                onMoved: if (root.interfaceSound)
                             root.interfaceSound.volume = value
                Accessible.name: qsTr("Interface sound volume")
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Preview interface sound \u2197")
                enabled: root.interfaceSound !== null && root.interfaceSound.enabled
                         && root.interfaceSound.available
                onClicked: root.interfaceSound.preview()
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: qsTr(
                          "Events without a dedicated short sound remain silent. BGM and effects use the system default output with separate volume controls.")
                cssPixelSize: 11
                wrapMode: Text.Wrap
                color: root.tokens.muted
            }
        }
        ArchiveText {
            tokens: root.tokens
            text: qsTr("Output device")
            cssPixelSize: 12
        }
        ArchiveComboBox {
            id: output
            tokens: root.tokens
            objectName: "songOutputDevices"
            width: parent.width
            model: root.playback.devices
            textRole: "name"
            valueRole: "id"
            currentValue: root.playback.selectedDevice
            onActivated: root.playback.selectDevice(currentValue)
            Accessible.name: qsTr("Track output device")
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
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: qsTr(
                      "Plays local files through the system audio output. Choose another track or retry if the format is unavailable.")
            cssPixelSize: 11
            wrapMode: Text.Wrap
            color: root.tokens.muted
        }
    }
}
