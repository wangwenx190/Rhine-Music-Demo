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
    readonly property point parentPosition: parent ? parent.mapToItem(null, 0, 0) : Qt.point(0, 0)
    readonly property var hostWindow: parent ? parent.Window.window : null
    x: parent ? Math.max(8 - parentPosition.x, Math.min((parent.width - width) / 2, (hostWindow
                                                                                     ? hostWindow.width :
                                                                                       320) - parentPosition.x
                                                        - width - 8)) : 0
    y: parent && hostWindow && parentPosition.y + parent.height + height + 4 > hostWindow.height ?
           -height - 4 : parent ? parent.height + 4 : 0
    popupType: T.Popup.Item
    closePolicy: T.Popup.NoAutoClose
    focus: false
    enter: null
    exit: null
    background: Rectangle {
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
