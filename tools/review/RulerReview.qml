pragma ComponentBehavior: Bound
import QtQuick
import QtCanvas2D
import Yalahoo.Music
import "components"

Item {
    id: root
    required property RulerController controller
    property bool night: false
    property int mode: 0
    property int marker: 0
    property real physicalDpr: 1
    property int jsPaints: 0
    property real jsMilliseconds: 0
    property bool surface: true
    property bool rebuilding: false
    readonly property real tick: controller.narrow ? 2 : 3
    readonly property real gap: controller.narrow ? 6 : 8
    readonly property real rulerHeight: controller.narrow ? 21 : 30
    property bool overflow: true
    width: 136
    height: 36
    DesignTokens {
        id: tokens
        night: root.night
        reducedMotion: true
    }
    Rectangle {
        anchors.fill: parent
        color: tokens.page
    }
    Rectangle {
        x: 135
        width: 1
        height: 1
        color: root.marker % 2 ? "red" : "blue"
    }
    Ruler {
        x: 2
        y: 2
        controller: root.controller
        tokens: tokens
        visible: root.mode === 0
    }
    function rebuild() {
        rebuilding = true;
        Qt.callLater(finishRebuild);
    }
    function finishRebuild() {
        rebuilding = false;
    }
    Connections {
        target: root.Window.window
        function onSceneGraphInvalidated() {
            root.surface = false;
        }
        function onSceneGraphInitialized() {
            root.surface = true;
        }
        function onVisibleChanged() {
            root.rebuild();
        }
    }
    Item {
        x: 2
        y: 2
        width: root.controller.width
        height: root.rulerHeight
        clip: true
        Loader {
            width: root.controller.narrow ? 96 : 130
            height: root.rulerHeight
            active: root.mode === 1 && root.surface && !root.rebuilding && root.Window.window
                    && root.Window.window.visible && root.controller.width > 0
            sourceComponent: Component {
                Canvas2D {
                    id: canvas
                    fillColor: "transparent"
                    sampleCount: 1
                    alphaBlending: true
                    function invalidate() {
                        if (available) {
                            requestPaint();
                        }
                    }
                    Component.onCompleted: Qt.callLater(invalidate)
                    onAvailableChanged: Qt.callLater(invalidate)
                    Connections {
                        target: root.controller
                        function onChanged() {
                            Qt.callLater(canvas.invalidate);
                        }
                    }
                    onPaint: {
                        const start = Date.now();
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.resetClipping();
                        ctx.antialias = 1 / root.physicalDpr;
                        const rulerWidth = root.controller.width;
                        for (const mark of root.controller.slotStates) {
                            if (mark.alpha <= 0 || mark.x + root.tick <= 0 || mark.x
                                    >= rulerWidth) {

                                continue;
                            }
                            ctx.globalAlpha = mark.alpha;
                            const color = mark.hovering && !mark.active ? tokens.accent :
                                                                          tokens.ink;


                            if (root.overflow) {
                                const fade = ctx.createLinearGradient(0, 0, rulerWidth, 0);
                                fade.addColorStop(0, Qt.rgba(color.r, color.g, color.b, 0));
                                fade.addColorStop(root.gap / rulerWidth, color);
                                fade.addColorStop(1 - root.gap / rulerWidth, color);
                                fade.addColorStop(1, Qt.rgba(color.r, color.g, color.b, 0));
                                ctx.fillStyle = fade;
                            } else {
                                ctx.fillStyle = color;
                            }
                            ctx.beginPath();
                            ctx.rect(mark.x, (height - mark.height) / 2, root.tick, mark.height);
                            ctx.fill();
                        }
                        root.jsPaints++;
                        root.jsMilliseconds += Date.now() - start;
                    }
                }
            }
        }
    }
    Item {
        x: 2
        y: 2
        width: root.controller.width
        height: root.rulerHeight
        visible: root.mode === 2
        clip: true
        layer.enabled: visible && root.overflow
        layer.effect: ShaderEffect {
            property vector2d fadeEdges: Qt.vector2d(root.gap / root.controller.width, 0)
            fragmentShader: "qrc:/assets/shaders/edge-fade.frag.qsb"
        }
        Repeater {
            model: 16
            delegate: Rectangle {
                required property int index
                readonly property var mark: root.controller.slotStates[index]
                x: mark.x
                y: (root.rulerHeight - mark.height) / 2
                width: root.tick
                height: mark.height
                opacity: mark.alpha
                color: mark.hovering && !mark.active ? tokens.accent : tokens.ink
                antialiasing: true
            }
        }
    }
}
