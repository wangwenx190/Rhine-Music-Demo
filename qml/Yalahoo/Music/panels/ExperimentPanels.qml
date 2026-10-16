pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "../components"

Rectangle {
    id: root
    objectName: "experimentPanels"
    required property DesignTokens tokens
    required property SettingsController settings
    required property SelectionLighting lighting
    required property SceneItem sceneSource
    property bool navigationInputOwner: true
    property bool diagnostics: false
    property bool compact: false
    signal closeRequested
    color: tokens.surface
    border.color: tokens.line
    radius: 5
    clip: true
    function percentile(series: var, key: string): string {
        return series && series.count ? Number(series[key]).toFixed(2) + " ms" : "不可用";
    }
    function summary(sample: var): string {
        if (!diagnostics)
            return "诊断已关闭";
        if (!sample.submissions)
            return "等待有效渲染样本…";
        return ["RHI 提交间隔 p50 / p95 / p99", percentile(sample.submissionIntervals, "p50") + " / "
                + percentile(sample.submissionIntervals, "p95") + " / " + percentile(
                    sample.submissionIntervals, "p99"), "CPU 提交 p95 " + percentile(sample.cpu,
                                                                                   "p95") + " · 同步 p95 "
                + percentile(sample.sync, "p95"), "GPU 已完成帧 p95 " + percentile(sample.gpu, "p95"),
                "Qt 呈现事件 p95 " + percentile(sample.qtFrameIntervals, "p95"),
                "本批上传 图集 / 选中 / uniform", sample.atlasUploadBytes + " / "
                + sample.selectedUploadBytes + " / " + sample.uniformUploadBytes + " bytes",
                "解码启动累计 " + sample.decodeStarts + " · 活跃 " + sample.decodeActive + " · 等待 "
                + sample.decodePending, "当前调度下提交吞吐 " + Number(sample.submissionRate).toFixed(1)
                + " / 秒", "提交样本 " + sample.submissions + " / " + Number(sample.elapsedMs).toFixed(0)
                + " ms", "VSYNC 请求间隔 " + sample.requestedSwapInterval + " · 实际 VSYNC / 屏幕显示时刻不可用",
                "固定 256 样本窗口，超出保留最近样本。RHI 间隔反映当前调度，非完整产品吞吐基准。CPU 不含 GPU 完成；GPU 范围由 QRhi 后端提供。"].join(
                    "\n");
    }
    onVisibleChanged: {
        if (!visible)
            diagnostics = false;
    }
    ScrollView {
        id: scroll
        objectName: "experimentScroll"
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        font.family: root.tokens.regular.family
        palette.window: root.tokens.surface
        palette.base: root.tokens.surface
        palette.button: root.tokens.surface
        palette.buttonText: root.tokens.ink
        palette.text: root.tokens.ink
        palette.windowText: root.tokens.ink
        palette.highlight: root.tokens.accent
        palette.highlightedText: root.tokens.surface
        Column {
            width: scroll.availableWidth
            spacing: 16
            ArchiveButton {
                objectName: "experimentsClose"
                tokens: root.tokens
                text: "关闭调试面板 ×"
                cssPixelSize: 11
                onClicked: root.closeRequested()
            }
            MotionPanel {
                width: parent.width
                tokens: root.tokens
                settings: root.settings
            }
            LightingPanel {
                compact: root.compact
                width: parent.width
                tokens: root.tokens
                lighting: root.lighting
            }
            CheckBox {
                objectName: "diagnosticsToggle"
                text: "采样运行诊断"
                checked: root.diagnostics
                onToggled: root.diagnostics = checked
            }
            ArchiveText {
                id: diagnosticText
                objectName: "experimentDiagnostics"
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 10
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: root.summary(root.sceneSource.diagnosticValues)
            }
        }
    }
}
