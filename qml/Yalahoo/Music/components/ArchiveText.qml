pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property DesignTokens tokens
    property string text: ""
    property font font
    property color color: tokens.ink
    property real cssPixelSize: 10
    readonly property real fontScale: cssPixelSize / Math.max(1, Math.round(cssPixelSize))
    readonly property real cssAscent: metrics.ascent * fontScale
    readonly property real cssDescent: metrics.descent * fontScale
    property real cssLineHeight: Math.round(cssAscent) + Math.round(cssDescent)
    property real topPadding: (cssLineHeight - Math.round(cssAscent) - Math.round(cssDescent)) / 2
                              + Math.round(cssAscent) - cssAscent
    property real bottomPadding: 0
    property int wrapMode: Text.NoWrap
    property int elide: Text.ElideNone
    property int verticalAlignment: Text.AlignTop
    property int horizontalAlignment: Text.AlignLeft
    property int textFormat: Text.PlainText
    property int renderType: Text.CurveRendering
    readonly property int lineCount: glyphs.lineCount
    font.family: tokens.regular.family
    font.styleName: tokens.regular.styleName
    font.weight: tokens.regular.weight
    font.pixelSize: Math.max(1, Math.round(cssPixelSize))
    font.hintingPreference: Font.PreferNoHinting
    anchors.alignWhenCentered: false
    implicitWidth: glyphs.implicitWidth * fontScale
    implicitHeight: lineCount * cssLineHeight + bottomPadding
    height: implicitHeight
    baselineOffset: glyphs.baselineOffset * fontScale
    FontMetrics {
        id: metrics
        font: glyphs.font
    }
    Text {
        id: glyphs
        antialiasing: true
        // Qt Quick rounds font sizes to pixels. A local vector transform keeps
        // the CSS fractional size without inflating font caches or adding a layer.
        width: root.width / root.fontScale
        height: root.height / root.fontScale
        transform: Scale {
            xScale: root.fontScale
            yScale: root.fontScale
        }
        text: root.text
        color: root.color
        textFormat: root.textFormat
        renderType: root.renderType
        wrapMode: root.wrapMode
        elide: root.elide
        verticalAlignment: root.verticalAlignment
        horizontalAlignment: root.horizontalAlignment
        font.family: root.font.family
        font.styleName: root.font.styleName
        font.weight: root.font.weight
        font.pixelSize: root.font.pixelSize
        font.letterSpacing: root.font.letterSpacing / root.fontScale
        font.hintingPreference: Font.PreferNoHinting
        lineHeightMode: Text.FixedHeight
        lineHeight: root.cssLineHeight / root.fontScale
        topPadding: root.topPadding / root.fontScale
        bottomPadding: root.bottomPadding / root.fontScale
    }
}
