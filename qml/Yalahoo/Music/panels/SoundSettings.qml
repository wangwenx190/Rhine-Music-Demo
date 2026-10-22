pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

SettingsSection {
    id: root
    component VolumeRange: SettingRange {
        suffix: "%"
        decimals: 0
        displayFactor: 100
    }
    property PlaybackController playback: null
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null

    ArchiveText {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel", "Sound")
        cssPixelSize: 12
    }
    VolumeRange {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "Track volume")
        visible: root.playback !== null
        value: root.playback ? root.playback.volume : 0
        onMoved: value => {
            if (root.playback)
                root.playback.volume = value;
        }
    }
    ArchiveCheckBox {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel", "Interface sounds")
        visible: root.interfaceSound !== null
        checked: root.interfaceSound ? root.interfaceSound.enabled : false
        onToggled: {
            if (root.interfaceSound)
                root.interfaceSound.enabled = checked;
        }
    }
    VolumeRange {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "Sound effect volume")
        visible: root.interfaceSound !== null
        value: root.interfaceSound ? root.interfaceSound.volume : 0
        onMoved: value => {
            if (root.interfaceSound)
                root.interfaceSound.volume = value;
        }
    }
    ArchiveCheckBox {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel", "Background music")
        visible: root.backgroundMusic !== null
        checked: root.backgroundMusic ? root.backgroundMusic.enabled : false
        onToggled: {
            if (root.backgroundMusic)
                root.backgroundMusic.enabled = checked;
        }
    }
    VolumeRange {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "BGM volume")
        visible: root.backgroundMusic !== null
        value: root.backgroundMusic ? root.backgroundMusic.volume : 0
        onMoved: value => {
            if (root.backgroundMusic)
                root.backgroundMusic.volume = value;
        }
    }
    InterfaceSoundPreview {
        tokens: root.tokens
        interfaceSound: root.interfaceSound
    }
    SettingChoice {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "Track output device")
        visible: root.playback !== null
        choices: root.playback ? root.playback.deviceChoices : []
        selected: root.playback ? root.playback.selectedDevice : ""
        onChosen: value => {
            if (root.playback)
                root.playback.selectDevice(value);
        }
    }
    PanelDescription {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel",
                          "BGM and interface effects use the system default output. All three volumes are independent. Events without a dedicated short sound remain silent.")
    }
}
