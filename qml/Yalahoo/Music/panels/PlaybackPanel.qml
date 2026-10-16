pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "../components"

ScrollView {
    id: root
    required property DesignTokens tokens
    required property PlaybackController playback
    property Component heading: null
    property bool navigationInputOwner: true
    contentWidth: availableWidth
    palette.window: tokens.surface
    palette.base: tokens.surface
    palette.dark: tokens.muted
    palette.mid: tokens.line
    palette.button: tokens.surface
    palette.buttonText: tokens.ink
    palette.text: tokens.ink
    palette.highlight: tokens.accent
    palette.highlightedText: tokens.surface
    function focusControl() {
        output.forceActiveFocus();
    }
    Column {
        width: root.availableWidth
        spacing: 18
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: root.playback.title || "尚未选择歌曲"
            cssPixelSize: 16
            wrapMode: Text.WrapAnywhere
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: !root.playback.outputAvailable ? (root.playback.requested ? "等待音频输出设备" :
                                                                              "暂无音频输出设备") :
                                                   root.playback.loading ? "正在加载歌曲…" :
                                                                           root.playback.playing
                                                                           ? "正在播放" :
                                                                             root.playback.state
                                                                             === "paused" ? "已暂停" :
                                                                                            "已停止"
            cssPixelSize: 11
            color: root.tokens.muted
        }
        PlaybackControls {
            width: parent.width
            tokens: root.tokens
            playback: root.playback
        }
        Row {
            spacing: 20
            ArchiveButton {
                tokens: root.tokens
                text: root.playback.requested ? "暂停" : "播放"
                onClicked: root.playback.toggle()
            }
            ArchiveButton {
                tokens: root.tokens
                text: "停止"
                onClicked: root.playback.stop()
            }
        }
        ArchiveText {
            tokens: root.tokens
            text: "歌曲音量"
            cssPixelSize: 12
        }
        Slider {
            width: parent.width
            from: 0
            to: 1
            value: root.playback.volume
            onMoved: root.playback.volume = value
            Accessible.name: "歌曲音量"
        }
        ArchiveText {
            tokens: root.tokens
            text: "输出设备"
            cssPixelSize: 12
        }
        ComboBox {
            id: output
            objectName: "songOutputDevices"
            width: parent.width
            model: root.playback.devices
            textRole: "name"
            valueRole: "id"
            currentIndex: Math.max(0, root.playback.devices.findIndex(device => device.id
                                                                                === root.playback.selectedDevice))
            onActivated: root.playback.selectDevice(currentValue)
            Accessible.name: "歌曲输出设备"
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: root.playback.notice
            visible: text.length > 0
            cssPixelSize: 11
            wrapMode: Text.Wrap
            color: root.tokens.accent
        }
        ArchiveText {
            objectName: "songPlaybackError"
            tokens: root.tokens
            width: parent.width
            text: root.playback.error
            visible: text.length > 0
            cssPixelSize: 11
            wrapMode: Text.WrapAnywhere
            color: root.tokens.accent
        }
        Row {
            visible: root.playback.error.length > 0
            spacing: 20
            ArchiveButton {
                tokens: root.tokens
                text: "重试"
                onClicked: root.playback.retry()
            }
            ArchiveButton {
                tokens: root.tokens
                text: "换下一首"
                enabled: root.playback.canNext
                onClicked: root.playback.next()
            }
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            text: "使用系统音频输出播放本地文件。格式不可用时可换曲或重试。"
            cssPixelSize: 11
            wrapMode: Text.Wrap
            color: root.tokens.muted
        }
    }
}
