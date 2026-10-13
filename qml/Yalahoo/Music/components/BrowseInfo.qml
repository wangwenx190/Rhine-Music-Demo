pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music

Flickable {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property var album: ({})
    property string ordinal: "001"
    property real calloutBottom: metrics.portrait ? 135 : 165
    signal openRequested
    readonly property real portraitHeight: Math.min(Math.max(210, metrics.height / 3),
                                                    metrics.height - calloutBottom - 150)
    readonly property real titleSize: metrics.width <= 700 ? 25 : metrics.width <= 1100 ? 28 : Math.max(
                                                                                              26, Math.min(
                                                                                                  44, metrics.width
                                                                                                  * 0.027))
    objectName: "browseInfo"
    x: metrics.informationX
    y: metrics.portrait ? metrics.height - calloutBottom - portraitHeight : metrics.height <= 650 && metrics.width
                          > 700 ? 125 : metrics.width >= 1700 ? metrics.height * 0.32 :
                                                                metrics.width <= 1100 ? Math.max(132,
                                                                                                 Math.min(240,
                                                                                                          metrics.height
                                                                                                          * 0.28)) :
                                                                                        Math.max(155,
                                                                                                 Math.min(330,
                                                                                                          metrics.height
                                                                                                          * 0.3))
    width: metrics.width - x - metrics.informationRight
    height: Math.max(0, metrics.portrait ? portraitHeight : metrics.height - calloutBottom - y)
    contentWidth: width
    contentHeight: content.height + (metrics.portrait ? 20 : 0)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    ScrollBar.vertical: ScrollBar {
        width: 3
        background: Item {}
        contentItem: Rectangle {
            color: root.tokens.muted
            radius: 2
            opacity: root.contentHeight > root.height ? 0.5 : 0
        }
    }
    Column {
        id: content
        x: root.metrics.portrait ? 12 : 0
        y: root.metrics.portrait ? 10 : 0
        width: root.width - (root.metrics.portrait ? 24 : 0)
        Row {
            id: eyebrow
            objectName: "browseEyebrow"
            readonly property real size: root.metrics.width >= 1700 ? 11 : root.metrics.width <= 700 ? 7 :
                                                                                                       9
            height: size * 1.7
            FontMetrics {
                id: eyebrowMetrics
                font: eyebrowPrefix.font
            }
            ArchiveText {
                id: eyebrowPrefix
                tokens: root.tokens
                text: "MUSIC ARCHIVE"
                cssPixelSize: eyebrow.size
                cssLineHeight: cssPixelSize * 1.7
                font.letterSpacing: 1.5
            }
            Item {
                width: eyebrowMetrics.advanceWidth(" ") + 4
                height: 1
            }
            ArchiveText {
                tokens: root.tokens
                text: "／"
                cssPixelSize: eyebrow.size
                cssLineHeight: cssPixelSize * 1.7
                font.letterSpacing: 1.5
            }
            Item {
                width: eyebrowMetrics.advanceWidth(" ") + 8
                height: 1
            }
            ArchiveText {
                tokens: root.tokens
                text: root.album.genre || "未分类"
                cssPixelSize: eyebrow.size
                cssLineHeight: cssPixelSize * 1.7
                font.letterSpacing: 1.5
            }
        }
        Item {
            id: rule
            readonly property real ruleTop: root.metrics.height <= 650 && root.metrics.width > 700
                                            ? 7 : root.metrics.width <= 700 ? 8 : 15
            readonly property real ruleBottom: root.metrics.height <= 650 && root.metrics.width
                                               > 700 ? 7 : root.metrics.width <= 700 ? 8 : 12
            readonly property real ruleMargin: root.metrics.height <= 650 && root.metrics.width
                                               > 700 ? 10 : root.metrics.width <= 700 ? 11 : 17
            width: parent.width
            height: ruleTop + codeLabel.height + ruleBottom + 1 + ruleMargin
            ArchiveText {
                id: codeLabel
                tokens: root.tokens
                y: rule.ruleTop
                text: "ALBUM " + root.ordinal
                cssPixelSize: root.metrics.width >= 1700 ? 12 : root.metrics.width <= 700 ? 8 : 10
                font.letterSpacing: 1
            }
            Rectangle {
                y: parent.height - rule.ruleMargin - 1
                width: parent.width
                height: 1
                color: root.tokens.ink
            }
        }
        ArchiveText {
            objectName: "albumTitle"
            tokens: root.tokens
            width: parent.width
            text: root.album.title || "让音乐进入这座档案馆。"
            font.family: root.tokens.bold.family
            font.styleName: root.tokens.bold.styleName
            font.weight: root.tokens.bold.weight
            cssPixelSize: root.titleSize
            font.letterSpacing: -0.6
            wrapMode: Text.WrapAnywhere
            cssLineHeight: root.titleSize * 1.4
            bottomPadding: root.metrics.width <= 700 ? 9 : 13
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            objectName: "albumArtist"
            text: root.album.artist || ""
            font.family: root.tokens.demibold.family
            font.styleName: root.tokens.demibold.styleName
            font.weight: root.tokens.demibold.weight
            cssPixelSize: root.metrics.width >= 1700 ? 19 : root.metrics.width <= 700 ? 12 :
                                                                                        root.metrics.width
                                                                                        <= 1100 ? 13 :
                                                                                                  15
            wrapMode: Text.Wrap
            cssLineHeight: cssPixelSize * 1.6
            bottomPadding: root.metrics.height <= 650 || root.metrics.width <= 700 ? 10 : 21
        }
        ArchiveText {
            tokens: root.tokens
            objectName: "albumMeta"
            text: (root.album.year || "") + (root.album.year ? "  ·  " : "") + (root.album.tracks
                                                                                || 0) + " TRACKS"
            color: root.tokens.muted
            cssPixelSize: root.metrics.width >= 1700 ? 12 : root.metrics.width <= 700 ? 9 : 10
            font.letterSpacing: 0.5
        }
        Item {
            width: 1
            height: root.metrics.height <= 650 ? 12 : root.metrics.width <= 700 ? 13 : 22
        }
        ArchiveButton {
            objectName: "openAlbumButton"
            tokens: root.tokens
            width: parent.width
            height: root.metrics.height <= 650 && root.metrics.width > 700 ? 41 : root.metrics.width
                                                                             <= 700 ? 42 : 49
            topPadding: root.metrics.height <= 650 && root.metrics.width > 700 ? 8 : 10
            bottomPadding: root.metrics.height <= 650 && root.metrics.width > 700 ? 9 :
                                                                                    root.metrics.width
                                                                                    <= 700 ? 8 : 15
            text: "打开专辑"
            cssPixelSize: 16
            underline: true
            contentItem: Item {
                ArchiveText {
                    tokens: root.tokens
                    objectName: "openAlbumCaption"
                    text: "打开专辑"
                    anchors.verticalCenter: parent.verticalCenter
                    cssPixelSize: 16
                }
                ArchiveText {
                    tokens: root.tokens
                    text: "↗"
                    cssPixelSize: 24
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            onClicked: root.openRequested()
        }
    }
}
