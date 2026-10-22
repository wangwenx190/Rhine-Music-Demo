// Displays one bounded overview label and its entry action from the C++ label model.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

Item {
    id: anchor
    required property OverviewController controller
    required property DesignTokens tokens
    required property Item sceneSource
    required property bool narrow
    required property real labelWidth
    required property real labelLineHeight
    required property real countLineHeight
    required property string columnKind
    signal returnFocusRequested

    required property int lane
    required property string columnName
    required property int albumCount
    required property real anchorX
    required property real anchorY
    required property real extent
    required property bool selectedColumn
    required property bool interactive
    required property bool expanded
    required property real fade
    required property real offset
    required property real entryWidth
    required property real entryOpacity
    required property real entryOffset
    readonly property real cardHeight: 1 + (anchor.narrow ? 16 : 19) + anchor.labelLineHeight + 5
                                       + anchor.countLineHeight
    objectName: "overviewAnchor-" + lane
    x: anchorX - anchor.labelWidth / 2
    y: anchorY - cardHeight
    width: anchor.labelWidth + entryWidth
    height: cardHeight
    visible: lane >= 0 && fade > 0
    opacity: extent
    Item {
        objectName: "overviewCard-" + anchor.lane
        y: anchor.offset
        width: parent.width
        height: parent.height
        opacity: anchor.fade
        Loader {
            anchors.fill: parent
            active: anchor.visible && anchor.extent > 0
            sourceComponent: OverviewBackdrop {
                hdrActive: anchor.tokens.hdrActive
                objectName: "overviewBackdrop-" + anchor.lane
                sceneSource: anchor.sceneSource
                sceneX: anchor.x
                sceneY: anchor.y + anchor.offset
                labelWidth: anchor.labelWidth
            }
        }
        Rectangle {
            antialiasing: true
            anchors.fill: parent
            color: Qt.rgba(anchor.tokens.surface.r, anchor.tokens.surface.g, anchor.tokens.surface.b,
                           .87)
        }
        Rectangle {
            antialiasing: true
            width: parent.width
            height: 1
            color: anchor.selectedColumn ? anchor.tokens.accent : anchor.tokens.line
        }
        Rectangle {
            antialiasing: true
            x: anchor.labelWidth / 2
            y: parent.height
            width: 1
            height: 22
            color: anchor.tokens.line
        }
        T.AbstractButton {
            id: label
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            objectName: "overviewLabel-" + anchor.lane
            y: 1
            width: anchor.labelWidth
            height: parent.height - 1
            enabled: anchor.controller.ready && anchor.interactive
            activeFocusOnTab: enabled
            Accessible.name: qsTranslate("Overview", "Expand %1 entry: %2").arg(
                                 anchor.columnKind).arg(anchor.columnName)
            Accessible.description: anchor.expanded ? qsTranslate("Overview", "Expanded") :
                                                      qsTranslate("Overview", "Collapsed")
            ArchiveToolTip {
                parent: label
                tokens: anchor.tokens
                text: anchor.columnName
                visible: label.visible && label.hovered
            }
            background: Rectangle {
                antialiasing: true
                color: "transparent"
                border.width: label.visualFocus ? 1 : 0
                border.color: anchor.tokens.accent
                anchors.margins: 3
            }
            contentItem: Item {
                ArchiveText {
                    objectName: "overviewName-" + anchor.lane
                    x: anchor.narrow ? 8 : 12
                    y: anchor.narrow ? 8 : 10
                    width: parent.width - x * 2
                    tokens: anchor.tokens
                    text: anchor.columnName
                    elide: Text.ElideRight
                    cssPixelSize: anchor.narrow ? 11 : 12
                    cssLineHeight: anchor.labelLineHeight
                    font: anchor.tokens.regular
                }
                ArchiveText {
                    objectName: "overviewCount-" + anchor.lane
                    x: anchor.narrow ? 8 : 12
                    width: parent.width - x * 2
                    y: (anchor.narrow ? 8 : 10) + anchor.labelLineHeight + 5
                    tokens: anchor.tokens
                    text: (anchor.lane + 1).toString().padStart(2, "0") + " / " + qsTranslate("Overview",
                                                                                              "%n albums",
                                                                                              "", anchor.albumCount)
                    cssPixelSize: 9
                    cssLineHeight: anchor.countLineHeight
                    font.letterSpacing: .54
                    color: anchor.tokens.muted
                }
            }
            onClicked: anchor.controller.expand(anchor.lane)
        }
        Item {
            x: anchor.labelWidth
            y: 1
            width: anchor.entryWidth
            height: parent.height - 1
            clip: true
            opacity: anchor.entryOpacity
            T.AbstractButton {
                id: enter
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                objectName: "overviewEnter-" + anchor.lane
                x: anchor.entryOffset
                width: 76
                height: parent.height
                enabled: anchor.expanded && anchor.interactive && anchor.controller.ready
                activeFocusOnTab: enabled
                Accessible.name: qsTranslate("Overview", "Enter %1 in standard view").arg(
                                     anchor.columnName)
                background: Rectangle {
                    color: "transparent"
                    border.width: enter.visualFocus ? 1 : 0
                    border.color: anchor.tokens.accent
                    Rectangle {
                        antialiasing: true
                        width: 1
                        height: parent.height
                        color: anchor.tokens.line
                    }
                }
                contentItem: ArchiveText {
                    tokens: anchor.tokens
                    text: qsTranslate("Overview", "Enter \u2192")
                    cssPixelSize: 11
                    color: anchor.tokens.accent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: anchor.controller.enter(anchor.lane)
                onEnabledChanged: {
                    if (!enabled && activeFocus && label.enabled)
                        label.forceActiveFocus(Qt.OtherFocusReason);
                }
            }
        }
    }
    onInteractiveChanged: {
        if (!interactive && (label.activeFocus || enter.activeFocus))
            anchor.returnFocusRequested();
    }
}
