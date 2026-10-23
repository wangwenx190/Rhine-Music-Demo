pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

SettingsSection {
    id: root
    // The three independent audio controllers share this volume-editing interface.
    component VolumeRange: SettingRange {
        required property var controller
        visible: controller !== null
        value: controller ? controller.volume : 0
        onMoved: value => {
            if (controller)
                controller.volume = value;
        }
        suffix: "%"
        decimals: 0
        displayFactor: 100
    }
    component SoundSwitch: ArchiveCheckBox {
        required property var controller
        visible: controller !== null
        checked: controller ? controller.enabled : false
        onToggled: {
            if (controller)
                controller.enabled = checked;
        }
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
        controller: root.playback
    }
    SoundSwitch {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel", "Interface sounds")
        controller: root.interfaceSound
    }
    VolumeRange {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "Sound effect volume")
        controller: root.interfaceSound
    }
    SoundSwitch {
        tokens: root.tokens
        text: qsTranslate("SettingsPanel", "Background music")
        controller: root.backgroundMusic
    }
    VolumeRange {
        tokens: root.tokens
        label: qsTranslate("SettingsPanel", "BGM volume")
        controller: root.backgroundMusic
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
