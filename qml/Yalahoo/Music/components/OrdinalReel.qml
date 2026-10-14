pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property DesignTokens tokens
    property FrameClock clock: null
    property int value: 1
    property int digits: 2
    property int direction: 0
    property real cssPixelSize: 50
    property real letterSpacing: -3
    property color color: tokens.ink
    property font digitFont: MusicFonts.light
    baselineOffset: measure.baselineOffset
    implicitWidth: measure.width
    implicitHeight: cssPixelSize
    Accessible.role: Accessible.StaticText
    Accessible.name: value.toString().padStart(digits, "0")
    ArchiveText {
        id: measure
        visible: false
        tokens: root.tokens
        text: root.value.toString().padStart(root.digits, "0")
        cssPixelSize: root.cssPixelSize
        cssLineHeight: root.cssPixelSize
        font.family: root.digitFont.family
        font.styleName: root.digitFont.styleName
        font.weight: root.digitFont.weight
        font.letterSpacing: root.letterSpacing
    }
    OrdinalMotion {
        id: motion
        clock: root.clock
        direction: root.direction
        digits: root.digits
        value: root.value
        reducedMotion: root.tokens.reducedMotion
        active: root.visible && (!root.Window.window || root.Window.window.visible)
    }
    Row {
        height: root.cssPixelSize
        layer.enabled: root.visible
        layer.effect: ShaderEffect {
            property vector2d fadeEdges: Qt.vector2d(0, 0.12)
            fragmentShader: "qrc:/assets/shaders/edge-fade.frag.qsb"
        }
        spacing: 0
        Repeater {
            model: motion.faces.length
            delegate: Item {
                id: slot
                required property int index
                readonly property var modelData: motion.faces[index]
                width: upper.width
                height: root.cssPixelSize
                clip: true
                ArchiveText {
                    id: upper
                    tokens: root.tokens
                    text: slot.modelData.upper.toString()
                    color: root.color
                    cssPixelSize: root.cssPixelSize
                    cssLineHeight: root.cssPixelSize
                    font.family: root.digitFont.family
                    font.styleName: root.digitFont.styleName
                    font.weight: root.digitFont.weight
                    font.letterSpacing: root.letterSpacing
                    y: -slot.modelData.fraction * root.cssPixelSize
                }
                ArchiveText {
                    tokens: root.tokens
                    text: slot.modelData.lower.toString()
                    color: root.color
                    cssPixelSize: root.cssPixelSize
                    cssLineHeight: root.cssPixelSize
                    font.family: root.digitFont.family
                    font.styleName: root.digitFont.styleName
                    font.weight: root.digitFont.weight
                    font.letterSpacing: root.letterSpacing
                    y: (1 - slot.modelData.fraction) * root.cssPixelSize
                }
            }
        }
    }
}
