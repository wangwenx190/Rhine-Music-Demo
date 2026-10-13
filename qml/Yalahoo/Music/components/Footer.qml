pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property int albumCount: 0
    objectName: "archiveFooter"
    x: metrics.width <= 700 ? 23 : metrics.width <= 1100 ? 27 : 42
    y: metrics.height - 13 - height
    width: metrics.width - x * 2
    height: leftText.height
    Row {
        ArchiveText {
            id: leftText
            tokens: root.tokens
            renderType: Text.CurveRendering
            text: "LOCAL COLLECTION "
            cssPixelSize: root.metrics.width <= 700 ? 6 : root.metrics.width <= 1100 ? 7 : 8
            font.letterSpacing: 1.1
            color: root.tokens.muted
        }
        Item {
            width: dot.implicitWidth + 16
            height: leftText.height
            ArchiveText {
                id: dot
                x: 8
                tokens: root.tokens
                renderType: Text.CurveRendering
                text: "·"
                cssPixelSize: leftText.cssPixelSize
                font.letterSpacing: 1.1
                color: root.tokens.muted
            }
        }
        ArchiveText {
            tokens: root.tokens
            renderType: Text.CurveRendering
            text: " " + root.albumCount + " ALBUMS"
            cssPixelSize: leftText.cssPixelSize
            font.letterSpacing: 1.1
            color: root.tokens.muted
        }
    }
    ArchiveText {
        tokens: root.tokens
        renderType: Text.CurveRendering
        anchors.right: parent.right
        text: "QT / LOCAL"

        cssPixelSize: leftText.cssPixelSize
        font.letterSpacing: 1.1
        color: root.tokens.muted
    }
}
