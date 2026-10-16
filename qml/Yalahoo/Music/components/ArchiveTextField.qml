pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.TextField {
    id: control
    required property DesignTokens tokens
    implicitWidth: 200
    property real cssLineHeight: 20.4
    implicitHeight: cssLineHeight + 26
    padding: 0
    leftPadding: 14
    rightPadding: 14
    topPadding: 13
    bottomPadding: 13
    clip: true
    font.family: tokens.regular.family
    font.styleName: tokens.regular.styleName
    font.weight: tokens.regular.weight
    font.pixelSize: 12
    verticalAlignment: TextInput.AlignVCenter
    color: tokens.ink
    placeholderTextColor: tokens.muted
    selectionColor: tokens.accent
    selectedTextColor: tokens.surface
    selectByMouse: true
    renderType: Text.CurveRendering
    opacity: enabled ? 1 : .38
    background: Rectangle {
        color: control.tokens.surface
        border.color: control.activeFocus ? control.tokens.accent : control.tokens.line
        radius: 0
    }
    Text {
        x: control.leftPadding
        y: control.topPadding
        width: Math.max(0, control.width - control.leftPadding - control.rightPadding)
        height: Math.max(0, control.height - control.topPadding - control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        visible: control.length === 0 && control.preeditText.length === 0
        renderType: Text.CurveRendering
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
    }
    ArchiveEditMenu {
        id: editMenu
        parent: control
        editor: control
        tokens: control.tokens
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            editMenu.x = eventPoint.position.x;
            editMenu.y = eventPoint.position.y;
            editMenu.open();
        }
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers
                                          === Qt.ShiftModifier)) {
            editMenu.x = control.cursorRectangle.x;
            editMenu.y = control.cursorRectangle.y + control.cursorRectangle.height;
            editMenu.open();
            event.accepted = true;
        } else
            event.accepted = false;
    }
}
