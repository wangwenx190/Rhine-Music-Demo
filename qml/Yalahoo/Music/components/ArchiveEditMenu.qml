pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.Popup {
    id: menu
    objectName: "editorMenu"
    required property DesignTokens tokens
    required property var editor
    property int actionIndex: -1
    function focusAction(direction: int) {
        for (let i = 0; i < actions.count; ++i) {
            actionIndex = (actionIndex + direction + actions.count) % actions.count;
            const button = actions.itemAt(actionIndex);
            if (button && button.enabled) {
                button.forceActiveFocus();
                return;
            }
        }
    }
    onOpened: {
        actionIndex = -1;
        focusAction(1);
    }
    width: 140
    height: contentItem.implicitHeight + 12
    padding: 6
    margins: 8
    focus: true
    popupType: T.Popup.Item
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    enter: null
    exit: null
    background: Rectangle {
        color: menu.tokens.surface
        border.color: menu.tokens.line
    }
    contentItem: Column {
        Keys.onDownPressed: menu.focusAction(1)
        Keys.onUpPressed: menu.focusAction(-1)
        Repeater {
            id: actions
            model: [
                {
                    label: "剪切",
                    action: "cut"
                },
                {
                    label: "复制",
                    action: "copy"
                },
                {
                    label: "粘贴",
                    action: "paste"
                },
                {
                    label: "全选",
                    action: "selectAll"
                }
            ]
            delegate: ArchiveButton {
                required property var modelData
                objectName: "editAction" + modelData.action
                width: parent.width
                tokens: menu.tokens
                cssPixelSize: 11
                text: modelData.label
                Accessible.role: Accessible.MenuItem
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                enabled: modelData.action === "paste" ? menu.editor.canPaste &&
                                                        !menu.editor.readOnly : modelData.action
                                                        === "selectAll" ? menu.editor.length > 0 :
                                                                          menu.editor.selectedText.length
                                                                          > 0 && (modelData.action
                                                                                  !== "cut" ||
                                                                                  !menu.editor.readOnly)
                onClicked: {
                    menu.editor[modelData.action]();
                    menu.close();
                    menu.editor.forceActiveFocus();
                }
            }
        }
    }
}
