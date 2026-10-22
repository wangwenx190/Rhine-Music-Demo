// Binds product preferences to typed C++ settings commands. Visual reload and media intent have
// separate owners so editing quality does not recreate playback.

pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    objectName: "settingsPanel"
    required property SettingsController settings
    required property LibrarySession librarySession
    property LibraryStore store: null
    property PlaybackController playback: null
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property Component heading: null
    property bool navigationInputOwner: true
    property bool narrow: false
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    contentWidth: availableWidth

    function focusControl(): void {
        appearance.focusControl();
    }
    Column {
        width: root.availableWidth
        spacing: 0
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        SettingsSection {
            tokens: root.tokens
            ArchiveCheckBox {
                objectName: "settingsSingleInstance"
                tokens: root.tokens
                text: qsTr("Single instance")
                checked: root.settings.values.singleInstance
                onToggled: root.settings.update("singleInstance", checked)
            }
            PanelDescription {
                tokens: root.tokens
                cssPixelSize: 12
                text: qsTr(
                          "Applies after restarting the application. Opening it again brings the existing window forward.")
            }
        }
        AppearanceSettings {
            id: appearance
            width: parent.width
            tokens: root.tokens
            settings: root.settings
            narrow: root.narrow
        }
        SettingsSection {
            tokens: root.tokens

            MetadataSection {
                tokens: root.tokens
                librarySession: root.librarySession
                kind: "introduction"
                title: qsTr("Album introductions")
                explanation: qsTr(
                                 "Public encyclopedia introductions and their sources are saved locally.")
            }
        }
        SettingsSection {
            tokens: root.tokens

            MetadataSection {
                tokens: root.tokens
                librarySession: root.librarySession
                kind: "credits"
                title: qsTr("Credits")
                explanation: qsTr(
                                 "Query MusicBrainz credits while retaining local tags and other sources.")
            }
        }

        ArrayQualitySettings {
            width: parent.width
            tokens: root.tokens
            settings: root.settings
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Motion and display")
                cssPixelSize: 12
            }
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "settingsReduced"
                text: qsTr("Reduce motion")
                checked: root.settings.values.reduced
                onToggled: root.settings.update("reduced", checked)
            }
            MotionSpeedRange {
                objectName: "settingsSpeed"
                tokens: root.tokens
                settings: root.settings
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Toggle fullscreen \u2197")
                onClicked: root.fullscreenRequested()
            }
            ArchiveButton {
                objectName: "settingsReplay"
                tokens: root.tokens
                text: qsTr("Replay opening \u2197")
                onClicked: root.replayRequested()
            }
            ArchiveButton {
                objectName: "settingsReload"
                tokens: root.tokens
                text: qsTr("Reload interface \u21bb")
                onClicked: root.settings.reload()
            }
        }
        SoundSettings {
            tokens: root.tokens
            playback: root.playback
            backgroundMusic: root.backgroundMusic
            interfaceSound: root.interfaceSound
        }
        MetadataSettings {
            tokens: root.tokens
            librarySession: root.librarySession
            store: root.store
        }
        SettingsSection {
            tokens: root.tokens
            ArchiveButton {
                objectName: "settingsExperiments"
                tokens: root.tokens
                text: qsTr("Lighting / speed tools \u2197")
                onClicked: root.experimentsRequested()
            }
        }
        SettingsSection {
            tokens: root.tokens
            last: true

            ArchiveVectorImage {
                objectName: "settingsBrandIcon"
                width: 48
                height: 48
                source: "qrc:/assets/icons/yalahoo/yalahoo.svg"
                smooth: true
                Accessible.ignored: true
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 12
                wrapMode: Text.Wrap
                text: qsTr(
                          "Development and assets \u00b7 yalahOO!!!\nMusic adaptation and maintenance: RonaldDeng\nOriginal interface: LBEILC / RhineLabUI\nMIT licensed code. Asset and font licenses are included with the application.")
            }
        }
    }
}
