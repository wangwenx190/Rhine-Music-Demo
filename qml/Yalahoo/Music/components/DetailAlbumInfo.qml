// Lays out the album heading, navigation, artist and fixed fact grid.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Column {
    id: root
    required property DesignTokens tokens
    required property PresentationController presentation
    required property var album
    required property real viewportWidth
    readonly property real wideWidthPixels: 1700
    readonly property real titleFontWidthRatio: 0.0245

    Item {
        id: overline
        width: parent.width
        readonly property real contentHeight: Math.max(ordinal.implicitHeight,
                                                       navigation.implicitHeight)
        height: contentHeight + (root.viewportWidth <= 700 ? 12 : 18)
        ArchiveText {
            id: ordinal
            objectName: "detailOrdinal"
            y: (overline.contentHeight - height) / 2
            tokens: root.tokens
            text: qsTranslate("Detail", "ALBUM %1").arg((root.album.code || 1).toString().padStart(3,
                                                                                                   "0"))
            cssPixelSize: root.viewportWidth <= 700 ? 7 : 8
            font.letterSpacing: 1.6
        }
        ArchiveText {
            id: navigationMeasure
            tokens: root.tokens
            visible: false
            text: qsTranslate("Detail", "Next album \u2193")
            cssPixelSize: 9
        }
        Row {
            id: navigation
            anchors.right: parent.right
            y: (overline.contentHeight - height) / 2
            spacing: 20
            ArchiveButton {
                objectName: "detailPrevious"
                tokens: root.tokens
                text: qsTranslate("Detail", "\u2191 Previous album")
                cssPixelSize: 9
                height: navigationMeasure.implicitHeight
                enabled: root.presentation.canMovePrevious
                onClicked: root.presentation.stepAlbum(-1)
            }
            ArchiveButton {
                objectName: "detailNext"
                tokens: root.tokens
                text: qsTranslate("Detail", "Next album \u2193")
                cssPixelSize: 9
                height: navigationMeasure.implicitHeight
                enabled: root.presentation.canMoveNext
                onClicked: root.presentation.stepAlbum(1)
            }
        }
    }
    Column {
        width: parent.width
        bottomPadding: 10
        spacing: title.cssPixelSize * 0.5 * 0.48
        ArchiveText {
            id: title
            objectName: "detailTitle"
            tokens: root.tokens
            width: parent.width
            text: root.album.mainTitle || root.album.title || ""
            cssPixelSize: Math.min(39, Math.max(24, root.viewportWidth * root.titleFontWidthRatio))
            cssLineHeight: cssPixelSize * 1.22
            font.styleName: root.tokens.demibold.styleName
            font.weight: root.tokens.demibold.weight
            font.letterSpacing: -0.6
            wrapMode: Text.WrapAnywhere
            Accessible.name: root.album.title || ""
        }
        ArchiveText {
            objectName: "detailTranslation"
            tokens: root.tokens
            width: parent.width
            visible: text.length > 0
            text: root.album.translation || ""
            cssPixelSize: title.cssPixelSize * 0.5
            cssLineHeight: cssPixelSize * 1.45
            font.styleName: root.tokens.demibold.styleName
            font.weight: root.tokens.demibold.weight
            font.letterSpacing: cssPixelSize * 0.01
            color: root.tokens.muted
            wrapMode: Text.WrapAnywhere
        }
    }
    Row {
        id: artistRow
        objectName: "detailArtist"
        readonly property real contentHeight: Math.max(artistLine.height, offline.visible
                                                       ? offline.height : 0)
        width: parent.width
        spacing: 12
        bottomPadding: root.viewportWidth <= 700 ? 16 : 22
        ArchiveText {
            id: artistLine
            tokens: root.tokens
            y: (artistRow.contentHeight - height) / 2
            text: root.album.artist || ""
            width: Math.min(implicitWidth, parent.width - (offline.visible ? offline.width + 12 :
                                                                             0))
            cssPixelSize: root.viewportWidth <= 700 ? 12 : root.viewportWidth
                                                      >= root.wideWidthPixels ? 17 : 14
            wrapMode: Text.Wrap
        }
        Rectangle {
            id: offline
            antialiasing: true
            objectName: "detailOfflineBadge"
            visible: root.album.offline || false
            width: badge.implicitWidth + 14
            height: badge.implicitHeight + 8
            color: "transparent"
            border.color: root.tokens.line
            y: (artistRow.contentHeight - height) / 2
            ArchiveText {
                id: badge
                anchors.centerIn: parent
                tokens: root.tokens
                text: qsTranslate("Detail", "Directory offline")
                cssPixelSize: 10
                color: root.tokens.accent
            }
        }
    }
    Rectangle {
        antialiasing: true
        width: parent.width
        height: 1
        color: root.tokens.ink
    }
    Grid {
        id: facts
        objectName: "detailFacts"
        width: parent.width
        columns: 2
        columnSpacing: root.viewportWidth <= 700 ? 16 : root.viewportWidth <= 1100 ? 14 : 18
        rowSpacing: root.viewportWidth <= 700 ? 11 : root.viewportWidth <= 1100 ? 12 : 14
        topPadding: root.viewportWidth <= 700 ? 15 : 19
        bottomPadding: root.viewportWidth <= 700 ? 15 : 19
        Repeater {
            model: root.album.facts || []
            delegate: Column {
                required property var modelData
                width: (facts.width - facts.columnSpacing) / 2
                spacing: 5
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: parent.modelData.label
                    objectName: "detailFactLabel"
                    cssPixelSize: root.viewportWidth >= root.wideWidthPixels ? 9 :
                                                                               root.viewportWidth
                                                                               <= 700 ? 6 : 7
                    font.letterSpacing: 0.6
                    color: root.tokens.muted
                    wrapMode: Text.Wrap
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: parent.modelData.value
                    objectName: "detailFactValue"
                    cssPixelSize: root.viewportWidth >= root.wideWidthPixels ? 13 :
                                                                               root.viewportWidth
                                                                               <= 1100 ? 10 : 11
                    cssLineHeight: cssPixelSize * 1.5
                    wrapMode: Text.WrapAnywhere
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
}
