// Displays one reusable search hit and validates identity across a pointer press.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

T.Button {
    id: result
    required property DesignTokens tokens
    required property bool searching
    required property bool currentResult
    signal resultRequested(string albumId, string trackId)

    objectName: "librarySearchResult-" + index
    enabled: !result.searching
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    required property int index
    required property string kind
    required property string albumId
    required property string trackId
    required property string title
    required property string artist
    required property string subtitle
    required property url coverUrl
    property bool pendingPress: false
    property string pressedAlbumId: ""
    property string pressedTrackId: ""
    height: Math.max(72, copy.implicitHeight + 22)
    padding: 0
    background: Rectangle {
        color: result.hovered || result.activeFocus || result.currentResult ? result.tokens.veil :
                                                                              "transparent"

        Rectangle {
            antialiasing: true
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: result.tokens.line
        }
    }
    contentItem: Item {
        Rectangle {
            antialiasing: true
            width: 49
            height: 49
            anchors.verticalCenter: parent.verticalCenter
            color: result.tokens.surface
            border.color: result.tokens.line
            Image {
                id: artwork
                smooth: true
                mipmap: true
                anchors.fill: parent
                anchors.margins: 1
                source: result.coverUrl
                sourceSize: Qt.size(Math.round(width * (Window.window
                                                        ? Window.window.devicePixelRatio : 1)),
                                    Math.round(height * (Window.window
                                                         ? Window.window.devicePixelRatio : 1)))
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            ArchiveText {
                anchors.centerIn: parent
                tokens: result.tokens
                visible: result.coverUrl.toString().length === 0 || artwork.status === Image.Error

                text: "\u266a"
                cssPixelSize: 16
            }
        }
        Column {
            id: copy
            x: 62
            width: Math.max(0, parent.width - x - 62)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            ArchiveText {
                width: parent.width
                tokens: result.tokens
                text: result.title
                cssPixelSize: 12
                wrapMode: Text.WrapAnywhere
            }
            ArchiveText {
                width: parent.width
                tokens: result.tokens
                text: result.artist + " \u00b7 " + result.subtitle
                cssPixelSize: 10
                color: result.tokens.muted
                wrapMode: Text.WrapAnywhere
            }
        }
        ArchiveText {
            x: parent.width - 49
            anchors.verticalCenter: parent.verticalCenter
            tokens: result.tokens
            text: result.kind === "track" ? qsTranslate("SearchPanel", "Track") : qsTranslate(
                                                "SearchPanel", "Albums")
            cssPixelSize: 10
            color: result.tokens.muted
        }
        ArchiveText {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            tokens: result.tokens
            text: "\u2197"
            cssPixelSize: 17
        }
    }
    onPressed: {
        pendingPress = true;
        pressedAlbumId = albumId;
        pressedTrackId = trackId;
    }
    onCanceled: pendingPress = false
    onClicked: {
        const unchanged = !pendingPress || (pressedAlbumId === albumId && pressedTrackId
                                            === trackId);
        pendingPress = false;
        if (unchanged)
            result.resultRequested(albumId, trackId);
    }
    Accessible.name: (kind === "track" ? qsTranslate("SearchPanel", "Locate track %1, %2") :
                                         qsTranslate("SearchPanel", "Locate album %1, %2")).arg(
                         title).arg(subtitle)
}
