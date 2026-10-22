pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music
import "../components"

Column {
    id: root
    objectName: "lightingPanel"
    required property DesignTokens tokens
    required property SelectionLighting lighting
    property bool compact: false
    property bool expanded: !compact
    readonly property string mode: lighting.settings.mode
    spacing: 10
    function stopCompare(): void {
        lighting.comparing = false;
    }
    function field(name: string, value: real): void {
        const patch = {};
        patch[name] = value;
        lighting.configure(patch);
    }
    onVisibleChanged: {
        if (!visible)
            stopCompare();
    }
    onExpandedChanged: {
        if (!expanded)
            stopCompare();
    }
    Component.onDestruction: stopCompare()
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window.active)
                root.stopCompare();
        }
        function onVisibleChanged() {
            if (!root.Window.window.visible)
                root.stopCompare();
        }
    }
    ArchiveButton {
        tokens: root.tokens
        text: qsTr("DEV \u00b7 LIGHTING %1").arg(root.expanded ? "\u2212" : "+")
        cssPixelSize: 11
        onClicked: root.expanded = !root.expanded
    }
    Column {
        width: parent.width
        spacing: 10
        visible: root.expanded
        Grid {
            width: parent.width
            columns: 2
            spacing: 5
            Repeater {
                model: [
                    {
                        value: "baseline",
                        label: qsTr("0 Original")
                    },
                    {
                        value: "area",
                        label: qsTr("A Strip light")
                    },
                    {
                        value: "guided",
                        label: qsTr("B Row field")
                    },
                    {
                        value: "hybrid",
                        label: qsTr("C Combined")
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
                        root.stopCompare();
                        root.lighting.configure({
                                                    mode: modelData.value
                                                });
                    }
                    Accessible.name: text
                    background: Rectangle {
                        antialiasing: true
                        radius: 2
                        color: modeButton.checked ? Qt.rgba(root.tokens.accent.r,
                                                            root.tokens.accent.g,
                                                            root.tokens.accent.b, .11) :
                                                    "transparent"
                        border.color: modeButton.activeFocus ? root.tokens.accent :
                                                               modeButton.checked ? Qt.tint(
                                                                                        root.tokens.line,
                                                                                        Qt.rgba(root.tokens.accent.r,
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
            text: root.mode === "baseline" ? qsTr(
                                                 "Retain the original ambient and cover lighting as a brightness reference.") :
                                             root.mode === "area" ? qsTr(
                                                                        "The strip light illuminates the top and ends of the case; covers do not respond directly to it.") :
                                                                    root.mode === "guided" ? qsTr(
                                                                                                 "Retain highlights across the row. Other albums at 100% approximate the original; lower values increase contrast.") :
                                                                                             qsTr("Use the row field for brightness, then add case highlights with the strip light.")
        }
        Column {
            width: parent.width
            spacing: 0
            Repeater {
                model: [
                    {
                        key: "transitionDuration",
                        label: qsTr("Light transition duration"),
                        min: .4,
                        max: 2.4,
                        step: .05,
                        suffix: qsTr(" seconds")
                    },
                    {
                        key: "strength",
                        label: qsTr("Light intensity"),
                        min: 0,
                        max: 2,
                        step: .05,
                        suffix: "\u00d7"
                    },
                    {
                        key: "width",
                        label: root.mode === "area" ? qsTr("Light surface width") : qsTr(
                                                          "Light band width"),
                        min: .6,
                        max: 2.5,
                        step: .05,
                        suffix: root.mode === "area" ? "\u00d7" : qsTr(" rows")
                    },
                    {
                        key: "offset",
                        label: qsTr("Light band position"),
                        min: -6,
                        max: 6,
                        step: .1,
                        suffix: qsTr(" rows")
                    },
                    {
                        key: "fill",
                        label: qsTr("Other album brightness"),
                        min: 0,
                        max: 1,
                        step: .05,
                        suffix: "%"
                    }
                ]
                delegate: SettingRange {
                    required property var modelData
                    stacked: true
                    captionSize: 10
                    objectName: "lighting" + modelData.key
                    tokens: root.tokens
                    label: modelData.label
                    minimum: modelData.min
                    maximum: modelData.max
                    step: modelData.step
                    suffix: modelData.suffix
                    displayFactor: modelData.key === "fill" ? 100 : 1
                    decimals: modelData.key === "fill" ? 0 : 2
                    value: root.lighting.settings[modelData.key]
                    enabled: root.mode !== "baseline" && (modelData.key !== "fill" || root.mode
                                                          === "guided")
                    onMoved: value => root.field(modelData.key, value)
                }
            }
        }
        PanelDescription {
            tokens: root.tokens
            cssPixelSize: 10
            text: qsTr(
                      "Duration is measured at 1x and follows the overall animation speed. Album details retain the original selection light.")
        }
        Row {
            spacing: 12
            ArchiveButton {
                id: compare
                tokens: root.tokens
                framed: true
                cssPixelSize: 10
                objectName: "lightingCompare"
                enabled: root.mode !== "baseline"
                text: pressed ? qsTr("Original \u00b7 Release to restore") : qsTr(
                                    "Hold for original")
                onPressedChanged: root.lighting.comparing = pressed
                onActiveFocusChanged: {
                    if (!activeFocus)
                        root.stopCompare();
                }
                Accessible.name: text
            }
            ArchiveButton {
                objectName: "lightingReset"
                tokens: root.tokens
                text: qsTr("Reset")
                cssPixelSize: 11
                onClicked: root.lighting.resetSettings()
            }
        }
    }
}
