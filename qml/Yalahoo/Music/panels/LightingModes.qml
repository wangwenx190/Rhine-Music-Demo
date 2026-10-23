// Displays the bounded lighting modes and their explanation. Selection is a caller intent.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import "../components"

Column {
    id: root
    required property DesignTokens tokens
    required property string mode
    signal modeRequested(string mode)
    spacing: 10
    Grid {
        width: parent.width
        columns: 2
        spacing: 5
        Repeater {
            model: [
                {
                    value: "baseline",
                    label: qsTranslate("LightingPanel", "0 Original")
                },
                {
                    value: "area",
                    label: qsTranslate("LightingPanel", "A Strip light")
                },
                {
                    value: "guided",
                    label: qsTranslate("LightingPanel", "B Row field")
                },
                {
                    value: "hybrid",
                    label: qsTranslate("LightingPanel", "C Combined")
                }
            ]
            delegate: T.Button {
                id: modeButton
                required property var modelData
                width: (parent.width - 5) / 2
                text: modelData.label
                implicitHeight: 30.5
                padding: 6
                leftPadding: 10
                rightPadding: 10
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                checked: root.mode === modelData.value
                onClicked: {
                    root.modeRequested(modelData.value);
                }
                Accessible.name: text
                background: Rectangle {
                    antialiasing: true
                    radius: 2
                    color: modeButton.checked ? Qt.rgba(root.tokens.accent.r, root.tokens.accent.g,
                                                        root.tokens.accent.b, .11) : "transparent"
                    border.color: modeButton.activeFocus ? root.tokens.accent : modeButton.checked
                                                           ? Qt.tint(root.tokens.line, Qt.rgba(
                                                                         root.tokens.accent.r,
                                                                         root.tokens.accent.g,
                                                                         root.tokens.accent.b,
                                                                         .65)) : root.tokens.line
                }
                contentItem: ArchiveText {
                    tokens: root.tokens
                    text: modeButton.text
                    cssPixelSize: 11
                    cssLineHeight: 16.5
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
    PanelDescription {
        tokens: root.tokens
        cssPixelSize: 10
        text: root.mode === "baseline" ? qsTranslate("LightingPanel",
                                                     "Retain the original ambient and cover lighting as a brightness reference.") :
                                         root.mode === "area" ? qsTranslate("LightingPanel",
                                                                            "The strip light illuminates the top and ends of the case; covers do not respond directly to it.") :
                                                                root.mode === "guided" ? qsTranslate(
                                                                                             "LightingPanel",
                                                                                             "Retain highlights across the row. Other albums at 100% approximate the original; lower values increase contrast.") :
                                                                                         qsTranslate(
                                                                                             "LightingPanel",
                                                                                             "Use the row field for brightness, then add case highlights with the strip light.")
    }
}
