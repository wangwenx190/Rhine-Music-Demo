pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property string label
    required property var choices
    required property var selected
    signal chosen(var value)
    width: parent ? parent.width : 300
    implicitHeight: Math.max(caption.implicitHeight, choice.implicitHeight)
    function focusControl(): void {
        choice.forceActiveFocus();
    }
    ArchiveText {
        id: caption
        tokens: root.tokens
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, parent.width - choice.width - 20)
        text: root.label
        cssPixelSize: 12
        wrapMode: Text.Wrap
    }
    ArchiveComboBox {
        id: choice
        tokens: root.tokens
        objectName: root.objectName + "Control"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(choice.implicitWidth, parent.width * .52)
        model: root.choices
        textRole: "label"
        valueRole: "value"
        currentValue: root.selected
        onActivated: root.chosen(currentValue)
        Accessible.name: root.label
        disabledValue: "custom"
    }
}
