pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property OverviewController controller
    required property DesignTokens tokens
    required property Item sceneSource
    required property InputRouter inputRouter
    property string columnKind: qsTr("Genres")
    readonly property bool narrow: width <= 600
    readonly property real margin: narrow ? 22 : 40
    readonly property real labelWidth: narrow ? 116 : 160
    readonly property real labelLineHeight: Math.round(labelMetrics.ascent) + Math.round(
                                                labelMetrics.descent)
    readonly property real countLineHeight: Math.round(countMetrics.ascent) + Math.round(
                                                countMetrics.descent)
    readonly property real headingLineHeight: Math.round(headingMetrics.ascent) + Math.round(
                                                  headingMetrics.descent)
    readonly property real headingTitleLineHeight: Math.round(headingTitleMetrics.ascent)
                                                   + Math.round(headingTitleMetrics.descent)
    FontMetrics {
        id: labelMetrics
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: root.narrow ? 11 : 12
    }
    FontMetrics {
        id: countMetrics
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: 9
    }
    FontMetrics {
        id: headingMetrics
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: 16
    }
    FontMetrics {
        id: headingTitleMetrics
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: root.narrow ? 17 : 19
    }
    visible: controller.active || controller.chrome.shown
    Item {
        id: heading
        x: root.margin
        y: (root.narrow ? 115 : 135) + root.controller.chrome.offset
        opacity: root.controller.chrome.opacity
        ArchiveText {
            y: Math.round(headingMetrics.ascent) - Math.round(countMetrics.ascent)
            tokens: root.tokens
            text: qsTr("COLLECTION / OVERVIEW")
            cssPixelSize: 9
            font.letterSpacing: 2.07
            color: root.tokens.muted
        }
        ArchiveText {
            y: root.headingLineHeight + 9
            tokens: root.tokens
            text: qsTr("Browse collection by %1").arg(root.columnKind)
            cssPixelSize: root.narrow ? 17 : 19
            font.letterSpacing: cssPixelSize * .12
        }
        ArchiveText {
            y: root.headingLineHeight + 9 + root.headingTitleLineHeight + 11
            tokens: root.tokens
            text: qsTr(
                      "Click a column label to expand its entry. Enter to return to the standard view.")
            cssPixelSize: 11
            color: root.tokens.muted
        }
    }
    Repeater {
        model: root.controller.labels
        delegate: Item {
            id: anchor
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
            readonly property real cardHeight: 1 + (root.narrow ? 16 : 19) + root.labelLineHeight
                                               + 5 + root.countLineHeight
            objectName: "overviewAnchor-" + lane
            x: anchorX - root.labelWidth / 2
            y: anchorY - cardHeight
            width: root.labelWidth + entryWidth
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
                        hdrActive: root.tokens.hdrActive
                        objectName: "overviewBackdrop-" + anchor.lane
                        sceneSource: root.sceneSource
                        sceneX: anchor.x
                        sceneY: anchor.y + anchor.offset
                        labelWidth: root.labelWidth
                    }
                }
                Rectangle {
                    antialiasing: true
                    anchors.fill: parent
                    color: Qt.rgba(root.tokens.surface.r, root.tokens.surface.g,
                                   root.tokens.surface.b, .87)
                }
                Rectangle {
                    antialiasing: true
                    width: parent.width
                    height: 1
                    color: anchor.selectedColumn ? root.tokens.accent : root.tokens.line
                }
                Rectangle {
                    antialiasing: true
                    x: root.labelWidth / 2
                    y: parent.height
                    width: 1
                    height: 22
                    color: root.tokens.line
                }
                T.AbstractButton {
                    id: label
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    objectName: "overviewLabel-" + anchor.lane
                    y: 1
                    width: root.labelWidth
                    height: parent.height - 1
                    enabled: root.controller.ready && anchor.interactive
                    activeFocusOnTab: enabled
                    Accessible.name: qsTr("Expand %1 entry: %2").arg(root.columnKind).arg(
                                         anchor.columnName)
                    Accessible.description: anchor.expanded ? qsTr("Expanded") : qsTr("Collapsed")
                    ArchiveToolTip {
                        parent: label
                        tokens: root.tokens
                        text: anchor.columnName
                        visible: label.visible && label.hovered
                    }
                    background: Rectangle {
                        antialiasing: true
                        color: "transparent"
                        border.width: label.visualFocus ? 1 : 0
                        border.color: root.tokens.accent
                        anchors.margins: 3
                    }
                    contentItem: Item {
                        ArchiveText {
                            objectName: "overviewName-" + anchor.lane
                            x: root.narrow ? 8 : 12
                            y: root.narrow ? 8 : 10
                            width: parent.width - x * 2
                            tokens: root.tokens
                            text: anchor.columnName
                            elide: Text.ElideRight
                            cssPixelSize: root.narrow ? 11 : 12
                            cssLineHeight: root.labelLineHeight
                            font: root.tokens.regular
                        }
                        ArchiveText {
                            objectName: "overviewCount-" + anchor.lane
                            x: root.narrow ? 8 : 12
                            width: parent.width - x * 2
                            y: (root.narrow ? 8 : 10) + root.labelLineHeight + 5
                            tokens: root.tokens
                            text: (anchor.lane + 1).toString().padStart(2, "0") + " / " + qsTr("%n albums",
                                                                                               "", anchor.albumCount)
                            cssPixelSize: 9
                            cssLineHeight: root.countLineHeight
                            font.letterSpacing: .54
                            color: root.tokens.muted
                        }
                    }
                    onClicked: root.controller.expand(anchor.lane)
                }
                Item {
                    x: root.labelWidth
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
                        enabled: anchor.expanded && anchor.interactive && root.controller.ready
                        activeFocusOnTab: enabled
                        Accessible.name: qsTr("Enter %1 in standard view").arg(anchor.columnName)
                        background: Rectangle {
                            color: "transparent"
                            border.width: enter.visualFocus ? 1 : 0
                            border.color: root.tokens.accent
                            Rectangle {
                                antialiasing: true
                                width: 1
                                height: parent.height
                                color: root.tokens.line
                            }
                        }
                        contentItem: ArchiveText {
                            tokens: root.tokens
                            text: qsTr("Enter \u2192")
                            cssPixelSize: 11
                            color: root.tokens.accent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: root.controller.enter(anchor.lane)
                        onEnabledChanged: {
                            if (!enabled && activeFocus && label.enabled)
                                label.forceActiveFocus(Qt.OtherFocusReason);
                        }
                    }
                }
            }
            onInteractiveChanged: {
                if (!interactive && (label.activeFocus || enter.activeFocus))
                    returnButton.forceActiveFocus(Qt.OtherFocusReason);
            }
        }
    }
    ArchiveButton {
        id: returnButton
        objectName: "overviewReturn"
        tokens: root.tokens
        x: root.margin
        y: root.height - (root.narrow ? 50 : 77) - height + root.controller.chrome.offset
        opacity: root.controller.chrome.opacity
        enabled: root.controller.active && opacity > 0
        iconName: "overview-return"
        text: qsTr("Return to close view     V")
        cssPixelSize: 12
        height: 37
        onClicked: root.controller.setActive(false)
        Rectangle {
            antialiasing: true
            y: parent.height - 1
            width: parent.width
            height: 1
            color: root.tokens.line
        }
    }
    Row {
        x: root.width - root.margin - width
        y: root.height - (root.narrow ? 50 : 76) - height + root.controller.chrome.offset
        spacing: root.narrow ? 12 : 22
        opacity: root.controller.chrome.opacity
        enabled: root.controller.active && opacity > 0
        ArchiveButton {
            objectName: "overviewPrevious"
            tokens: root.tokens
            text: "\u2190"
            width: 36
            height: 32
            onClicked: root.inputRouter.lane(-1)
            Rectangle {
                antialiasing: true
                anchors.fill: parent
                color: "transparent"
                border.color: root.tokens.line
            }
        }
        ArchiveText {
            tokens: root.tokens
            visible: !root.narrow
            y: 8
            text: qsTr("\u2190 \u2192 Switch %1").arg(root.columnKind)
            cssPixelSize: 11
            color: root.tokens.muted
        }
        ArchiveButton {
            objectName: "overviewNext"
            tokens: root.tokens
            text: "\u2192"
            width: 36
            height: 32
            onClicked: root.inputRouter.lane(1)
            Rectangle {
                antialiasing: true
                anchors.fill: parent
                color: "transparent"
                border.color: root.tokens.line
            }
        }
    }
    Connections {
        target: root.controller
        function onFocusRequested(overview: bool) {
            if (overview)
                returnButton.forceActiveFocus(Qt.OtherFocusReason);
        }
    }
}
