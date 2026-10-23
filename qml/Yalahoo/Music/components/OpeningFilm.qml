// Composes the brand, orbit and statement layers from the opening controller snapshot.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property OpeningController controller
    required property DesignTokens tokens
    required property real markWidth
    required property real orbitWidth
    ShaderEffect {
        anchors.fill: parent
        opacity: root.controller.drawing.background
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
        opacity: root.controller.drawing.mark
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
        scale: root.controller.drawing.orbitScale
        opacity: root.controller.drawing.orbit
        visible: opacity > 0
        time: root.controller.time
        ink: root.tokens.ink
        accent: root.tokens.accent
    }
    Item {
        id: statement
        objectName: "openingStatement"
        readonly property real titleSize: root.width / root.height <= 1 ? Math.max(18, Math.min(
                                                                                       root.width
                                                                                       * 0.0575, 32)) :
                                                                          Math.max(23, Math.min(
                                                                                       root.width
                                                                                       * 0.0335, 48))
        x: 20
        width: root.width - 40
        height: titleSize * 1.5 + 10 + 24 + 13.5
        y: root.height * 0.48 - height / 2
        Item {
            width: parent.width
            height: (parent.titleSize * 1.5 + 10) * (1 - root.controller.drawing.titleClip / 100)
            clip: true
            ArchiveText {
                objectName: "openingSlogan"
                tokens: root.tokens
                anchors.horizontalCenter: parent.horizontalCenter
                y: 5 + root.controller.drawing.titleY
                opacity: root.controller.drawing.title
                text: qsTranslate("Opening", "One album at a time")
                cssPixelSize: statement.titleSize
                cssLineHeight: cssPixelSize * 1.5
                font.weight: 450
                font.letterSpacing: cssPixelSize * (root.width / root.height <= 1 ? 0.015 : 0.045)
            }
        }
        ArchiveText {
            tokens: root.tokens
            anchors.horizontalCenter: parent.horizontalCenter
            y: statement.titleSize * 1.5 + 10 + 24
            text: qsTranslate("Opening", "ONE ALBUM AT A TIME")
            cssPixelSize: 9
            cssLineHeight: 13.5
            font.letterSpacing: 2.7
            color: root.tokens.muted
            opacity: root.controller.drawing.subtitle
        }
    }
}
