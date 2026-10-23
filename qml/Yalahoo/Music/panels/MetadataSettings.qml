pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

SettingsSection {
    id: root
    objectName: "metadataSettings"
    required property LibrarySession librarySession
    property LibraryStore store: null
    property bool expanded: false

    ArchiveButton {
        objectName: "settingsMusicBrainzExpand"
        tokens: root.tokens
        text: (root.expanded ? "\u25be " : "\u25b8 ") + qsTranslate("SettingsPanel",
                                                                    "Advanced \u00b7 Online metadata")
        onClicked: root.expanded = !root.expanded
    }
    Column {
        width: parent.width
        spacing: 16
        visible: root.expanded
        MetadataSection {
            tokens: root.tokens
            librarySession: root.librarySession
            kind: "enrichment"
            title: "MusicBrainz"
            explanation: qsTranslate("SettingsPanel",
                                     "MusicBrainz provides release identifiers and genres. Local titles and manual classification are preserved.")
        }
        ArchiveCheckBox {
            tokens: root.tokens
            objectName: "settingsNetworkEnabled"
            text: qsTranslate("SettingsPanel", "Enable online metadata")
            checked: root.librarySession.metadataConfiguration.networkEnabled || false
            enabled: root.store !== null && !root.store.busy
            onToggled: root.store.setMetadataConfiguration({
                                                               networkEnabled: checked
                                                           })
        }
        PanelDescription {
            tokens: root.tokens
            text: qsTranslate("SettingsPanel",
                              "Queries send album titles, artist names, and public identifiers to MusicBrainz, Wikipedia, or Wikidata. Music files and local paths stay on this device. Automatic queries are controlled separately.")
        }
        ArchiveCheckBox {
            id: proxyEnabled
            tokens: root.tokens
            objectName: "settingsProxyEnabled"
            text: qsTranslate("SettingsPanel", "Use a proxy for metadata queries")
            checked: root.librarySession.metadataConfiguration.proxyEnabled ?? true
            enabled: root.store !== null && !root.store.busy
            onToggled: root.store.setMetadataConfiguration({
                                                               proxyEnabled: checked
                                                           })
        }
        SettingChoice {
            tokens: root.tokens
            objectName: "settingsProxyMode"
            label: qsTranslate("SettingsPanel", "Proxy source")
            choices: [
                {
                    label: qsTranslate("SettingsPanel", "System default proxy"),
                    value: "system"
                },
                {
                    label: qsTranslate("SettingsPanel", "Manual proxy"),
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
            property bool dirty: false
            onTextEdited: proxy.dirty = true
            Binding on text {
                when: !proxy.dirty
                value: root.librarySession.metadataConfiguration.proxyUrl || ""
                restoreMode: Binding.RestoreNone
            }
            visible: (root.librarySession.metadataConfiguration.proxyMode || "system") === "manual"
            enabled: proxyEnabled.checked
            placeholderText: qsTranslate("SettingsPanel", "Manual HTTP proxy URL")
            Accessible.name: qsTranslate("SettingsPanel", "Metadata HTTP proxy")
        }
        ArchiveTextField {
            id: contact
            tokens: root.tokens
            objectName: "settingsMetadataContact"
            width: parent.width
            property bool dirty: false
            onTextEdited: contact.dirty = true
            Binding on text {
                when: !contact.dirty
                value: root.librarySession.metadataConfiguration.musicBrainzContact || ""
                restoreMode: Binding.RestoreNone
            }
            placeholderText: qsTranslate("SettingsPanel",
                                         "Optional contact email or public project URL")
            Accessible.name: qsTranslate("SettingsPanel",
                                         "MusicBrainz contact email or project URL")
        }
        ArchiveCheckBox {
            id: onlineEnabled
            tokens: root.tokens
            text: qsTranslate("SettingsPanel",
                              "Automatically query MusicBrainz for new albums after scanning")
            checked: root.librarySession.metadataConfiguration.onlineEnabled || false
            enabled: root.librarySession.providerCapabilities.networkEnabled && root.store !== null
                     && !root.store.busy
            onToggled: root.store.setMetadataConfiguration({
                                                               onlineEnabled: checked
                                                           })
        }
        ArchiveButton {
            tokens: root.tokens
            objectName: "settingsMetadataSave"
            text: qsTranslate("SettingsPanel", "Save metadata settings \u2197")
            enabled: root.store !== null && !root.store.busy
            onClicked: {
                if (root.store.setMetadataConfiguration({
                                                            musicBrainzContact: contact.text,
                                                            proxyUrl: proxy.text
                                                        })) {
                    proxy.dirty = false;
                    contact.dirty = false;
                }
            }
        }
        PanelDescription {
            tokens: root.tokens
            text: root.store ? root.store.error : qsTranslate("SettingsPanel",
                                                              "Metadata configuration is saved through explicit import or the native music library.")
        }
    }
}
