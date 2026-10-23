// Displays one reusable track row; selection and playback state are injected by the list.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: row
    required property DesignTokens tokens
    required property DetailModel detailModel
    property TrackFocusController trackFocus: null
    property string playingTrackId: ""
    required property real viewportWidth
    required property real contentWidth
    required property real columnGap
    required property real numberWidth
    required property real formatWidth
    required property real durationWidth
    readonly property real textSize: 11
    signal trackRequested(string trackId)

    objectName: "trackDelegate"
    required property int index
    readonly property string albumId: detailModel.album.id || ""
    required property string trackId
    required property string number
    required property string trackTitle
    required property string artist
    required property string format
    required property string duration
    required property int disc
    readonly property real trackTop: discHeading ? discLabel.implicitHeight + 28 : 0
    readonly property real trackHeight: height - trackTop - 1
    required property bool discHeading
    activeFocusOnTab: true
    Keys.onReturnPressed: if (!row.detailModel.album.offline)
                              row.trackRequested(trackId)
    Keys.onSpacePressed: if (!row.detailModel.album.offline)
                             row.trackRequested(trackId)
    Accessible.role: Accessible.ListItem
    Accessible.name: number + " " + trackTitle + " " + duration
    width: row.contentWidth
    height: names.implicitHeight + (row.viewportWidth >= 1700 ? 28 : 22) + 1 + (discHeading
                                                                                ? discLabel.implicitHeight
                                                                                  + 28 : 0)
    ArchiveText {
        id: discLabel
        tokens: row.tokens
        visible: row.discHeading
        text: qsTranslate("TrackList", "DISC %1").arg(row.disc.toString().padStart(2, "0"))
        cssPixelSize: 8
        color: row.tokens.muted
        y: 20
    }
    Rectangle {
        y: row.discHeading ? discLabel.implicitHeight + 28 : 0
        width: parent.width
        height: parent.height - y - 1
        color: row.trackId === (row.trackFocus ? row.trackFocus.trackId : "") ? Qt.rgba(
                                                                                    row.tokens.accent.r,
                                                                                    row.tokens.accent.g,
                                                                                    row.tokens.accent.b,
                                                                                    .2 * row.trackFocus.highlight) :
                                                                                hit.hovered
                                                                                ? row.tokens.veil :
                                                                                  "transparent"
        Rectangle {
            antialiasing: true
            width: 3
            height: parent.height
            color: row.tokens.accent
            opacity: row.trackFocus && row.trackId === row.trackFocus.trackId
                     ? row.trackFocus.highlight : 0
        }
        ArchiveText {
            tokens: row.tokens
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            text: row.number
            cssPixelSize: row.viewportWidth >= 1700 ? row.textSize : 9
            color: row.tokens.muted
        }
        Column {
            id: names
            x: row.numberWidth + row.columnGap
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.width - x - row.formatWidth - row.durationWidth - row.columnGap
                            * 2)
            spacing: 5
            ArchiveText {
                id: trackTitleText
                tokens: row.tokens
                width: parent.width
                objectName: "detailTrackTitle"
                text: row.trackTitle
                cssPixelSize: row.viewportWidth >= 1700 ? 14 : row.viewportWidth <= 1100
                                                          && row.viewportWidth > 700 ? 10 :
                                                                                       row.textSize
                cssLineHeight: row.detailModel.normalLineHeight(row.trackTitle, font) * fontScale
                elide: Text.ElideRight
                color: row.trackId === row.playingTrackId ? row.tokens.accent : row.tokens.ink
            }
            ArchiveText {
                tokens: row.tokens
                width: parent.width
                objectName: "detailTrackArtist"
                text: row.artist
                cssPixelSize: row.viewportWidth >= 1700 ? 10 : 8
                color: row.tokens.muted
                elide: Text.ElideRight
            }
        }
        ArchiveText {
            tokens: row.tokens
            x: parent.width - row.durationWidth - row.columnGap - row.formatWidth
            width: row.formatWidth
            anchors.verticalCenter: parent.verticalCenter
            text: row.format
            cssPixelSize: 7
            horizontalAlignment: Text.AlignRight
            color: row.tokens.muted
            elide: Text.ElideRight
        }
        ArchiveText {
            tokens: row.tokens
            x: parent.width - row.durationWidth
            width: row.durationWidth
            anchors.verticalCenter: parent.verticalCenter
            text: row.duration
            cssPixelSize: row.viewportWidth >= 1700 ? row.textSize : 9
            horizontalAlignment: Text.AlignRight
            color: row.tokens.muted
        }
        Rectangle {
            antialiasing: true
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -1
            width: parent.width
            height: 1
            color: row.tokens.line
        }
        HoverHandler {
            id: hit
        }
        TapHandler {
            property string pressedAlbumId: ""
            property string pressedTrackId: ""
            enabled: !row.detailModel.album.offline
            onPressedChanged: {
                if (pressed) {
                    pressedAlbumId = row.albumId;
                    pressedTrackId = row.trackId;
                }
            }
            onTapped: {
                if (pressedAlbumId === row.albumId && pressedTrackId === row.trackId)
                    row.trackRequested(row.trackId);
            }
        }
        Accessible.role: Accessible.ListItem
        Accessible.name: row.number + " " + row.trackTitle + " " + row.duration
    }
}
