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
    property bool musicBrainzExpanded: false
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    contentWidth: availableWidth

    component VolumeRange: SettingRange {
        suffix: "%"
        decimals: 0
        displayFactor: 100
    }

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
            PanelDescription {
                tokens: root.tokens
                cssPixelSize: 12
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
                explanation: qsTr(
                                 "Query MusicBrainz credits while retaining local tags and other sources.")
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
            PanelDescription {
                tokens: root.tokens
                text: qsTr(
                          "Renders at the display's native pixel density. Antialiasing and full-resolution effects remain enabled.")
            }
            Column {
                width: parent.width
                spacing: 16
                SettingRange {
                    objectName: "settingsdepthOfField"
                    tokens: root.tokens
                    label: qsTr("Array depth of field")
                    minimum: 0
                    maximum: 300
                    suffix: "%"
                    decimals: 0
                    step: 5
                    value: root.settings.values.renderQuality.depthOfField
                    onMoved: value => root.settings.updateQuality("depthOfField", value)
                }
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
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: qsTr("Sound")
                cssPixelSize: 12
            }
            VolumeRange {
                tokens: root.tokens
                label: qsTr("Track volume")
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
            VolumeRange {
                tokens: root.tokens
                label: qsTr("Sound effect volume")
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
            VolumeRange {
                tokens: root.tokens
                label: qsTr("BGM volume")
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
                label: qsTr("Track output device")
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
                          "Advanced \u00b7 Online metadata")
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
                                     "MusicBrainz provides release identifiers and genres. Local titles and manual classification are preserved.")
                }
                ArchiveCheckBox {
                    tokens: root.tokens
                    objectName: "settingsNetworkEnabled"
                    text: qsTr("Enable online metadata")
                    checked: root.librarySession.metadataConfiguration.networkEnabled || false
                    enabled: root.store !== null && !root.store.busy
                    onToggled: root.store.setMetadataConfiguration({
                                                                       networkEnabled: checked
                                                                   })
                }
                PanelDescription {
                    tokens: root.tokens
                    text: qsTr(
                              "Queries send album titles, artist names, and public identifiers to MusicBrainz, Wikipedia, or Wikidata. Music files and local paths stay on this device. Automatic queries are controlled separately.")
                }
                ArchiveCheckBox {
                    id: proxyEnabled
                    tokens: root.tokens
                    objectName: "settingsProxyEnabled"
                    text: qsTr("Use a proxy for metadata queries")
                    checked: root.librarySession.metadataConfiguration.proxyEnabled ?? true
                    enabled: root.store !== null && !root.store.busy
                    onToggled: root.store.setMetadataConfiguration({
                                                                       proxyEnabled: checked
                                                                   })
                }
                SettingChoice {
                    tokens: root.tokens
                    objectName: "settingsProxyMode"
                    label: qsTr("Proxy source")
                    choices: [
                        {
                            label: qsTr("System default proxy"),
                            value: "system"
                        },
                        {
                            label: qsTr("Manual proxy"),
                            value: "manual"
                        }
                    ]
                    selected: root.librarySession.metadataConfiguration.proxyMode || "system"
                    enabled: proxyEnabled.checked && root.store !== null && !root.store.busy
                    onChosen: value => root.store.setMetadataConfiguration({
                                                                               proxyMode: value
                                                                           })
                }
                ArchiveTextField {
                    id: proxy
                    tokens: root.tokens
                    objectName: "settingsMetadataProxy"
                    width: parent.width
                    text: root.librarySession.metadataConfiguration.proxyUrl || ""
                    visible: (root.librarySession.metadataConfiguration.proxyMode || "system")
                             === "manual"
                    enabled: proxyEnabled.checked
                    placeholderText: qsTr("Manual HTTP proxy URL")
                    Accessible.name: qsTr("Metadata HTTP proxy")
                }
                ArchiveTextField {
                    id: contact
                    tokens: root.tokens
                    objectName: "settingsMetadataContact"
                    width: parent.width
                    text: root.librarySession.metadataConfiguration.musicBrainzContact || ""
                    placeholderText: qsTr("Optional contact email or public project URL")
                    Accessible.name: qsTr("MusicBrainz contact email or project URL")
                }
                ArchiveCheckBox {
                    id: onlineEnabled
                    tokens: root.tokens
                    text: qsTr("Automatically query MusicBrainz for new albums after scanning")
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
                                                                       contact.text,
                                                                       proxyUrl: proxy.text
                                                                   })
                }
                PanelDescription {
                    tokens: root.tokens
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
