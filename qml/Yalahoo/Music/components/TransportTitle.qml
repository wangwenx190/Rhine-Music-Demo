pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.Button {
    id: root
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    required property DesignTokens tokens
    property FrameClock clock: null
    property string title: ""
    property string retainedTitle: ""
    property real cap: 240
    property real pixelSize: 12
    readonly property alias motion: transition
    readonly property TitleReelMotion reelController: caption.controller
    signal locateRequested
    width: transition.width
    height: 29
    padding: 0
    clip: true
    enabled: title.length > 0 && width > 0
    opacity: transition.opacity
    Accessible.name: "定位歌曲：" + title
    Accessible.ignored: !enabled
    ArchiveToolTip {
        parent: root
        tokens: root.tokens
        text: root.title
        visible: root.visible && root.hovered && root.enabled
    }
    background: Item {}
    contentItem: Item {
        TitleReel {
            id: caption
            objectName: "transportTitleReel"
            tokens: root.tokens
            clock: root.clock
            text: root.retainedTitle
            titleFont: root.tokens.regular
            pixelSize: root.pixelSize
            transport: true
            x: transition.offset + 2
            width: Math.max(0, root.cap - 18)
            height: 29
            budget: 29
            visible: root.title.length > 0 || transition.moving
        }
    }
    TransportMotion {
        id: transition
        clock: root.clock
        targetWidth: Math.min(root.cap, Math.ceil(caption.naturalWidth + 4) + 14)
        shown: root.title.length > 0
        reducedMotion: root.tokens.reducedMotion
        active: root.visible && (root.tokens.offscreenRendering || !root.Window.window
                                 || root.Window.window.visible)
    }
    onClicked: locateRequested()
    onTitleChanged: {
        if (title.length > 0)
            retainedTitle = title;
        else
            caption.controller.finish();
    }
    Component.onCompleted: {
        if (title.length > 0)
            retainedTitle = title;
    }
}
