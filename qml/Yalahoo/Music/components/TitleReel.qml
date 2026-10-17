pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property DesignTokens tokens
    property FrameClock clock: null
    property string text: ""
    property font titleFont: tokens.bold
    property real pixelSize: 28
    property real budget: 300
    property bool narrow: false
    property bool transport: false
    property real bottomPadding: 0
    property TitleReelMotion sharedController: null
    property bool quickTextCandidate: false
    readonly property TitleReelMotion controller: sharedController || motion
    readonly property real naturalWidth: controller.naturalWidth
    implicitHeight: controller.layoutHeight + bottomPadding
    height: implicitHeight
    clip: true
    Accessible.role: Accessible.StaticText
    Accessible.name: text
    Accessible.ignored: transport
    ArchiveToolTip {
        parent: root
        tokens: root.tokens
        text: root.text
        visible: root.visible && hover.hovered && root.text.length > 0
    }
    HoverHandler {
        id: hover
    }
    TitleReelMotion {
        id: motion
        clock: root.clock
        text: root.text
        font: root.titleFont
        subtitleFont: root.tokens.demibold
        pixelSize: root.pixelSize
        availableWidth: root.width
        budget: root.budget
        narrow: root.narrow
        transport: root.transport
        dpr: root.Window.window ? root.Window.window.devicePixelRatio : 1
        reducedMotion: root.tokens.reducedMotion
        active: !root.sharedController && root.visible && (root.tokens.offscreenRendering ||
                                                           !root.Window.window
                                                           || root.Window.window.visible)
    }
    function layerSizes(): var {
        const sizes = [];
        for (let i = 0; i < cells.count; ++i) {
            const cell = cells.itemAt(i);
            if (cell && cell.layer.enabled)
                sizes.push({
                               width: cell.layer.textureSize.width,
                               height: cell.layer.textureSize.height
                           });
        }
        return sizes;
    }
    Repeater {
        id: cells
        model: root.controller
        delegate: Item {
            id: cell
            required property ReelSlot slotObject
            readonly property ReelSlot slot: slotObject
            x: slot ? slot.positionX - slot.padding : 0
            y: slot ? slot.positionY : 0
            width: slot ? slot.capacityWidth : 0
            height: slot ? slot.capacityHeight : 0
            visible: Boolean(slot) && slot.shown
            opacity: slot ? slot.opacity : 1
            layer.enabled: visible && root.visible && (root.tokens.offscreenRendering ||
                                                       !root.Window.window
                                                       || root.Window.window.visible)

            layer.smooth: true
            layer.textureSize: Qt.size(Math.ceil(width * root.controller.dpr), Math.ceil(height
                                                                                         * root.controller.dpr))

            layer.effect: ShaderEffect {
                readonly property real sourceHeight: cell.height
                readonly property real textureHeight: Math.ceil(cell.height * root.controller.dpr)
                readonly property real blurAmount: root.transport && cell.slot
                                                   ? cell.slot.blurOpacity : 0
                readonly property real viewportHeight: cell.slot ? cell.slot.viewportHeight : 0
                readonly property real fadeHeight: cell.slot ? cell.slot.pixelSize * (
                                                                   root.transport ? .08 : .02) : 0
                fragmentShader: "qrc:/assets/shaders/reel-slot.frag.qsb"
            }
            ReelInk {
                anchors.fill: parent
                slot: cell.slot
                visible: !root.quickTextCandidate && Boolean(cell.slot) && cell.slot.nativeInk
                color: cell.slot && cell.slot.translation ? root.tokens.muted : root.tokens.ink
            }
            Repeater {
                model: cell.slot ? cell.slot.faceCapacity : 0
                delegate: Item {
                    id: face
                    required property int index
                    readonly property var faceData: cell.slot && index < cell.slot.faceValues.length
                                                    ? cell.slot.faceValues[index] : null
                    x: cell.slot ? cell.slot.padding : 0
                    y: visible ? faceData.offset - (cell.slot ? cell.slot.rollPosition : 0) : 0
                    width: cell.width
                    height: faceData ? faceData.height : 0
                    clip: true
                    visible: Boolean(faceData) && (root.quickTextCandidate || faceData.colored)
                    Loader {
                        active: face.visible
                        sourceComponent: Text {
                            antialiasing: true
                            text: face.faceData ? face.faceData.text : ""
                            font: face.faceData ? face.faceData.font : root.titleFont
                            renderType: Text.CurveRendering
                            Accessible.ignored: true
                            color: cell.slot && cell.slot.translation ? root.tokens.muted :
                                                                        root.tokens.ink

                            y: face.faceData ? face.faceData.baseline - baselineOffset
                                               * face.faceData.scale : 0
                            transform: Scale {
                                xScale: face.faceData ? face.faceData.scale : 1
                                yScale: face.faceData ? face.faceData.scale : 1
                            }
                        }
                    }
                }
            }
        }
    }
}
