// Displays virtualized genre rows and search input; the parent owns result navigation.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

Column {
    id: filters
    required property DesignTokens tokens
    required property GenreRowsModel genreRows
    required property string queryText
    required property string genreId
    required property real viewportHeight
    property Component heading: null
    readonly property real genreChipHeight: 27
    readonly property real genreFocusMargin: 6
    readonly property real genreContentWidth: Math.max(0, width - genreFocusMargin * 2)
    signal queryChanged(string query)
    signal genreRequested(string genreId)
    signal resultsRequested
    signal resultAccepted

    property alias queryInput: query
    bottomPadding: 16
    spacing: 16
    Loader {
        width: parent.width
        sourceComponent: filters.heading
    }
    ArchiveTextField {
        id: query
        tokens: filters.tokens
        objectName: "librarySearchInput"
        width: parent.width
        font.family: filters.tokens.regular.family
        font.pixelSize: 12
        color: filters.tokens.ink
        placeholderText: qsTranslate("SearchPanel", "Albums, tracks, artists, genres...")
        placeholderTextColor: filters.tokens.muted
        selectByMouse: true
        background: Rectangle {
            antialiasing: true
            color: filters.tokens.surface
            border.color: filters.tokens.line
        }
        Accessible.name: qsTranslate("SearchPanel", "Search albums and tracks")
        text: filters.queryText
        onTextChanged: filters.queryChanged(query.text)
        Keys.onDownPressed: filters.resultsRequested()
        onAccepted: filters.resultAccepted()
    }
    ListView {
        id: genreList
        objectName: "libraryGenreFilters"
        width: parent.width
        height: Math.min(contentHeight + filters.genreFocusMargin * 2, Math.max(
                             filters.genreChipHeight + filters.genreFocusMargin * 2,
                             filters.viewportHeight * .25))
        contentWidth: filters.genreContentWidth
        leftMargin: filters.genreFocusMargin
        rightMargin: filters.genreFocusMargin
        topMargin: filters.genreFocusMargin
        bottomMargin: filters.genreFocusMargin
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        model: filters.genreRows
        spacing: 7
        delegate: Row {
            id: genreRow
            required property var entries
            spacing: 7
            height: filters.genreChipHeight
            Repeater {
                model: genreRow.entries
                delegate: T.Button {
                    id: filter
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    required property var modelData
                    objectName: "genreFilter-" + modelData.id
                    text: modelData.name
                    width: modelData.size
                    height: filters.genreChipHeight
                    font.family: filters.tokens.regular.family
                    font.styleName: filters.tokens.regular.styleName
                    font.weight: filters.tokens.regular.weight
                    font.pixelSize: 10
                    padding: 5
                    checked: filters.genreId === modelData.id
                    background: Rectangle {
                        antialiasing: true
                        color: filter.checked ? filters.tokens.ink : "transparent"
                        border.color: filters.tokens.line
                        Rectangle {
                            objectName: "genreFocusOutline"
                            x: -filters.genreFocusMargin
                            y: -filters.genreFocusMargin
                            width: filter.width + filters.genreFocusMargin * 2
                            height: filter.height + filters.genreFocusMargin * 2
                            visible: filter.visualFocus
                            color: "transparent"
                            radius: 0
                            antialiasing: true
                            border.width: 2
                            border.color: filters.tokens.accent
                        }
                    }
                    contentItem: Text {
                        antialiasing: true
                        text: filter.text
                        textFormat: Text.PlainText
                        font: filter.font
                        renderType: Text.CurveRendering
                        color: filter.checked ? filters.tokens.surface : filters.tokens.ink
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                    onClicked: {
                        filters.genreRequested(modelData.id);
                    }
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: checked
                }
            }
        }
        T.ScrollBar.vertical: ArchiveScrollBar {
            tokens: filters.tokens
            policy: T.ScrollBar.AsNeeded
        }
    }
}
