// Measures and composes column navigation without coupling it to the album ruler.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Row {
    id: genreStepper
    required property DesignTokens tokens
    property InputRouter inputRouter: null
    property FrameClock clock: null
    required property real viewportWidth
    required property bool wrapped
    required property real availableWidth
    property string columnLabel: ""
    property int columnNumber: 1
    property int columnCount: 1
    readonly property real naturalColumnWidth: Math.max(viewportWidth <= 700 ? 64 : 118, columnName.implicitWidth,
                                                        position.width)
    readonly property real naturalWidth: previousColumn.implicitWidth + nextColumn.implicitWidth
                                         + 2 * spacing + naturalColumnWidth

    objectName: "genreStepper"
    visible: Boolean(genreStepper.inputRouter)
    spacing: genreStepper.viewportWidth <= 700 ? 10 : genreStepper.viewportWidth <= 1100 ? 15 : 28
    ArchiveButton {
        id: previousColumn
        tokens: genreStepper.tokens
        text: "\u2190"
        cssPixelSize: genreStepper.viewportWidth <= 700 ? 20 : 25
        Accessible.name: qsTranslate("NavigationFrame", "Previous column")
        onClicked: genreStepper.inputRouter.lane(-1)
    }
    Column {
        id: columnLabels
        spacing: 5
        anchors.verticalCenter: parent.verticalCenter
        width: genreStepper.wrapped ? Math.min(genreStepper.naturalColumnWidth, Math.max(0,
                                                                                         genreStepper.availableWidth
                                                                                         - previousColumn.width
                                                                                         - nextColumn.width
                                                                                         - 2 * genreStepper.spacing)) :
                                      genreStepper.naturalColumnWidth
        Row {
            id: position
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 3
            readonly property real size: genreStepper.viewportWidth <= 700 ? 6 : 8
            ArchiveText {
                tokens: genreStepper.tokens
                text: qsTranslate("NavigationFrame", "GENRE")
                cssPixelSize: position.size
                font.letterSpacing: 1.3
                color: genreStepper.tokens.muted
            }
            OrdinalReel {
                tokens: genreStepper.tokens
                clock: genreStepper.clock
                value: genreStepper.columnNumber
                direction: genreStepper.inputRouter ? genreStepper.inputRouter.columnDirection : 0
                digitFont: genreStepper.tokens.regular
                cssPixelSize: position.size
                letterSpacing: 1.3
                color: genreStepper.tokens.muted
            }
            ArchiveText {
                tokens: genreStepper.tokens
                text: "/"
                cssPixelSize: position.size
                color: genreStepper.tokens.muted
            }
            OrdinalReel {
                tokens: genreStepper.tokens
                clock: genreStepper.clock
                value: genreStepper.columnCount
                digitFont: genreStepper.tokens.regular
                cssPixelSize: position.size
                letterSpacing: 1.3
                color: genreStepper.tokens.muted
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
            tokens: genreStepper.tokens
            text: genreStepper.columnLabel
            cssPixelSize: genreStepper.viewportWidth <= 700 ? 11 : 14
            font.letterSpacing: 1
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    ArchiveButton {
        id: nextColumn
        tokens: genreStepper.tokens
        text: "\u2192"
        cssPixelSize: genreStepper.viewportWidth <= 700 ? 20 : 25
        Accessible.name: qsTranslate("NavigationFrame", "Next column")
        onClicked: genreStepper.inputRouter.lane(1)
    }
}
