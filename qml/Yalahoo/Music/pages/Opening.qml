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
    function focusSkip() {
        skip.forceActiveFocus(Qt.OtherFocusReason);
    }
    visible: controller.active
    enabled: visible
    Accessible.role: Accessible.Dialog
    Accessible.name: controller.preparing ? "正在准备音乐库" : controller.film ? "音乐开场：一张一张，慢慢听" :
                                                                          "正在显示音乐库"
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
        sourceComponent: Item {
            ShaderEffect {
                anchors.fill: parent
                opacity: root.drawing.background
                property color pageColor: root.tokens.page
                property color surfaceColor: root.tokens.surface
                fragmentShader: "qrc:/assets/shaders/opening-paper.frag.qsb"
            }
            Item {
                id: mark
                objectName: "openingMark"
                width: root.markWidth
                height: width * 145 / 310 + 17 + 15
                x: (root.width - width) / 2
                y: root.height * 0.48 - height / 2
                opacity: root.drawing.mark
                visible: opacity > 0
                OpeningInk {
                    objectName: "openingBrandInk"
                    width: parent.width
                    height: parent.width * 145 / 310
                    time: root.controller.time
                    ink: root.tokens.ink
                    accent: root.tokens.accent
                }
                ArchiveText {
                    tokens: root.tokens
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.width * 145 / 310 + 17
                    text: root.tokens.displayName
                    cssPixelSize: 10
                    cssLineHeight: 15
                    font.weight: Font.DemiBold
                    font.letterSpacing: 3
                }
            }
            OpeningInk {
                objectName: "openingOrbitInk"
                orbit: true
                width: root.orbitWidth
                height: width
                x: (root.width - width) / 2
                y: root.height * 0.48 - height / 2
                scale: root.drawing.orbitScale
                opacity: root.drawing.orbit
                visible: opacity > 0
                time: root.controller.time
                ink: root.tokens.ink
                accent: root.tokens.accent
            }
            Item {
                id: statement
                objectName: "openingStatement"
                readonly property real titleSize: root.width / root.height <= 1 ? Math.max(18,
                                                                                           Math.min(root.width
                                                                                                    * 0.0575, 32)) :
                                                                                  Math.max(23,
                                                                                           Math.min(root.width
                                                                                                    * 0.0335, 48))
                x: 20
                width: root.width - 40
                height: titleSize * 1.5 + 10 + 24 + 13.5
                y: root.height * 0.48 - height / 2
                Item {
                    width: parent.width
                    height: (parent.titleSize * 1.5 + 10) * (1 - root.drawing.titleClip / 100)
                    clip: true
                    ArchiveText {
                        objectName: "openingSlogan"
                        tokens: root.tokens
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 5 + root.drawing.titleY
                        opacity: root.drawing.title
                        text: "一张一张，慢慢听"
                        cssPixelSize: statement.titleSize
                        cssLineHeight: cssPixelSize * 1.5
                        font.weight: 450
                        font.letterSpacing: cssPixelSize * (root.width / root.height <= 1 ? 0.015 :
                                                                                            0.045)
                    }
                }
                ArchiveText {
                    tokens: root.tokens
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: statement.titleSize * 1.5 + 10 + 24
                    text: "ONE ALBUM AT A TIME"
                    cssPixelSize: 9
                    cssLineHeight: 13.5
                    font.letterSpacing: 2.7
                    color: root.tokens.muted
                    opacity: root.drawing.subtitle
                }
            }
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
        text: "跳过开场 ↗"
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
                anchors.fill: parent
                color: root.tokens.veil
                border.width: 1
                border.color: root.tokens.line
            }
            Rectangle {
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
        text: root.errorText.length > 0 ? "图形配置不可用：" + root.errorText : "正在准备音乐库…"
        color: root.errorText.length > 0 ? root.tokens.accent : root.tokens.muted
        cssPixelSize: 12
    }
}
