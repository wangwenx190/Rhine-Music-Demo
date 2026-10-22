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
    required property string genreId
    required property real viewportHeight
    property Component heading: null
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
        onTextChanged: filters.queryChanged(query.text)
        Keys.onDownPressed: filters.resultsRequested()
        onAccepted: filters.resultAccepted()
    }
    ListView {
        id: genreList
        objectName: "libraryGenreFilters"
        width: parent.width
        height: Math.min(contentHeight, Math.max(27, filters.viewportHeight * .25))
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        model: filters.genreRows
        spacing: 7
        delegate: Row {
            id: genreRow
            required property var entries
            spacing: 7
            height: 27
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
                    height: 27
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
