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
    function stopCompare() {
        lighting.comparing = false;
    }
    function field(name: string, value: real) {
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
        text: "DEV · LIGHTING 光效调试 " + (root.expanded ? "−" : "+")
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
                        label: "0 原版"
                    },
                    {
                        value: "area",
                        label: "A 条形灯"
                    },
                    {
                        value: "guided",
                        label: "B 排光场"
                    },
                    {
                        value: "hybrid",
                        label: "C 混合"
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
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            cssPixelSize: 10
            color: root.tokens.muted
            wrapMode: Text.Wrap
            text: root.mode === "baseline" ? "保留原有环境光与封面照明，作为亮度对照。" : root.mode === "area" ? "条形灯照亮玻璃顶边与端面；封面不直接响应这盏灯。" :
                                                                                             root.mode
                                                                                             === "guided"
                                                                                             ? "整排高光保留；其他专辑亮度 100% 接近原版，降低可加强对比。" :
                                                                                               "用排光场控制明暗，再以条形灯补充壳体高光。"
        }
        Column {
            width: parent.width
            spacing: 0
            Repeater {
                model: [
                    {
                        key: "transitionDuration",
                        label: "光带切换时长",
                        min: .4,
                        max: 2.4,
                        step: .05,
                        suffix: " 秒"
                    },
                    {
                        key: "strength",
                        label: "光照强度",
                        min: 0,
                        max: 2,
                        step: .05,
                        suffix: "×"
                    },
                    {
                        key: "width",
                        label: root.mode === "area" ? "灯面宽度" : "光带宽度",
                        min: .6,
                        max: 2.5,
                        step: .05,
                        suffix: root.mode === "area" ? "×" : " 排"
                    },
                    {
                        key: "offset",
                        label: "光带位置",
                        min: -6,
                        max: 6,
                        step: .1,
                        suffix: " 排"
                    },
                    {
                        key: "fill",
                        label: "其他专辑亮度",
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
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            cssPixelSize: 10
            color: root.tokens.muted
            wrapMode: Text.Wrap
            text: "时长为 1× 下的值，同时受整体动画速度调节。专辑详情沿用原选中光。"
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
                text: pressed ? "原版 · 松开恢复" : "按住看原版"
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
                text: "重置"
                cssPixelSize: 11
                onClicked: root.lighting.resetSettings()
            }
        }
    }
}
