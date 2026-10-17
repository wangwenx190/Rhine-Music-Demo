pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    required property LibrarySession librarySession
    property Component heading: null
    property LibraryStore store: null
    implicitHeight: 550
    contentWidth: availableWidth
    function focusEditor(): void {
        roots.forceActiveFocus();
    }
    function loadRoots(): void {
        roots.text = store ? store.rootPaths().join("\n") : "";
    }
    FolderDialog {
        id: folder
        title: qsTr("Choose a local music folder")
        onAccepted: {
            roots.text += (roots.text.trim().length ? "\n" : "") + root.store.localPath(
                        selectedFolder);
        }
    }
    Column {
        width: root.availableWidth
        spacing: 15
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        ArchiveText {
            width: parent.width
            tokens: root.tokens
            text: qsTr(
                      "Each track in a root directory gets its own card, preferring embedded artwork. Subfolders appear as albums, preferring folder artwork.")
            cssPixelSize: 12
            cssLineHeight: 22.2
            wrapMode: Text.Wrap
            color: root.tokens.muted
        }
        ArchiveText {
            tokens: root.tokens
            text: qsTr("Music folders \u00b7 One directory per line")
            cssPixelSize: 11
        }
        ArchiveTextArea {
            id: roots
            tokens: root.tokens
            objectName: "libraryRootsEditor"
            width: parent.width
            height: implicitHeight
            font.family: root.tokens.regular.family
            font.pixelSize: 12
            color: root.tokens.ink
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            placeholderText: qsTr("Enter a local absolute path, or choose a folder")
            placeholderTextColor: root.tokens.muted
            background: Rectangle {
                antialiasing: true
                color: root.tokens.surface
                border.color: root.tokens.line
            }
            Accessible.name: qsTr("Music folders, one directory per line")
        }
        Flow {
            width: parent.width
            spacing: 20
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Choose folder \u2197")
                enabled: root.store !== null
                onClicked: folder.open()
            }
            ArchiveButton {
                objectName: "librarySaveRoots"
                tokens: root.tokens
                text: qsTr("Save folders and scan \u2197")
                enabled: root.store !== null && !root.store.busy
                onClicked: root.store.setRootPaths(roots.text)
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Rescan")
                enabled: root.store !== null && !root.store.busy
                onClicked: root.store.rescan()
            }
            ArchiveButton {
                tokens: root.tokens
                text: qsTr("Cancel")
                visible: root.store !== null && root.store.busy
                onClicked: root.store.cancel()
            }
        }
        ArchiveText {
            objectName: "libraryScanStatus"
            tokens: root.tokens
            width: parent.width
            cssPixelSize: 10
            cssLineHeight: 18
            wrapMode: Text.WrapAnywhere
            color: root.store && root.store.error ? root.tokens.accent : root.tokens.muted
            text: !root.store ? qsTr("Anonymous previews do not read local music folders.") :
                                root.store.probing ? qsTr(
                                                         "Reading track tags and artwork. You can keep browsing the existing library...") :
                                                     root.store.scanning ? qsTr(
                                                                               "Scanning. You can keep browsing the existing library...") :
                                                                           root.store.busy ? qsTr(
                                                                                                 "Reading or saving the local music index...") :
                                                                                             root.store.error
                                                                                             || root.librarySession.summary.offline
                                                                                             || (root.store.roots.length
                                                                                                 ? qsTr("The local index is ready.") :
                                                                                                   qsTr("No music folders configured."))
            Accessible.role: Accessible.StaticText
        }
        Rectangle {
            antialiasing: true
            width: parent.width
            height: 1
            color: root.tokens.line
        }
        Row {
            width: parent.width
            Repeater {
                model: [
                    {
                        value: root.librarySession.summary.albums,
                        label: qsTr("Albums")
                    },
                    {
                        value: root.librarySession.summary.tracks,
                        label: qsTr("Tracks")
                    },
                    {
                        value: root.librarySession.summary.genres,
                        label: qsTr("Genres")
                    }
                ]
                delegate: Row {
                    required property var modelData
                    width: parent.width / 3
                    spacing: 12
                    ArchiveText {
                        tokens: root.tokens
                        text: parent.modelData.value.toString()
                        cssPixelSize: 29
                    }
                    ArchiveText {
                        tokens: root.tokens
                        text: parent.modelData.label
                        cssPixelSize: 10
                        color: root.tokens.muted
                        anchors.baseline: parent.children[0].baseline
                    }
                }
            }
        }
        Rectangle {
            antialiasing: true
            width: parent.width
            height: 1
            color: root.tokens.line
        }
        ArchiveText {
            tokens: root.tokens
            text: qsTr("Cover display")
            cssPixelSize: 12
        }
        ArchiveText {
            width: parent.width
            tokens: root.tokens
            text: qsTr(
                      "Square, portrait, and landscape covers keep their original proportions and fit fully on the card. Missing artwork uses an album title placeholder, never another album's image.")
            cssPixelSize: 11
            cssLineHeight: 20.9
            color: root.tokens.muted
            wrapMode: Text.Wrap
        }
    }
    Component.onCompleted: loadRoots()
}
