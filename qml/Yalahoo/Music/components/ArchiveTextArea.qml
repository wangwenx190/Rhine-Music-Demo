pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

T.TextArea {
    id: control
    required property DesignTokens tokens
    implicitWidth: tokens.editorWidth
    property real cssLineHeight: tokens.editorLineHeight
    implicitHeight: Math.max(3, lineCount) * cssLineHeight + 2 * tokens.editorVerticalPadding
    padding: 0
    leftPadding: tokens.editorHorizontalPadding
    rightPadding: tokens.editorHorizontalPadding
    topPadding: tokens.editorVerticalPadding
    bottomPadding: tokens.editorVerticalPadding
    clip: true
    textFormat: TextEdit.PlainText
    textMargin: 0
    font: tokens.editorFont
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
        multiline: true
    }
    TextAreaFormat {
        document: control.textDocument
        lineHeight: control.cssLineHeight
    }
    Keys.onPressed: event => editorSurface.handleKey(event)
}
