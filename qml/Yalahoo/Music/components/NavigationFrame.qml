pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    property RulerController rulerController: null
    property InputRouter inputRouter: null
    property FrameClock clock: null
    property SelectionController selection: null
    property string columnLabel: ""
    property int columnNumber: 1
    property int columnCount: 1
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
    width: metrics.width - x - (metrics.width <= 700 ? 23 : metrics.width * 0.062)
    readonly property real counterHeight: labelRowHeight + 6 + Math.max(numberSize, total.y
                                                                        + total.height)
    height: counterHeight + (wrapped ? gridGap + genreStepper.height : 0)
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
        text: qsTr("ALBUM / SELECT")
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
            visible: !root.clock
            tokens: root.tokens
            text: root.ordinal
            font.family: MusicFonts.light.family
            font.styleName: MusicFonts.light.styleName
            font.weight: MusicFonts.light.weight
            cssPixelSize: root.numberSize
            font.letterSpacing: -3
            cssLineHeight: root.numberSize
        }
        OrdinalReel {
            id: reel
            objectName: "selectionOrdinalReel"
            tokens: root.tokens
            visible: Boolean(root.clock)
            clock: root.clock
            value: parseInt(root.ordinal, 10)
            direction: root.inputRouter ? root.inputRouter.rowDirection : 0
            cssPixelSize: root.numberSize
        }
        ArchiveText {
            id: total
            objectName: "staticTotal"
            tokens: root.tokens
            x: (root.clock ? reel.width : number.width) + 8
            y: number.y + number.baselineOffset - baselineOffset
            text: root.clock ? "/" : "/  " + root.albumCount.toString().padStart(2, "0")
            font.family: MusicFonts.light.family
            font.styleName: MusicFonts.light.styleName
            font.weight: MusicFonts.light.weight
            cssPixelSize: root.metrics.width <= 700 ? 9 : 12
            color: root.tokens.muted
        }
        OrdinalReel {
            tokens: root.tokens
            visible: Boolean(root.clock)
            clock: root.clock
            value: root.albumCount
            direction: root.inputRouter ? root.inputRouter.columnDirection : 0
            x: total.x + total.width + (root.metrics.width <= 700 ? 0 : 8)
            y: number.baselineOffset - baselineOffset
            cssPixelSize: root.metrics.width <= 700 ? 9 : 12
            letterSpacing: 1
            color: root.tokens.muted
        }
    }
    readonly property real firstColumnWidth: metrics.width <= 700 ? 74 : metrics.width <= 1100 ? 145 :
                                                                                                 210
    readonly property real gridGap: metrics.width <= 700 ? 12 : metrics.width <= 1100 ? 20 : 40
    readonly property real middleWidth: (width - firstColumnWidth - 2 * gridGap) / (metrics.width
                                                                                    <= 1100 ? 2 :
                                                                                              2.1)
    readonly property real naturalColumnWidth: Math.max(metrics.width <= 700 ? 64 : 118,
                                                        columnName.implicitWidth, position.width)
    readonly property real naturalGenreWidth: previousColumn.implicitWidth
                                              + nextColumn.implicitWidth + 2 * genreStepper.spacing
                                              + naturalColumnWidth
    readonly property bool wrapped: rulerController !== null && inputRouter !== null
                                    && firstColumnWidth + 2 * gridGap + stepper.width
                                    + naturalGenreWidth > width
    Row {
        id: stepper
        objectName: "albumStepper"
        visible: Boolean(root.rulerController)
        spacing: root.metrics.width <= 700 ? 14 : 21
        x: root.firstColumnWidth + root.gridGap + ((root.wrapped ? root.width
                                                                   - root.firstColumnWidth
                                                                   - root.gridGap :
                                                                   root.middleWidth) - width) / 2
        y: (root.counterHeight - height) / 2
        ArchiveButton {
            tokens: root.tokens
            text: "\u2191"
            cssPixelSize: root.metrics.width <= 700 ? 21 : 25
            Accessible.name: qsTr("Previous album")
            enabled: Boolean(root.selection && root.selection.canMovePrevious)
            onClicked: root.inputRouter.row(-1)
        }
        Ruler {
            controller: root.rulerController
            tokens: root.tokens
            anchors.verticalCenter: parent.verticalCenter
        }
        ArchiveButton {
            tokens: root.tokens
            text: "\u2193"
            cssPixelSize: root.metrics.width <= 700 ? 21 : 25
            Accessible.name: qsTr("Next album")
            enabled: Boolean(root.selection && root.selection.canMoveNext)
            onClicked: root.inputRouter.row(1)
        }
    }
    Row {
        id: genreStepper
        objectName: "genreStepper"
        visible: Boolean(root.inputRouter)
        x: root.wrapped ? (root.width - width) / 2 : root.firstColumnWidth + root.gridGap * 2
                          + root.middleWidth + (root.width - root.firstColumnWidth - root.gridGap
                                                * 2 - root.middleWidth - width) / 2
        spacing: root.metrics.width <= 700 ? 10 : root.metrics.width <= 1100 ? 15 : 28
        y: root.wrapped ? root.counterHeight + root.gridGap : (root.counterHeight - height) / 2
        ArchiveButton {
            id: previousColumn
            tokens: root.tokens
            text: "\u2190"
            cssPixelSize: root.metrics.width <= 700 ? 20 : 25
            Accessible.name: qsTr("Previous column")
            onClicked: root.inputRouter.lane(-1)
        }
        Column {
            id: columnLabels
            spacing: 5
            anchors.verticalCenter: parent.verticalCenter
            width: root.wrapped ? Math.min(root.naturalColumnWidth, Math.max(0, root.width
                                                                             - previousColumn.width
                                                                             - nextColumn.width - 2
                                                                             * genreStepper.spacing)) :
                                  root.naturalColumnWidth
            Row {
                id: position
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3
                readonly property real size: root.metrics.width <= 700 ? 6 : 8
                ArchiveText {
                    tokens: root.tokens
                    text: qsTr("GENRE")
                    cssPixelSize: position.size
                    font.letterSpacing: 1.3
                    color: root.tokens.muted
                }
                OrdinalReel {
                    tokens: root.tokens
                    clock: root.clock
                    value: root.columnNumber
                    direction: root.inputRouter ? root.inputRouter.columnDirection : 0
                    digitFont: root.tokens.regular
                    cssPixelSize: position.size
                    letterSpacing: 1.3
                    color: root.tokens.muted
                }
                ArchiveText {
                    tokens: root.tokens
                    text: "/"
                    cssPixelSize: position.size
                    color: root.tokens.muted
                }
                OrdinalReel {
                    tokens: root.tokens
                    clock: root.clock
                    value: root.columnCount
                    digitFont: root.tokens.regular
                    cssPixelSize: position.size
                    letterSpacing: 1.3
                    color: root.tokens.muted
                }
            }
            ArchiveText {
                id: columnName
                objectName: "navigationColumnLabel"
                Accessible.role: Accessible.StaticText
                Accessible.name: text
                width: columnLabels.width
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                tokens: root.tokens
                text: root.columnLabel
                cssPixelSize: root.metrics.width <= 700 ? 11 : 14
                font.letterSpacing: 1
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        ArchiveButton {
            id: nextColumn
            tokens: root.tokens
            text: "\u2192"
            cssPixelSize: root.metrics.width <= 700 ? 20 : 25
            Accessible.name: qsTr("Next column")
            onClicked: root.inputRouter.lane(1)
        }
    }
    Binding {
        target: root.rulerController
        property: "narrow"
        value: root.metrics.width <= 700
        when: Boolean(root.rulerController)
    }
    Binding {
        target: root.rulerController
        property: "active"
        value: root.visible && (root.tokens.offscreenRendering || !root.Window.window
                                || root.Window.window.visible)
        when: Boolean(root.rulerController)
    }
}
