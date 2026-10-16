pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.ScrollView {
    id: control
    required property DesignTokens tokens
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    background: Item {}
    clip: true
    T.ScrollBar.vertical: ArchiveScrollBar {
        parent: control
        tokens: control.tokens
        x: control.mirrored ? 0 : control.width - width
        y: control.topPadding
        height: control.availableHeight
        policy: T.ScrollBar.AsNeeded
    }
    T.ScrollBar.horizontal: ArchiveScrollBar {
        parent: control
        tokens: control.tokens
        x: control.leftPadding
        y: control.height - height
        width: control.availableWidth
        policy: T.ScrollBar.AsNeeded
    }
}
