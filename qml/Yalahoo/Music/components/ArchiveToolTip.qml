pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.ToolTip {
    id: tip
    required property DesignTokens tokens
    padding: 8
    margins: 8
    width: Math.min(320, implicitContentWidth + leftPadding + rightPadding, parent && parent.Window.window
                    ? parent.Window.window.width - 16 : 320)
    height: implicitContentHeight + topPadding + bottomPadding
    // Popup tracks ancestor geometry, clips to margins and flips around the host.
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? parent.height + 4 : 0
    popupType: T.Popup.Item
    closePolicy: T.Popup.NoAutoClose
    focus: false
    enter: null
    exit: null
    background: Rectangle {
        antialiasing: true
        color: tip.tokens.surface
        border.color: tip.tokens.line
        radius: 2
    }
    contentItem: ArchiveText {
        tokens: tip.tokens
        text: tip.text
        cssPixelSize: 10
        wrapMode: Text.Wrap
    }
}
