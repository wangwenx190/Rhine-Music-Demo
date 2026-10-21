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
    readonly property SearchHeader searchHeader: results.headerItem as SearchHeader
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
        id: genreRows
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
    component SearchHeader: Column {
        id: filters
        property alias queryInput: query
        width: results.width
        bottomPadding: 16
        spacing: 16
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        ArchiveTextField {
            id: query
            tokens: root.tokens
            objectName: "librarySearchInput"
            width: parent.width
            font.family: root.tokens.regular.family
            font.pixelSize: 12
            color: root.tokens.ink
            placeholderText: qsTr("Albums, tracks, artists, genres...")
            placeholderTextColor: root.tokens.muted
            selectByMouse: true
            background: Rectangle {
                antialiasing: true
                color: root.tokens.surface
                border.color: root.tokens.line
            }
            Accessible.name: qsTr("Search albums and tracks")
            onTextChanged: root.refresh()
            Keys.onDownPressed: {
                if (results.count) {
                    results.currentIndex = 0;
                    results.forceActiveFocus();
                }
            }
            onAccepted: if (results.count)
                            results.activate(Math.max(0, results.currentIndex))
        }
        ListView {
            id: genreList
            objectName: "libraryGenreFilters"
            width: parent.width
            height: Math.min(contentHeight, Math.max(27, root.height * .25))
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: genreRows
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
                        text: modelData.name
                        width: modelData.size
                        height: 27
                        font.family: root.tokens.regular.family
                        font.styleName: root.tokens.regular.styleName
                        font.weight: root.tokens.regular.weight
                        font.pixelSize: 10
                        padding: 5
                        checked: root.genreId === modelData.id
                        background: Rectangle {
                            antialiasing: true
                            color: filter.checked ? root.tokens.ink : "transparent"
                            border.color: root.tokens.line
                        }
                        contentItem: Text {
                            antialiasing: true
                            text: filter.text
                            font: filter.font
                            renderType: Text.CurveRendering
                            color: filter.checked ? root.tokens.surface : root.tokens.ink
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            root.genreId = modelData.id;
                            root.refresh();
                        }
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: checked
                    }
                }
            }
            T.ScrollBar.vertical: ArchiveScrollBar {
                tokens: root.tokens
                policy: T.ScrollBar.AsNeeded
            }
        }
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
        header: SearchHeader {}
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
        delegate: T.Button {
            id: result
            objectName: "librarySearchResult-" + index
            enabled: !root.librarySession.searchResults.searching
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            required property int index
            required property string kind
            required property string albumId
            required property string trackId
            required property string title
            required property string artist
            required property string subtitle
            required property url coverUrl
            property bool pendingPress: false
            property string pressedAlbumId: ""
            property string pressedTrackId: ""
            width: results.width
            height: Math.max(72, copy.implicitHeight + 22)
            padding: 0
            background: Rectangle {
                color: result.hovered || result.activeFocus || (results.activeFocus
                                                                && results.currentIndex
                                                                === result.index)
                       ? root.tokens.veil : "transparent"
                Rectangle {
                    antialiasing: true
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: root.tokens.line
                }
            }
            contentItem: Item {
                Rectangle {
                    antialiasing: true
                    width: 49
                    height: 49
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.tokens.surface
                    border.color: root.tokens.line
                    Image {
                        id: artwork
                        smooth: true
                        mipmap: true
                        anchors.fill: parent
                        anchors.margins: 1
                        source: result.coverUrl
                        sourceSize: Qt.size(Math.round(width * (Window.window ? Window.window.devicePixelRatio :
                                                                                1)), Math.round(
                                                height * (Window.window
                                                          ? Window.window.devicePixelRatio : 1)))
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    ArchiveText {
                        anchors.centerIn: parent
                        tokens: root.tokens
                        visible: result.coverUrl.toString().length === 0 || artwork.status
                                 === Image.Error

                        text: "\u266a"
                        cssPixelSize: 16
                    }
                }
                Column {
                    id: copy
                    x: 62
                    width: Math.max(0, parent.width - x - 62)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    ArchiveText {
                        width: parent.width
                        tokens: root.tokens
                        text: result.title
                        cssPixelSize: 12
                        wrapMode: Text.WrapAnywhere
                    }
                    ArchiveText {
                        width: parent.width
                        tokens: root.tokens
                        text: result.artist + " \u00b7 " + result.subtitle
                        cssPixelSize: 10
                        color: root.tokens.muted
                        wrapMode: Text.WrapAnywhere
                    }
                }
                ArchiveText {
                    x: parent.width - 49
                    anchors.verticalCenter: parent.verticalCenter
                    tokens: root.tokens
                    text: result.kind === "track" ? qsTr("Track") : qsTr("Albums")
                    cssPixelSize: 10
                    color: root.tokens.muted
                }
                ArchiveText {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    tokens: root.tokens
                    text: "\u2197"
                    cssPixelSize: 17
                }
            }
            onPressed: {
                pendingPress = true;
                pressedAlbumId = albumId;
                pressedTrackId = trackId;
            }
            onCanceled: pendingPress = false
            onClicked: {
                const unchanged = !pendingPress || (pressedAlbumId === albumId && pressedTrackId
                                                    === trackId);
                pendingPress = false;
                if (unchanged)
                    root.resultRequested(albumId, trackId);
            }
            Accessible.name: (kind === "track" ? qsTr("Locate track %1, %2") : qsTr(
                                                     "Locate album %1, %2")).arg(title).arg(
                                 subtitle)
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
