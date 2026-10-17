pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

Item {
    id: root
    required property RulerController controller
    required property DesignTokens tokens
    objectName: "productionRuler"
    width: controller ? controller.width : 0
    height: controller && controller.narrow ? 21 : 30
    clip: true
    RulerInk {
        objectName: "productionRulerInk"
        visible: root.width > 0
        width: root.controller && root.controller.narrow ? 96 : 130
        height: root.height
        controller: root.controller
        ink: root.tokens.ink
        accent: root.tokens.accent
    }
    Item {
        anchors.fill: parent
        clip: true
        Repeater {
            id: tickPool
            model: 16
            delegate: T.Button {
                id: button
                hoverEnabled: true
                required property int index
                readonly property var slotState: root.controller
                                                 ? root.controller.slotStates[index] : ({})
                objectName: "rulerSlot" + index
                x: (slotState.x || 0) - (root.controller && root.controller.narrow ? 3 : 4)
                width: root.controller && root.controller.narrow ? 8 : 11
                height: root.height
                enabled: root.visible && Boolean(slotState.enabled)
                visible: enabled
                focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
                Accessible.name: qsTr("Select album %1").arg(slotState.title || "")
                Accessible.checkable: true
                Accessible.checked: Boolean(slotState.active)
                contentItem: Item {}
                background: Rectangle {
                    antialiasing: true
                    color: "transparent"
                    border.width: button.visualFocus ? 1 : 0
                    border.color: root.tokens.accent
                }
                onHoveredChanged: root.controller.hover(index, hovered, visualFocus)
                onVisualFocusChanged: root.controller.hover(index, hovered, visualFocus)
                onEnabledChanged: {
                    if (!enabled && activeFocus) {
                        root.restoreFocus();
                    }
                }
                onClicked: root.controller.click(index)
            }
        }
    }
    function restoreFocus(): void {
        let first = null;
        for (let i = 0; i < 16; ++i) {
            const item = tickPool.itemAt(i);
            if (item && item.enabled) {
                if (!first) {
                    first = item;
                }
                if (root.controller.slotStates[i].active) {
                    item.forceActiveFocus(Qt.TabFocusReason);
                    return;
                }
            }
        }
        if (first) {
            first.forceActiveFocus(Qt.TabFocusReason);
        }
    }
}
