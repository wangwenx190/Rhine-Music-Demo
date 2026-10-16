pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T
import Yalahoo.Music

T.ComboBox {
    id: control
    required property DesignTokens tokens
    readonly property var hostWindow: control.Window.window
    property var disabledValue: undefined
    readonly property real optionsWidth: {
        let result = 0;
        const rows = control.model;
        for (let i = 0; rows && i < rows.length; ++i)
            result = Math.max(result, metrics.advanceWidth(String(rows[i][control.textRole]
                                                                  ?? rows[i])));
        return result;
    }
    FontMetrics {
        id: metrics
        font.family: control.tokens.regular.family
        font.styleName: control.tokens.regular.styleName
        font.weight: control.tokens.regular.weight
        font.pixelSize: 11
    }
    implicitWidth: Math.max(90, Math.ceil(optionsWidth) + 40)
    implicitHeight: 28
    padding: 5
    leftPadding: 10
    rightPadding: 28
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : .38
    background: Rectangle {
        color: control.tokens.surface
        border.color: control.activeFocus || control.hovered ? control.tokens.accent :
                                                               control.tokens.line
        radius: 0
    }
    contentItem: ArchiveText {
        tokens: control.tokens
        text: control.displayText
        cssPixelSize: 11
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Shape {
        x: control.width - width - 10
        y: (control.height - height) / 2
        width: 9
        height: 5
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: control.tokens.ink
            strokeWidth: 1
            capStyle: ShapePath.RoundCap
            fillColor: "transparent"
            startX: 0
            startY: 0
            PathLine {
                x: 4.5
                y: 4.5
            }
            PathLine {
                x: 9
                y: 0
            }
        }
    }
    delegate: T.ItemDelegate {
        id: option
        required property var model
        required property int index
        width: control.width - 2
        height: Math.max(30, implicitContentHeight + 12)
        padding: 6
        leftPadding: 10
        hoverEnabled: true
        enabled: control.disabledValue === undefined || model[control.valueRole]
                 !== control.disabledValue
        opacity: enabled ? 1 : .38
        highlighted: control.highlightedIndex === index
        background: Rectangle {
            color: option.highlighted || option.hovered ? control.tokens.veil :
                                                          control.tokens.surface

            border.color: option.visualFocus ? control.tokens.accent : "transparent"
        }
        contentItem: ArchiveText {
            tokens: control.tokens
            text: String(option.model[control.textRole] ?? option.model.modelData ?? "")
            cssPixelSize: 11
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        Accessible.name: String(model[control.textRole] ?? model.modelData ?? "")
    }
    popup: T.Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, Math.max(32, Math.min(264,
                                                                                                        control.hostWindow
                                                                                                        ? control.hostWindow.height
                                                                                                          - 16 : 264)))
        margins: 8
        padding: 1
        focus: false
        popupType: T.Popup.Item
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent
        enter: null
        exit: null
        background: Rectangle {
            color: control.tokens.surface
            border.color: control.tokens.line
        }
        contentItem: ListView {
            implicitHeight: contentHeight
            clip: true
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ArchiveScrollBar {
                tokens: control.tokens
            }
        }
    }
}
