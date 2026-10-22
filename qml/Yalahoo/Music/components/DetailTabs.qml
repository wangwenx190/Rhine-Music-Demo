// Composes sticky detail tabs and reports actual button geometry to the presentation controller.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: tabBar
    required property DesignTokens tokens
    required property PresentationController presentation
    required property int activeTab
    required property real viewportWidth
    required property real viewportHeight
    required property real tabsY
    required property bool portrait
    required property Item portraitSource
    property Item sceneSource: null
    required property Item contentSource
    required property real sceneX
    required property real sceneY
    required property real portraitY
    readonly property real wideWidthPixels: 1700
    signal tabRequested(int tab)

    component DetailTab: ArchiveButton {
        id: tab
        required property int tabIndex
        readonly property color ink: tabBar.activeTab === tabIndex ? tabBar.tokens.ink :
                                                                     tabBar.tokens.muted

        tokens: tabBar.tokens
        height: tabBar.height - 7
        onXChanged: tabBar.syncIndicator(false)
        onWidthChanged: tabBar.syncIndicator(false)
        contentItem: Row {
            spacing: 6
            topPadding: 15
            ArchiveText {
                tokens: tabBar.tokens
                text: (tab.tabIndex + 1).toString().padStart(2, "0")
                cssPixelSize: 8
                color: tab.ink
            }
            ArchiveText {
                id: label
                tokens: tabBar.tokens
                text: tab.text
                cssPixelSize: tabBar.viewportWidth >= tabBar.wideWidthPixels ? 14 :
                                                                               tabBar.viewportWidth
                                                                               <= 1100 ? 10 : 11
                color: tab.ink
            }
        }
        implicitWidth: contentItem.implicitWidth
        implicitHeight: label.implicitHeight
        onClicked: tabBar.tabRequested(tabIndex)
        Accessible.role: Accessible.PageTab
        Accessible.selected: tabBar.activeTab === tabIndex
    }
    objectName: "detailTabs"
    height: 6 + Math.max(trackTab.implicitHeight, aboutTab.implicitHeight) + 26 + 1
    y: Math.max(0, tabBar.tabsY)
    Loader {
        objectName: "detailTabsBlurLoader"
        anchors.fill: parent
        active: tabBar.visible && tabBar.y < tabBar.viewportHeight
        sourceComponent: DetailTabsBackdrop {
            hdrActive: tabBar.tokens.hdrActive
            sceneSource: tabBar.portrait ? tabBar.portraitSource : tabBar.sceneSource
            contentSource: tabBar.contentSource
            sceneX: tabBar.sceneX
            sceneY: tabBar.sceneY + tabBar.y - (tabBar.portrait ? tabBar.portraitY : 0)
            contentY: tabBar.y
        }
    }
    Rectangle {
        antialiasing: true
        anchors.fill: parent
        color: tabBar.tokens.veil
    }
    Row {
        id: tabs
        y: 6
        spacing: tabBar.viewportWidth <= 1100 ? 14 : 22
        DetailTab {
            id: trackTab
            tabIndex: 0
            objectName: "detailTracksTab"
            text: qsTranslate("Detail", "Track list")
            Keys.onRightPressed: {
                tabBar.tabRequested(1);
                aboutTab.forceActiveFocus();
            }
        }
        DetailTab {
            id: aboutTab
            tabIndex: 1
            objectName: "detailAboutTab"
            text: qsTranslate("Detail", "Album introductions")
            Keys.onLeftPressed: {
                tabBar.tabRequested(0);
                trackTab.forceActiveFocus();
            }
        }
    }
    Rectangle {
        antialiasing: true
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: tabBar.tokens.line
    }
    Rectangle {
        antialiasing: true
        objectName: "detailTabIndicator"
        y: parent.height - 1
        height: 2
        x: tabBar.presentation.tabX
        width: tabBar.presentation.tabWidth
        color: tabBar.tokens.ink
    }
    function syncIndicator(animate: bool): void {
        if (!tabBar.presentation || !trackTab || !aboutTab)
            return;
        const button = tabBar.activeTab === 0 ? trackTab : aboutTab;
        tabBar.presentation.setTabIndicator(button.x, button.width, animate);
    }
    onWidthChanged: syncIndicator(false)
    Connections {
        target: tabBar
        function onActiveTabChanged() {
            tabBar.syncIndicator(true);
        }
    }
    Connections {
        target: tabBar.presentation
        function onPrepared() {
            tabBar.syncIndicator(false);
        }
    }
}
