pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

FocusScope {
    id: root
    required property LayoutMetrics metrics
    required property DesignTokens tokens
    required property PresentationController presentation
    required property DetailModel detailModel
    property string playingTrackId: ""
    property int activeTab: 0
    property int queuedLayoutRevision: -1
    property bool layoutPending: false
    function finishLayout() {
        const revision = queuedLayoutRevision;
        layoutPending = false;
        tracks.forceLayout();
        tracks.positionViewAtBeginning();
        presentation.layoutReady(revision);
    }
    readonly property var album: detailModel.album
    readonly property real contentLeft: metrics.portrait ? Math.max(22, Math.min(42, width * 0.04)) :
                                                           width * 0.52
    readonly property real contentRight: metrics.portrait ? Math.max(18, Math.min(42, width * 0.04)) : width
                                                            * 0.045
    readonly property real contentTop: metrics.portrait ? height * 0.5 : width >= 1700 ? 210 :
                                                                                         width <= 1100
                                                                                         ? 132 : 170
    visible: presentation.detailSurface.shown
    opacity: presentation.detailSurface.opacity
    enabled: presentation.detailEnabled
    signal backRequested
    signal trackRequested(string trackId)
    Keys.onEscapePressed: event => {
        root.backRequested();
        event.accepted = true;
    }
    Connections {
        target: root.presentation
        function onPrepared() {
            root.activeTab = 0;
            tracks.positionViewAtBeginning();
            root.queuedLayoutRevision = root.presentation.layoutRevision;
            // Keep one layout callback; new preparations replace its revision.
            if (!root.layoutPending) {
                root.layoutPending = true;
                Qt.callLater(root.finishLayout);
            }
        }
        function onFocusRequested(detail) {
            if (detail) {
                Qt.callLater(() => {
                    if (root.presentation.detailEnabled)
                        root.forceActiveFocus(Qt.OtherFocusReason);
                });
            }
        }
    }
    Rectangle {
        visible: root.metrics.portrait
        y: root.height * 0.46
        width: root.width
        height: root.height - y
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.rgba(root.tokens.page.r, root.tokens.page.g, root.tokens.page.b, 0)
            }
            GradientStop {
                position: Math.min(1, 34 / Math.max(1, root.height * 0.54))
                color: root.tokens.page
            }
        }
    }
    ArchiveButton {
        objectName: "detailBack"
        tokens: root.tokens
        x: root.width <= 700 ? 22 : root.width >= 1700 ? 62 : root.width <= 1100 ? 27 : 42
        y: root.width <= 700 ? 87 : root.width >= 1700 ? 144 : root.width <= 1100 ? 91 : 112
        text: "← 返回收藏架     Esc"
        cssPixelSize: root.width >= 1700 ? 12 : 10
        onClicked: root.backRequested()
    }
    ArchiveText {
        tokens: root.tokens
        x: root.width <= 700 ? 22 : 42
        y: root.metrics.portrait ? root.height * 0.43 : root.height - 82
        text: "ALBUM " + (root.album.code || 1).toString().padStart(3, "0")
        cssPixelSize: root.width <= 700 ? 9 : 12
        font.letterSpacing: 2
    }
    TrackList {
        id: tracks
        objectName: "detailTrackList"
        tokens: root.tokens
        detailModel: root.detailModel
        viewportWidth: root.width
        playingTrackId: root.playingTrackId
        about: root.activeTab === 1
        x: root.contentLeft + root.presentation.detailSurface.offset
        y: root.contentTop
        width: Math.max(0, root.width - root.contentLeft - root.contentRight - (root.metrics.portrait
                                                                                ? 5 : 9))
        height: Math.max(0, root.height - y - (root.metrics.portrait ? 28 : root.width >= 1700 ? 75 :
                                                                                                 52))
        onTrackRequested: trackId => root.trackRequested(trackId)
        header: Column {
            width: tracks.width
            spacing: 0
            onHeightChanged: {
                if (tracks.preserveBeginning)
                    Qt.callLater(tracks.restoreBeginning);
            }
            Item {
                id: overline
                width: parent.width
                readonly property real contentHeight: Math.max(ordinal.implicitHeight,
                                                               navigation.implicitHeight)
                height: contentHeight + (root.width <= 700 ? 12 : 18)
                ArchiveText {
                    id: ordinal
                    objectName: "detailOrdinal"
                    y: (overline.contentHeight - height) / 2
                    tokens: root.tokens
                    text: "ALBUM " + (root.album.code || 1).toString().padStart(3, "0")
                    cssPixelSize: root.width <= 700 ? 7 : 8
                    font.letterSpacing: 1.6
                }
                ArchiveText {
                    id: navigationMeasure
                    tokens: root.tokens
                    visible: false
                    text: "下一张 ↓"
                    cssPixelSize: 9
                }
                Row {
                    id: navigation
                    anchors.right: parent.right
                    y: (overline.contentHeight - height) / 2
                    spacing: 20
                    ArchiveButton {
                        objectName: "detailPrevious"
                        tokens: root.tokens
                        text: "↑ 上一张"
                        cssPixelSize: 9
                        height: navigationMeasure.implicitHeight
                        enabled: root.presentation.canMovePrevious
                        onClicked: root.presentation.stepAlbum(-1)
                    }
                    ArchiveButton {
                        objectName: "detailNext"
                        tokens: root.tokens
                        text: "下一张 ↓"
                        cssPixelSize: 9
                        height: navigationMeasure.implicitHeight
                        enabled: root.presentation.canMoveNext
                        onClicked: root.presentation.stepAlbum(1)
                    }
                }
            }
            Column {
                width: parent.width
                bottomPadding: 10
                spacing: title.cssPixelSize * 0.5 * 0.48
                ArchiveText {
                    id: title
                    objectName: "detailTitle"
                    tokens: root.tokens
                    width: parent.width
                    text: root.album.mainTitle || root.album.title || ""
                    cssPixelSize: Math.min(39, Math.max(24, root.width * 0.0245))
                    cssLineHeight: cssPixelSize * 1.22
                    font.styleName: root.tokens.demibold.styleName
                    font.weight: root.tokens.demibold.weight
                    font.letterSpacing: -0.6
                    wrapMode: Text.WrapAnywhere
                    Accessible.name: root.album.title || ""
                }
                ArchiveText {
                    objectName: "detailTranslation"
                    tokens: root.tokens
                    width: parent.width
                    visible: text.length > 0
                    text: root.album.translation || ""
                    cssPixelSize: title.cssPixelSize * 0.5
                    cssLineHeight: cssPixelSize * 1.45
                    font.styleName: root.tokens.demibold.styleName
                    font.weight: root.tokens.demibold.weight
                    font.letterSpacing: cssPixelSize * 0.01
                    color: root.tokens.muted
                    wrapMode: Text.WrapAnywhere
                }
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: (root.album.artist || "") + (root.album.offline ? " · 目录离线" : "")
                cssPixelSize: root.width <= 700 ? 12 : root.width >= 1700 ? 17 : 14
                wrapMode: Text.Wrap
                bottomPadding: root.width <= 700 ? 16 : 22
            }
            Rectangle {
                width: parent.width
                height: 1
                color: root.tokens.ink
            }
            Grid {
                width: parent.width
                columns: 2
                columnSpacing: 18
                rowSpacing: 14
                topPadding: 19
                bottomPadding: 19
                Repeater {
                    model: root.album.facts || []
                    delegate: Column {
                        required property var modelData
                        width: (tracks.width - 18) / 2
                        spacing: 5
                        ArchiveText {
                            tokens: root.tokens
                            width: parent.width
                            text: parent.modelData.label
                            cssPixelSize: root.width >= 1700 ? 9 : 7
                            color: root.tokens.muted
                            wrapMode: Text.Wrap
                        }
                        ArchiveText {
                            tokens: root.tokens
                            width: parent.width
                            text: parent.modelData.value
                            cssPixelSize: root.width >= 1700 ? 13 : root.width <= 700 ? 10 : 11
                            cssLineHeight: cssPixelSize * 1.5
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 1
                color: root.tokens.line
            }
            Row {
                spacing: 22
                topPadding: 6
                ArchiveButton {
                    tokens: root.tokens
                    text: "01  歌单"
                    cssPixelSize: root.width >= 1700 ? 14 : 11
                    height: 42
                    underline: root.activeTab === 0
                    onClicked: {
                        root.activeTab = 0;
                        tracks.positionViewAtBeginning();
                    }
                }
                ArchiveButton {
                    objectName: "detailAboutTab"
                    tokens: root.tokens
                    text: "02  专辑介绍"
                    cssPixelSize: root.width >= 1700 ? 14 : 11
                    height: 42
                    underline: root.activeTab === 1
                    onClicked: {
                        root.activeTab = 1;
                        tracks.positionViewAtBeginning();
                    }
                }
            }
            Column {
                visible: root.activeTab === 1
                width: parent.width
                spacing: 14
                topPadding: 25
                bottomPadding: 20
                ArchiveText {
                    tokens: root.tokens
                    text: "ABOUT THIS ALBUM"
                    cssPixelSize: 8
                    color: root.tokens.muted
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: root.album.description || "专辑介绍待补充"
                    cssPixelSize: root.width >= 1700 ? 12 : 11
                    cssLineHeight: cssPixelSize * 1.8
                    wrapMode: Text.Wrap
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: root.album.descriptionSource ? [root.album.descriptionSource.name,
                                                          root.album.descriptionSource.url,
                                                          root.album.descriptionSource.license].filter(
                                                             Boolean).join(" · ") : ""
                    cssPixelSize: 9
                    wrapMode: Text.WrapAnywhere
                    color: root.tokens.muted
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: "本地备注\n" + (root.album.localNote || "未提供")
                    cssPixelSize: 11
                    wrapMode: Text.Wrap
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    text: "本地目录\n" + (root.album.folder || "") + "\n" + (root.album.rawGenres
                                                                         || []).join(" · ")
                    cssPixelSize: 9
                    wrapMode: Text.WrapAnywhere
                    color: root.tokens.muted
                }
                ArchiveText {
                    tokens: root.tokens
                    text: "ALBUM CREDITS / 制作人员"
                    cssPixelSize: 12
                }
            }
            ArchiveText {
                tokens: root.tokens
                visible: root.activeTab === 0 && root.detailModel.count === 0
                width: parent.width
                topPadding: 25
                text: "这个专辑还没有可播放曲目"
                cssPixelSize: 13
                wrapMode: Text.Wrap
            }
        }
    }
}
