// Common decoration for the two Qt template editors; their text/IME behavior stays in Templates.
pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: root
    required property var editor
    property bool multiline: false
    antialiasing: true
    color: editor.tokens.surface
    border.color: editor.activeFocus ? editor.tokens.accent : editor.tokens.line
    radius: 0
    Text {
        antialiasing: true
        x: root.editor.leftPadding
        y: root.editor.topPadding
        width: Math.max(0, root.editor.width - root.editor.leftPadding - root.editor.rightPadding)
        height: Math.max(0, root.editor.height - root.editor.topPadding - root.editor.bottomPadding)
        text: root.editor.placeholderText
        font: root.editor.font
        color: root.editor.placeholderTextColor
        visible: root.editor.length === 0 && root.editor.preeditText.length === 0
        renderType: Text.CurveRendering
        elide: Text.ElideRight
        wrapMode: root.multiline ? Text.Wrap : Text.NoWrap
    }
    ArchiveEditMenu {
        id: editMenu
        parent: root.editor
        editor: root.editor
        tokens: root.editor.tokens
    }
    TapHandler {
        parent: root.editor
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            editMenu.x = eventPoint.position.x;
            editMenu.y = eventPoint.position.y;
            editMenu.open();
        }
    }
    function handleKey(event) {
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers
                                          === Qt.ShiftModifier)) {
            editMenu.x = root.editor.cursorRectangle.x;
            editMenu.y = root.editor.cursorRectangle.y + root.editor.cursorRectangle.height;
            editMenu.open();
            event.accepted = true;
        } else
            event.accepted = false;
    }
}
