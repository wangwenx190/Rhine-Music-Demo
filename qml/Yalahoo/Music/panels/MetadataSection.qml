pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Column {
    id: root
    required property DesignTokens tokens
    required property LibrarySession librarySession
    required property string kind
    required property string title
    required property string explanation
    readonly property var result: librarySession.metadataFor(librarySession.navigationAlbumId, kind,
                                                             librarySession.metadataRevision)
    readonly property var progress: librarySession.metadataProgress
    readonly property bool queryEnabled: librarySession.providerCapabilities.networkEnabled
                                         && librarySession.providerCapabilities[kind
                                                                                === "introduction"
                                                                                ? "introductions" :
                                                                                  kind]
    width: parent ? parent.width : 300
    spacing: 12
    ArchiveText {
        tokens: root.tokens
        text: root.title
        cssPixelSize: 12
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: root.explanation
        cssPixelSize: 12
        color: root.tokens.muted
        wrapMode: Text.Wrap
    }
    ArchiveText {
        objectName: root.kind + "MetadataState"
        tokens: root.tokens
        width: parent.width
        text: root.result.stateText || qsTr("Select an album to view local metadata.")
        cssPixelSize: 12
        wrapMode: Text.WrapAnywhere
        Accessible.role: Accessible.StaticText
    }
    ArchiveText {
        objectName: root.kind + "MetadataContent"
        tokens: root.tokens
        width: parent.width
        text: root.result.contentText || ""
        cssPixelSize: 12
        wrapMode: Text.WrapAnywhere
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: root.result.sourceText || qsTr("Source: ")
        cssPixelSize: 11
        color: root.tokens.muted
        wrapMode: Text.WrapAnywhere
    }
    ArchiveButton {
        id: queryButton
        objectName: root.kind + "MetadataQuery"
        tokens: root.tokens
        text: root.kind === "introduction" ? qsTr("Query / update album introduction \u2197") :
                                             root.kind === "credits" ? qsTr(
                                                                           "Query / update credits \u2197") :
                                                                       qsTr("Query / update MusicBrainz metadata \u2197")
        enabled: root.queryEnabled && !root.progress.active && !root.librarySession.metadataBusy
                 && root.librarySession.navigationAlbumId !== ""
        onClicked: root.librarySession.requestMetadata(root.librarySession.navigationAlbumId,
                                                       root.kind, true)
        ArchiveToolTip {
            parent: queryButton
            tokens: root.tokens
            visible: queryButton.hovered && !root.queryEnabled
            text: qsTr("Enable online metadata in advanced settings")
        }
        Accessible.description: root.queryEnabled ? "" : qsTr(
                                                        "Enable online metadata in advanced settings")
    }
    ArchiveText {
        tokens: root.tokens
        text: qsTr("Online queries are disabled. Existing metadata remains available offline.")
        width: parent.width
        wrapMode: Text.Wrap
        visible: !root.queryEnabled
        cssPixelSize: 11
        color: root.tokens.muted
    }
    ArchiveButton {
        objectName: root.kind + "MetadataBatch"
        tokens: root.tokens
        text: qsTr("Query missing or expired metadata for the library")
        enabled: root.queryEnabled && !root.progress.active && !root.librarySession.metadataBusy
        onClicked: root.librarySession.requestMetadataBatch(root.kind)
    }
    Column {
        width: parent.width
        spacing: 12
        visible: (root.result.releaseCandidates || []).length > 0
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            wrapMode: Text.Wrap
            cssPixelSize: 11
            text: qsTr(
                      "Several releases may match. Confirm the edition for this album before querying credits or genres.")
        }
        ArchiveComboBox {
            id: releaseChoice
            objectName: root.kind + "MetadataRelease"
            tokens: root.tokens
            width: parent.width
            model: root.result.releaseCandidates || []
            textRole: "display"
            valueRole: "id"
            Accessible.name: qsTr("MusicBrainz release edition")
        }
        ArchiveButton {
            tokens: root.tokens
            text: qsTr("Query the selected release")
            enabled: root.queryEnabled && !root.progress.active &&
                     !root.librarySession.metadataBusy && releaseChoice.currentIndex >= 0
            onClicked: root.librarySession.requestMetadata(root.librarySession.navigationAlbumId,
                                                           root.kind, true,
                                                           releaseChoice.currentValue)
        }
    }
    ArchiveProgressBar {
        tokens: root.tokens
        implicitHeight: root.kind === "introduction" ? 3 : 16
        width: parent.width
        visible: root.progress.kind === root.kind && root.progress.total > 0
        from: 0
        to: root.progress.total || 1
        value: root.progress.completed || 0
        Accessible.name: qsTr("%1 query progress").arg(root.title)
    }
    ArchiveText {
        tokens: root.tokens
        visible: root.progress.kind === root.kind && root.progress.total > 0
        text: qsTr("%1 / %2 \u00b7 %3 \u00b7 Errors %4").arg(root.progress.completed || 0).arg(
                  root.progress.total || 0).arg(root.progress.statusText || "").arg(
                  root.progress.errors || 0)
        cssPixelSize: 11
    }
    ArchiveButton {
        tokens: root.tokens
        text: qsTr("Cancel query")
        visible: root.progress.active || root.librarySession.metadataBusy
        onClicked: root.librarySession.cancelAllMetadata()
    }
}
