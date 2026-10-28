// Projects bounded C++ label slots into self-drawn cards. The first click expands an entry
// action; the second click commits column navigation.

pragma ComponentBehavior: Bound
import QtQuick
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
        delegate: OverviewLabel {
            controller: root.controller
            tokens: root.tokens
            sceneSource: root.sceneSource
            narrow: root.narrow
            labelWidth: root.labelWidth
            labelLineHeight: root.labelLineHeight
            countLineHeight: root.countLineHeight
            columnKind: root.columnKind
            onReturnFocusRequested: returnButton.forceActiveFocus(Qt.OtherFocusReason)
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
            Accessible.name: qsTr("Previous column")
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
            Accessible.name: qsTr("Next column")
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
