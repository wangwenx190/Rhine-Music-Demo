pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property string ordinal: "01"
    property int albumCount: 0
    readonly property real bottomGap: metrics.width <= 700 ? 65 : metrics.width <= 1100 ? 57 :
                                                                                          metrics.width
                                                                                          >= 1700 ? 90 :
                                                                                                    60
    readonly property real labelSize: metrics.width <= 700 ? 6 : metrics.width >= 1700 ? 11 : 9
    readonly property real numberSize: metrics.width <= 700 ? 34 : metrics.width <= 1100 ? 42 : metrics.width
                                                                                           >= 1700 ? 65 :
                                                                                                     50
    readonly property real labelRowAscent: Math.max(Math.round(strut.ascent), Math.round(
                                                        labelMetrics.ascent) + (labelSize * 1.7
                                                                                - Math.round(
                                                                                    labelMetrics.height))
                                                    / 2)
    readonly property real labelRowDescent: Math.max(Math.round(strut.descent), Math.round(
                                                         labelMetrics.descent) + (labelSize * 1.7
                                                                                  - Math.round(
                                                                                      labelMetrics.height))
                                                     / 2)
    readonly property real labelRowHeight: labelRowAscent + labelRowDescent
    objectName: "navigationFrame"
    x: metrics.width <= 700 ? 23 : metrics.width <= 1100 ? 27 : metrics.width >= 1700 ? 62 : 42
    width: metrics.width - x - metrics.width * 0.062
    height: labelRowHeight + 6 + Math.max(numberSize, total.y + total.height)
    y: metrics.height - bottomGap - height
    FontMetrics {
        id: strut
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: 16
    }
    FontMetrics {
        id: labelMetrics
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: root.labelSize
    }
    ArchiveText {
        tokens: root.tokens
        text: "ALBUM / SELECT"
        cssPixelSize: root.labelSize
        font.letterSpacing: root.metrics.width <= 700 ? 1 : 1.5
        y: root.labelRowAscent - baselineOffset
        height: root.labelSize * 1.7
    }
    Item {
        y: root.labelRowHeight + 6
        ArchiveText {
            id: number
            objectName: "staticOrdinal"
            tokens: root.tokens
            text: root.ordinal
            font.family: MusicFonts.light.family
            font.styleName: MusicFonts.light.styleName
            font.weight: MusicFonts.light.weight
            cssPixelSize: root.numberSize
            font.letterSpacing: -3
            cssLineHeight: root.numberSize
        }
        ArchiveText {
            id: total
            objectName: "staticTotal"
            tokens: root.tokens
            x: number.width + 8
            y: number.y + number.baselineOffset - baselineOffset
            text: "/  " + root.albumCount.toString().padStart(2, "0")
            font.family: MusicFonts.light.family
            font.styleName: MusicFonts.light.styleName
            font.weight: MusicFonts.light.weight
            cssPixelSize: root.metrics.width <= 700 ? 9 : 12
            color: root.tokens.muted
        }
    }
}
