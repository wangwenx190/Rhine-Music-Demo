pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property LibrarySession librarySession
    property Component heading: null
    readonly property SearchFilters searchHeader: results.headerItem as SearchFilters
    readonly property ArchiveTextField queryInput: searchHeader ? searchHeader.queryInput : null
    property string genreId: ""
    signal resultRequested(string albumId, string trackId)
    implicitHeight: 560
    FontMetrics {
        id: genreFont
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: 10
    }
    GenreRowsModel {
        id: genreRowsModel
        genres: root.librarySession.genres
        font: genreFont.font
        width: root.width
    }
    function prepare(): void {
        if (queryInput)
            queryInput.clear();
        results.currentIndex = -1;
    }
    function focusSearch(): void {
        results.forceLayout();
        results.positionViewAtBeginning();
        if (queryInput)
            queryInput.forceActiveFocus();
    }
    function refresh(): void {
        if (!visible)
            return;
        librarySession.search(queryInput ? queryInput.text : "", genreId);
        results.currentIndex = -1;
        results.positionViewAtBeginning();
    }

    ListView {
        id: results
        objectName: "librarySearchResults"
        anchors.fill: parent
        clip: true
        reuseItems: true
        cacheBuffer: 100
        boundsBehavior: Flickable.StopAtBounds
        model: root.librarySession.searchResults
        currentIndex: -1
        header: SearchFilters {
            width: results.width
            tokens: root.tokens
            genreRows: genreRowsModel
            genreId: root.genreId
            viewportHeight: root.height
            heading: root.heading
            onQueryChanged: root.refresh()
            onGenreRequested: genreId => {
                root.genreId = genreId;
                root.refresh();
            }
            onResultsRequested: {
                if (results.count) {
                    results.currentIndex = 0;
                    results.forceActiveFocus();
                }
            }
            onResultAccepted: if (results.count)
                                  results.activate(Math.max(0, results.currentIndex))
        }
        function activate(index: int): void {
            if (root.librarySession.searchResults.searching)
                return;
            const item = root.librarySession.searchResults.resultAt(index);
            if (item.albumId)
                root.resultRequested(item.albumId, item.trackId);
        }
        Keys.onReturnPressed: activate(currentIndex)
        Keys.onEnterPressed: activate(currentIndex)
        Keys.onUpPressed: {
            if (currentIndex <= 0)
                root.focusSearch();
            else
                decrementCurrentIndex();
        }
        delegate: SearchResult {
            width: results.width
            tokens: root.tokens
            searching: root.librarySession.searchResults.searching
            currentResult: results.activeFocus && results.currentIndex === index
            onResultRequested: (albumId, trackId) => root.resultRequested(albumId, trackId)
        }
        T.ScrollBar.vertical: ArchiveScrollBar {
            tokens: root.tokens
            policy: T.ScrollBar.AsNeeded
        }
        ArchiveText {
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            tokens: root.tokens
            visible: results.count === 0
            text: qsTr("No albums or tracks found.")
            cssPixelSize: 12
            color: root.tokens.muted
        }
    }
    Connections {
        target: root.librarySession
        function onSnapshotChanged() {
            if (root.genreId && !root.librarySession.hasGenre(root.genreId))
                root.genreId = "";
            root.refresh();
        }
    }
    onVisibleChanged: if (visible)
                          refresh()
    Component.onCompleted: if (visible)
                               refresh()
}
