// Composes the authored opening layers from one C++ timeline snapshot. Opacity and blur overlap
// deliberately; they do not create independent completion timers.

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

FocusScope {
    id: root
    required property Item sceneSource
    required property OpeningController controller
    required property DesignTokens tokens
    property string errorText: ""
    readonly property var drawing: controller.drawing
    readonly property real markWidth: Math.max(155, Math.min(width * 0.17, 235))
    readonly property real orbitWidth: width / height <= 1 ? Math.min(width * 0.86, 500) : Math.min(
                                                                 height * 0.66, width * 0.54, 520)
    readonly property size canvasSize: controller.film || controller.preparing ? Qt.size(Math.ceil(
                                                                                             Math.max(markWidth,
                                                                                                      orbitWidth)),
                                                                                         Math.ceil(
                                                                                             markWidth
                                                                                             * 145 / 310
                                                                                             + orbitWidth)) :
                                                                                 Qt.size(0, 0)
    function focusSkip(): void {
        skip.forceActiveFocus(Qt.OtherFocusReason);
    }
    visible: controller.active
    enabled: visible
    Accessible.role: Accessible.Dialog
    Accessible.name: controller.preparing ? qsTr("Preparing the music library") : controller.film
                                            ? qsTr("Music opening: one album at a time") : qsTr(
                                                  "Showing the music library")
    Keys.onPressed: event => {
        event.accepted = true;
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Return || event.key
                === Qt.Key_Enter)
            controller.skip();
        if (event.key === Qt.Key_Tab)
            skip.forceActiveFocus(Qt.TabFocusReason);
    }
    Loader {
        anchors.fill: parent
        active: root.controller.preparing || root.controller.film
        sourceComponent: OpeningFilm {
            width: root.width
            height: root.height
            tokens: root.tokens
            controller: root.controller
            markWidth: root.markWidth
            orbitWidth: root.orbitWidth
        }
    }
    T.Button {
        id: skip
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        objectName: "openingSkip"
        x: root.width - width - (root.width <= 700 ? 20 : 40)
        y: root.width <= 700 ? 23 : 33
        text: qsTr("Skip opening \u2197")
        padding: 10
        horizontalPadding: 14
        font.family: root.tokens.regular.family
        font.styleName: root.tokens.regular.styleName
        font.weight: root.tokens.regular.weight
        font.pixelSize: root.width <= 700 ? 11 : 12
        contentItem: ArchiveText {
            tokens: root.tokens
            text: skip.text
            cssPixelSize: skip.font.pixelSize
            cssLineHeight: cssPixelSize * 1.5
        }
        background: Item {
            OpeningBackdrop {
                hdrActive: root.tokens.hdrActive
                anchors.fill: parent
                sceneSource: root.sceneSource
                sceneX: skip.x
                sceneY: skip.y
                viewport: Qt.size(root.width, root.height)
                pageColor: root.tokens.page
                surfaceColor: root.tokens.surface
                paperAlpha: root.drawing.background
            }
            Rectangle {
                antialiasing: true
                anchors.fill: parent
                color: root.tokens.veil
                border.width: 1
                border.color: root.tokens.line
            }
            Rectangle {
                antialiasing: true
                x: -6
                y: -6
                width: parent.width + 12
                height: parent.height + 12
                color: "transparent"
                border.width: skip.activeFocus ? 2 : 0
                border.color: root.tokens.accent
            }
        }
        visible: root.controller.film || root.controller.preparing
        Keys.onReturnPressed: root.controller.skip()
        Keys.onEnterPressed: root.controller.skip()
        Keys.onEscapePressed: root.controller.skip()
        onClicked: root.controller.skip()
    }
    ArchiveText {
        tokens: root.tokens
        anchors.centerIn: parent
        visible: root.controller.preparing
        text: root.errorText.length > 0 ? qsTr("Graphics configuration unavailable: %1").arg(
                                              root.errorText) : qsTr(
                                              "Preparing the music library...")
        color: root.errorText.length > 0 ? root.tokens.accent : root.tokens.muted
        cssPixelSize: 12
    }
}
