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
    property bool qualityExpanded: false
    property bool musicBrainzExpanded: false
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    contentWidth: availableWidth

    function focusControl(): void {
        themeChoice.forceActiveFocus();
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

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Theme")
                cssPixelSize: 12
            }
            Row {
                width: parent.width
                spacing: root.narrow ? 8 : 12
                ThemeCard {
                    id: themeChoice
                    objectName: "settingsDayTheme"
                    width: (parent.width - parent.spacing) / 2
                    tokens: root.tokens
                    night: false
                    narrow: root.narrow
                    selected: root.settings.values.theme === "day"
                    onClicked: root.settings.update("theme", "day")
                }
                ThemeCard {
                    objectName: "settingsNightTheme"
                    width: (parent.width - parent.spacing) / 2
                    tokens: root.tokens
                    night: true
                    narrow: root.narrow
                    selected: root.settings.values.theme === "night"
                    onClicked: root.settings.update("theme", "night")
                }
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Library arrangement")
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsSort"
                tokens: root.tokens
                label: qsTr("Sort order")
                choices: [
                    {
                        label: qsTr("By genre"),
                        value: "genre"
                    },
                    {
                        label: qsTr("By artist"),
                        value: "artist"
                    },
                    {
                        label: qsTr("By album title"),
                        value: "album"
                    }
                ]
                selected: root.settings.values.sortMode
                onChosen: value => root.settings.update("sortMode", value)
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 12
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: qsTr(
                          "Artist sorting puts each artist in one column. Album title sorting puts 12 albums in each column.")
            }
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
                explanation: qsTr("Retain local tags, contributors, and existing metadata sources.")
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Advanced \u00b7 Album array")
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsArray"
                tokens: root.tokens
                label: qsTr("Album columns")
                choices: [
                    {
                        label: qsTr("Fill the view"),
                        value: "filled"
                    },
                    {
                        label: qsTr("Real album columns"),
                        value: "realistic"
                    }
                ]
                selected: root.settings.values.arrayMode
                onChosen: value => root.settings.update("arrayMode", value)
            }
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "settingsRemember"
                text: qsTr("Remember each column's position")
                checked: root.settings.values.rememberColumnPosition
                onToggled: root.settings.update("rememberColumnPosition", checked)
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("RENDER QUALITY")
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsQuality"
                tokens: root.tokens
                label: qsTr("Quality preset")
                choices: [
                    {
                        label: qsTr("Performance"),
                        value: "performance"
                    },
                    {
                        label: qsTr("Original"),
                        value: "original"
                    },
                    {
                        label: qsTr("High"),
                        value: "high"
                    },
                    {
                        label: qsTr("Ultra"),
                        value: "ultra"
                    }
                ].concat(root.settings.values.quality === "custom" ? [
                                                                         {
                                                                             label: qsTr("Custom"),
                                                                             value: "custom"
                                                                         }
                                                                     ] : [])
                selected: root.settings.values.quality
                onChosen: value => root.settings.update("quality", value)
            }
            ArchiveButton {
                objectName: "settingsQualityExpand"
                tokens: root.tokens
                text: (root.qualityExpanded ? "\u25be " : "\u25b8 ") + qsTr(
                          "Fine settings \u00b7 Clarity / Materials / Shadows")
                onClicked: root.qualityExpanded = !root.qualityExpanded
            }
            Column {
                width: parent.width
                spacing: 16
                visible: root.qualityExpanded
                SettingRange {
                    objectName: "settingsscale"
                    tokens: root.tokens
                    label: qsTr("Render scale")
                    minimum: 50
                    maximum: 200
                    suffix: "%"
                    decimals: 0
                    step: 5
                    value: root.settings.values.renderQuality.scale
                    onMoved: value => root.settings.updateQuality("scale", value)
                }
                SettingChoice {
                    objectName: "settingspixelRatio"
                    tokens: root.tokens
                    label: qsTr("Pixel density limit")
                    choices: [
                        {
                            label: "1\u00d7",
                            value: 1
                        },
                        {
                            label: "1.5\u00d7",
                            value: 1.5
                        },
                        {
                            label: "2\u00d7",
                            value: 2
                        },
                        {
                            label: "3\u00d7",
                            value: 3
                        }
                    ]
                    selected: root.settings.values.renderQuality.pixelRatio
                    onChosen: value => root.settings.updateQuality("pixelRatio", value)
                }
                SettingChoice {
                    objectName: "settingsantialias"
                    tokens: root.tokens
                    label: qsTr("Antialiasing")
                    choices: [
                        {
                            label: qsTr("Original"),
                            value: "off"
                        },
                        {
                            label: "SMAA",
                            value: "smaa"
                        }
                    ]
                    selected: root.settings.values.renderQuality.antialias
                    onChosen: value => root.settings.updateQuality("antialias", value)
                }
                SettingChoice {
                    objectName: "settingsanisotropy"
                    tokens: root.tokens
                    label: qsTr("Anisotropic filtering")
                    choices: [
                        {
                            label: "1\u00d7",
                            value: 1
                        },
                        {
                            label: "2\u00d7",
                            value: 2
                        },
                        {
                            label: "4\u00d7",
                            value: 4
                        },
                        {
                            label: "8\u00d7",
                            value: 8
                        },
                        {
                            label: "16\u00d7",
                            value: 16
                        }
                    ]
                    selected: root.settings.values.renderQuality.anisotropy
                    onChosen: value => root.settings.updateQuality("anisotropy", value)
                }
                SettingChoice {
                    objectName: "settingstransmission"
                    tokens: root.tokens
                    label: qsTr("Transmission resolution")
                    choices: [
                        {
                            label: "25%",
                            value: 0.25
                        },
                        {
                            label: "50%",
                            value: 0.5
                        },
                        {
                            label: "75%",
                            value: 0.75
                        },
                        {
                            label: "100%",
                            value: 1
                        }
                    ]
                    selected: root.settings.values.renderQuality.transmission
                    onChosen: value => root.settings.updateQuality("transmission", value)
                }
                SettingChoice {
                    objectName: "settingsshadows"
                    tokens: root.tokens
                    label: qsTr("Shadow resolution")
                    choices: [
                        {
                            label: qsTr("Off"),
                            value: 0
                        },
                        {
                            label: "1024",
                            value: 1024
                        },
                        {
                            label: "2048",
                            value: 2048
                        },
                        {
                            label: "4096",
                            value: 4096
                        }
                    ]
                    selected: root.settings.values.renderQuality.shadows
                    onChosen: value => root.settings.updateQuality("shadows", value)
                }
                SettingChoice {
                    objectName: "settingsaoSamples"
                    tokens: root.tokens
                    label: qsTr("Ambient occlusion samples")
                    choices: [
                        {
                            label: qsTr("Off"),
                            value: 0
                        },
                        {
                            label: qsTr("16 samples"),
                            value: 16
                        },
                        {
                            label: qsTr("32 samples"),
                            value: 32
                        },
                        {
                            label: qsTr("64 samples"),
                            value: 64
                        }
                    ]
                    selected: root.settings.values.renderQuality.aoSamples
                    onChosen: value => root.settings.updateQuality("aoSamples", value)
                }
                SettingChoice {
                    objectName: "settingsaoResolution"
                    tokens: root.tokens
                    label: qsTr("Ambient occlusion resolution")
                    choices: [
                        {
                            label: "50%",
                            value: 0.5
                        },
                        {
                            label: "75%",
                            value: 0.75
                        },
                        {
                            label: "100%",
                            value: 1
                        }
                    ]
                    selected: root.settings.values.renderQuality.aoResolution
                    onChosen: value => root.settings.updateQuality("aoResolution", value)
                }
                SettingRange {
                    objectName: "settingsdepthOfField"
                    tokens: root.tokens
                    label: qsTr("Array depth of field")
                    minimum: 0
                    maximum: 150
                    suffix: "%"
                    decimals: 0
                    step: 5
                    value: root.settings.values.renderQuality.depthOfField
                    onMoved: value => root.settings.updateQuality("depthOfField", value)
                }
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 11
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: qsTr(
                          "Applies immediately and saves automatically. Device size and the 8.29 megapixel target limit protect output size while preserving your selected quality preferences.")
            }
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
            SettingRange {
                objectName: "settingsSpeed"
                tokens: root.tokens
                label: qsTr("Animation speed")
                suffix: "\u00d7"
                minimum: .25
                maximum: 3
                step: .05
                value: root.settings.values.motionSpeed
                onMoved: value => root.settings.update("motionSpeed", value)
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
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Sound")
                cssPixelSize: 12
            }
            SettingRange {
                tokens: root.tokens
                label: qsTr("Track volume")
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.playback !== null
                value: root.playback ? root.playback.volume : 0
                onMoved: value => {
                    if (root.playback)
                        root.playback.volume = value;
                }
            }
            ArchiveCheckBox {
                tokens: root.tokens
                text: qsTr("Interface sounds")
                visible: root.interfaceSound !== null
                checked: root.interfaceSound ? root.interfaceSound.enabled : false
                onToggled: {
                    if (root.interfaceSound)
                        root.interfaceSound.enabled = checked;
                }
            }
            SettingRange {
                tokens: root.tokens
                label: qsTr("Sound effect volume")
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.interfaceSound !== null
                value: root.interfaceSound ? root.interfaceSound.volume : 0
                onMoved: value => {
                    if (root.interfaceSound)
                        root.interfaceSound.volume = value;
                }
            }
            ArchiveCheckBox {
                tokens: root.tokens
                text: qsTr("Background music")
                visible: root.backgroundMusic !== null
                checked: root.backgroundMusic ? root.backgroundMusic.enabled : false
                onToggled: {
                    if (root.backgroundMusic)
                        root.backgroundMusic.enabled = checked;
                }
            }
            SettingRange {
                tokens: root.tokens
                label: qsTr("BGM volume")
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.backgroundMusic !== null
                value: root.backgroundMusic ? root.backgroundMusic.volume : 0
                onMoved: value => {
                    if (root.backgroundMusic)
                        root.backgroundMusic.volume = value;
                }
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Preview interface sound \u2197")
                enabled: root.interfaceSound !== null && root.interfaceSound.enabled
                         && root.interfaceSound.available
                onClicked: root.interfaceSound.preview()
            }
            SettingChoice {
                tokens: root.tokens
                label: qsTr("Track output device")
                visible: root.playback !== null
                choices: root.playback ? root.playback.deviceChoices : []
                selected: root.playback ? root.playback.selectedDevice : ""
                onChosen: value => {
                    if (root.playback)
                        root.playback.selectDevice(value);
                }
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 11
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: qsTr(
                          "BGM and interface effects use the system default output. All three volumes are independent. Events without a dedicated short sound remain silent.")
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveButton {
                objectName: "settingsMusicBrainzExpand"
                tokens: root.tokens
                text: (root.musicBrainzExpanded ? "\u25be " : "\u25b8 ") + qsTr(
                          "Advanced \u00b7 MusicBrainz")
                onClicked: root.musicBrainzExpanded = !root.musicBrainzExpanded
            }
            Column {
                width: parent.width
                spacing: 16
                visible: root.musicBrainzExpanded
                MetadataSection {
                    tokens: root.tokens
                    librarySession: root.librarySession
                    kind: "enrichment"
                    title: "MusicBrainz"
                    explanation: qsTr(
                                     "The local cache retains album titles, artists, genres, contributors, and sources. Manual classification takes priority.")
                }
                ArchiveTextField {
                    id: contact
                    tokens: root.tokens
                    objectName: "settingsMetadataContact"
                    width: parent.width
                    text: root.librarySession.metadataConfiguration.musicBrainzContact || ""
                    placeholderText: qsTr("Contact email or public project URL")
                    Accessible.name: qsTr("MusicBrainz contact email or project URL")
                }
                ArchiveCheckBox {
                    id: onlineEnabled
                    tokens: root.tokens
                    text: qsTr(
                              "Automatically enrich new albums after scanning (network access is not enabled yet)")
                    checked: root.librarySession.metadataConfiguration.onlineEnabled || false
                    enabled: root.librarySession.providerCapabilities.networkEnabled && root.store
                             !== null && !root.store.busy
                    onToggled: root.store.setMetadataConfiguration({
                                                                       onlineEnabled: checked
                                                                   })
                }
                ArchiveButton {
                    tokens: root.tokens
                    text: qsTr("Save metadata settings \u2197")
                    enabled: root.store !== null && !root.store.busy
                    onClicked: root.store.setMetadataConfiguration({
                                                                       musicBrainzContact:
                                                                       contact.text
                                                                   })
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    cssPixelSize: 11
                    color: root.tokens.muted
                    wrapMode: Text.Wrap
                    text: root.store ? root.store.error : qsTr(
                                           "Metadata configuration is saved through explicit import or the native music library.")
                }
            }
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

            Image {
                objectName: "settingsBrandIcon"
                width: 48
                height: 48
                source: "qrc:/assets/icons/yalahoo/yalahoo.svg"
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                fillMode: Image.PreserveAspectFit
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
