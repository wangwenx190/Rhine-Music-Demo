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
                                                                           "Query missing credits \u2197") :
                                                                       qsTr("Enrich missing online metadata \u2197")
        enabled: root.queryEnabled && !root.progress.active
        onClicked: root.librarySession.requestMetadataBatch(root.kind)
        ArchiveToolTip {
            parent: queryButton
            tokens: root.tokens
            visible: queryButton.hovered && !queryButton.enabled
            text: qsTr("Online metadata is not enabled yet")
        }
        Accessible.description: enabled ? "" : qsTr("Online metadata is not enabled yet")
    }
    ArchiveText {
        tokens: root.tokens
        text: qsTr("Online metadata is not enabled yet. Existing caches remain available offline.")
        width: parent.width
        wrapMode: Text.Wrap
        visible: !root.queryEnabled
        cssPixelSize: 11
        color: root.tokens.muted
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
        visible: root.progress.kind === root.kind && root.progress.active
        onClicked: root.librarySession.cancelAllMetadata()
    }
}
