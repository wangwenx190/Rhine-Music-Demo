pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.TextField {
    id: control
    required property DesignTokens tokens
    implicitWidth: tokens.editorWidth
    property real cssLineHeight: tokens.editorLineHeight
    implicitHeight: cssLineHeight + 2 * tokens.editorVerticalPadding
    padding: 0
    leftPadding: tokens.editorHorizontalPadding
    rightPadding: tokens.editorHorizontalPadding
    topPadding: tokens.editorVerticalPadding
    bottomPadding: tokens.editorVerticalPadding
    clip: true
    font: tokens.editorFont
    verticalAlignment: TextInput.AlignVCenter
    color: tokens.ink
    placeholderTextColor: tokens.muted
    selectionColor: tokens.accent
    selectedTextColor: tokens.surface
    selectByMouse: true
    renderType: Text.CurveRendering
    opacity: enabled ? 1 : tokens.disabledControlOpacity
    background: ArchiveEditorSurface {
        id: editorSurface
        editor: control
        multiline: false
    }
    Keys.onPressed: event => editorSurface.handleKey(event)
}
