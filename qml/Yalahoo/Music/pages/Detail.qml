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
    property TrackFocusController trackFocus: null
    property string playingTrackId: ""
    property Item sceneSource: null
    property int activeTab: 0
    property int queuedLayoutRevision: -1
    property bool layoutPending: false
    function setTab(tab) {
        changeTab(tab, false);
    }
    function changeTab(tab, reveal) {
        if (activeTab === tab)
            return;
        if (!reveal && trackFocus)
            trackFocus.cancel();
        tracks.saveScroll();
        presentation.revealTabContent();
        activeTab = tab;
    }
    function focusDetailIfReady() {
        if (presentation.detailEnabled)
            forceActiveFocus(Qt.OtherFocusReason);
    }
    function revealAfterLayout(revision, index) {
        if (trackFocus && revision === trackFocus.revision)
            tracks.reveal(revision, index);
    }
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
    readonly property real contentRight: metrics.portrait ? Math.max(18, Math.min(42, width * 0.04)) : width * (
                                                                width <= 1100 ? 0.04 : 0.045)
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
                Qt.callLater(root.focusDetailIfReady);
            }
        }
    }
    Connections {
        target: root.trackFocus
        function onRevealRequested(revision, index) {
            root.changeTab(0, true);
            Qt.callLater(root.revealAfterLayout, revision, index);
        }
    }
    Rectangle {
        id: portraitBackdrop
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
    Column {
        objectName: "detailCaption"
        x: root.metrics.portrait ? root.contentLeft : root.width <= 1100 ? 27 : 42
        y: root.metrics.portrait ? root.height * 0.43 : root.height - 82 - height
        spacing: root.width <= 700 || root.metrics.portrait ? 5 : 10
        ArchiveText {
            tokens: root.tokens
            text: "ALBUM " + (root.album.code || 1).toString().padStart(3, "0")
            cssPixelSize: root.width <= 700 ? 9 : 12
            font.letterSpacing: 2
        }
        ArchiveText {
            visible: !root.metrics.portrait && root.width > 700
            tokens: root.tokens
            text: "拖动卡片，查看完整封面"
            cssPixelSize: root.width <= 1100 ? 8 : 9
            color: root.tokens.muted
        }
    }
    TrackList {
        id: tracks
        objectName: "detailTrackList"
        tokens: root.tokens
        detailModel: root.detailModel
        trackFocus: root.trackFocus
        viewportWidth: root.width
        playingTrackId: root.playingTrackId
        about: root.activeTab === 1
        contentOpacity: root.presentation.tabOpacity
        readonly property real tabHeight: tabBar.height
        x: root.contentLeft + root.presentation.detailSurface.offset
        y: root.contentTop
        width: Math.max(0, root.width - root.contentLeft - root.contentRight - (root.metrics.portrait
                                                                                ? 5 : 9))
        height: Math.max(0, root.height - y - (root.metrics.portrait ? 28 : root.width >= 1700 ? 75 :
                                                                                                 root.width
                                                                                                 <= 1100 ? 40 :
                                                                                                           52))
        onTrackRequested: trackId => root.trackRequested(trackId)
        header: Column {
            width: tracks.contentWidth
            property real tabsY: tabSpace.y
            property real tabsHeight: tabBar.height
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
            Row {
                id: artistRow
                objectName: "detailArtist"
                readonly property real contentHeight: Math.max(artistLine.height, offline.visible
                                                               ? offline.height : 0)
                width: parent.width
                spacing: 12
                bottomPadding: root.width <= 700 ? 16 : 22
                ArchiveText {
                    id: artistLine
                    tokens: root.tokens
                    y: (artistRow.contentHeight - height) / 2
                    text: root.album.artist || ""
                    width: Math.min(implicitWidth, parent.width - (offline.visible ? offline.width + 12 :
                                                                                     0))
                    cssPixelSize: root.width <= 700 ? 12 : root.width >= 1700 ? 17 : 14
                    wrapMode: Text.Wrap
                }
                Rectangle {
                    id: offline
                    objectName: "detailOfflineBadge"
                    visible: root.album.offline || false
                    width: badge.implicitWidth + 14
                    height: badge.implicitHeight + 8
                    color: "transparent"
                    border.color: root.tokens.line
                    y: (artistRow.contentHeight - height) / 2
                    ArchiveText {
                        id: badge
                        anchors.centerIn: parent
                        tokens: root.tokens
                        text: "目录离线"
                        cssPixelSize: 10
                        color: root.tokens.accent
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 1
                color: root.tokens.ink
            }
            Grid {
                id: facts
                objectName: "detailFacts"
                width: parent.width
                columns: 2
                columnSpacing: root.width <= 700 ? 16 : root.width <= 1100 ? 14 : 18
                rowSpacing: root.width <= 700 ? 11 : root.width <= 1100 ? 12 : 14
                topPadding: root.width <= 700 ? 15 : 19
                bottomPadding: root.width <= 700 ? 15 : 19
                Repeater {
                    model: root.album.facts || []
                    delegate: Column {
                        required property var modelData
                        width: (tracks.contentWidth - facts.columnSpacing) / 2
                        spacing: 5
                        ArchiveText {
                            tokens: root.tokens
                            width: parent.width
                            text: parent.modelData.label
                            objectName: "detailFactLabel"
                            cssPixelSize: root.width >= 1700 ? 9 : root.width <= 700 ? 6 : 7
                            font.letterSpacing: 0.6
                            color: root.tokens.muted
                            wrapMode: Text.Wrap
                        }
                        ArchiveText {
                            tokens: root.tokens
                            width: parent.width
                            text: parent.modelData.value
                            objectName: "detailFactValue"
                            cssPixelSize: root.width >= 1700 ? 13 : root.width <= 1100 ? 10 : 11
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
            Item {
                id: tabSpace
                width: parent.width
                height: tabBar.height
            }
            Column {
                visible: root.activeTab === 1
                opacity: root.presentation.tabOpacity
                width: parent.width
                spacing: 14
                topPadding: 27
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
                    objectName: "detailDescription"
                    text: root.album.description || "专辑介绍待补充"
                    cssPixelSize: 12
                    cssLineHeight: cssPixelSize * 1.95
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
                    objectName: "detailIntroductionStatus"
                    tokens: root.tokens
                    width: parent.width
                    text: root.album.introductionStatus || ""
                    cssPixelSize: 11
                    cssLineHeight: cssPixelSize * 1.9
                    wrapMode: Text.Wrap
                    color: root.tokens.muted
                    Accessible.role: Accessible.StaticText
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
                Column {
                    width: parent.width
                    spacing: 6
                    topPadding: 24
                    ArchiveText {
                        tokens: root.tokens
                        text: "ALBUM CREDITS"
                        cssPixelSize: 9
                        font.letterSpacing: 1.4
                        color: root.tokens.muted
                    }
                    ArchiveText {
                        tokens: root.tokens
                        text: "制作人员"
                        cssPixelSize: 16
                        font.letterSpacing: 0.4
                    }
                }
                ArchiveText {
                    objectName: "detailCreditsStatus"
                    tokens: root.tokens
                    width: parent.width
                    text: root.album.creditsStatus || "暂无制作资料。"
                    cssPixelSize: 11
                    cssLineHeight: cssPixelSize * 1.9
                    wrapMode: Text.Wrap
                    color: root.tokens.muted
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
        Item {
            id: tabBar
            objectName: "detailTabs"
            width: tracks.contentWidth
            height: 6 + Math.max(trackTab.implicitHeight, aboutTab.implicitHeight) + 26 + 1
            y: Math.max(0, tracks.tabsY)
            Loader {
                objectName: "detailTabsBlurLoader"
                anchors.fill: parent
                active: root.visible && tabBar.y < tracks.height
                sourceComponent: DetailTabsBackdrop {
                    sceneSource: root.metrics.portrait ? portraitBackdrop : root.sceneSource
                    contentSource: tracks.currentView
                    sceneX: tracks.x
                    sceneY: tracks.y + tabBar.y - (root.metrics.portrait ? portraitBackdrop.y : 0)
                    contentY: tabBar.y
                }
            }
            Rectangle {
                anchors.fill: parent
                color: root.tokens.veil
            }
            Row {
                id: tabs
                y: 6
                spacing: root.width <= 1100 ? 14 : 22
                ArchiveButton {
                    id: trackTab
                    onXChanged: tabBar.syncIndicator(false)
                    onWidthChanged: tabBar.syncIndicator(false)
                    objectName: "detailTracksTab"
                    tokens: root.tokens
                    text: "歌单"
                    height: tabBar.height - 7
                    contentItem: Row {
                        spacing: 6
                        topPadding: 15
                        ArchiveText {
                            tokens: root.tokens
                            text: "01"
                            cssPixelSize: 8
                            color: root.activeTab === 0 ? root.tokens.ink : root.tokens.muted
                        }
                        ArchiveText {
                            id: tabText
                            tokens: root.tokens
                            text: "歌单"
                            cssPixelSize: root.width >= 1700 ? 14 : root.width <= 1100 ? 10 : 11
                            color: root.activeTab === 0 ? root.tokens.ink : root.tokens.muted
                        }
                    }
                    implicitWidth: contentItem.implicitWidth
                    implicitHeight: tabText.implicitHeight
                    onClicked: root.setTab(0)
                    Accessible.role: Accessible.PageTab
                    Accessible.selected: root.activeTab === 0
                    Keys.onRightPressed: {
                        root.setTab(1);
                        aboutTab.forceActiveFocus();
                    }
                }
                ArchiveButton {
                    id: aboutTab
                    onXChanged: tabBar.syncIndicator(false)
                    onWidthChanged: tabBar.syncIndicator(false)
                    objectName: "detailAboutTab"
                    tokens: root.tokens
                    text: "专辑介绍"
                    height: trackTab.height
                    contentItem: Row {
                        spacing: 6
                        topPadding: 15
                        ArchiveText {
                            tokens: root.tokens
                            text: "02"
                            cssPixelSize: 8
                            color: root.activeTab === 1 ? root.tokens.ink : root.tokens.muted
                        }
                        ArchiveText {
                            tokens: root.tokens
                            text: "专辑介绍"
                            cssPixelSize: root.width >= 1700 ? 14 : root.width <= 1100 ? 10 : 11
                            color: root.activeTab === 1 ? root.tokens.ink : root.tokens.muted
                        }
                    }
                    implicitWidth: contentItem.implicitWidth
                    implicitHeight: trackTab.implicitHeight
                    onClicked: root.setTab(1)
                    Accessible.role: Accessible.PageTab
                    Accessible.selected: root.activeTab === 1
                    Keys.onLeftPressed: {
                        root.setTab(0);
                        trackTab.forceActiveFocus();
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.tokens.line
            }
            Rectangle {
                objectName: "detailTabIndicator"
                y: parent.height - 1
                height: 2
                x: root.presentation.tabX
                width: root.presentation.tabWidth
                color: root.tokens.ink
            }
            function syncIndicator(animate) {
                if (!root.presentation || !trackTab || !aboutTab)
                    return;
                const button = root.activeTab === 0 ? trackTab : aboutTab;
                root.presentation.setTabIndicator(button.x, button.width, animate);
            }
            onWidthChanged: syncIndicator(false)
            Connections {
                target: root
                function onActiveTabChanged() {
                    tabBar.syncIndicator(true);
                }
            }
            Connections {
                target: root.presentation
                function onPrepared() {
                    tabBar.syncIndicator(false);
                }
            }
        }
    }
}
