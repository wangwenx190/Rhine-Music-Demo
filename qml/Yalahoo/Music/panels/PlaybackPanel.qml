pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    objectName: "audioSettingsPanel"
    required property PlaybackController playback
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property Component heading: null
    property bool navigationInputOwner: true
    contentWidth: availableWidth

    function focusControl() {
        playToggle.forceActiveFocus();
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
                id: playToggle
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
        ArchiveSlider {
            tokens: root.tokens
            width: parent.width
            from: 0
            to: 1
            value: root.playback.volume
            onMoved: root.playback.volume = value
            Accessible.name: "歌曲音量"
        }
        Column {
            width: parent.width
            spacing: 14
            visible: root.backgroundMusic !== null
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "bgmEnabled"
                text: "氛围 BGM"
                checked: root.backgroundMusic ? root.backgroundMusic.enabled : false
                onToggled: if (root.backgroundMusic)
                               root.backgroundMusic.enabled = checked
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: "歌曲开始前淡出；暂停保持安静，停止或队列结束后恢复。"
                cssPixelSize: 11
                wrapMode: Text.Wrap
                color: root.tokens.muted
            }
            ArchiveSlider {
                tokens: root.tokens
                objectName: "bgmVolume"
                width: parent.width
                from: 0
                to: 1
                value: root.backgroundMusic ? root.backgroundMusic.volume : 0
                onMoved: if (root.backgroundMusic)
                             root.backgroundMusic.volume = value
                Accessible.name: "BGM 音量"
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: root.backgroundMusic ? root.backgroundMusic.error : ""
                visible: text.length > 0
                cssPixelSize: 11
                wrapMode: Text.WrapAnywhere
                color: root.tokens.accent
            }
        }
        Column {
            width: parent.width
            spacing: 14
            visible: root.interfaceSound !== null
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "interfaceSoundEnabled"
                text: "界面音效"
                checked: root.interfaceSound ? root.interfaceSound.enabled : false
                onToggled: if (root.interfaceSound)
                               root.interfaceSound.enabled = checked
            }
            ArchiveSlider {
                tokens: root.tokens
                objectName: "interfaceSoundVolume"
                width: parent.width
                from: 0
                to: 1
                value: root.interfaceSound ? root.interfaceSound.volume : 0
                onMoved: if (root.interfaceSound)
                             root.interfaceSound.volume = value
                Accessible.name: "界面音效音量"
            }
            ArchiveButton {
                tokens: root.tokens
                text: "试听界面音效 ↗"
                enabled: root.interfaceSound !== null && root.interfaceSound.enabled
                         && root.interfaceSound.available
                onClicked: root.interfaceSound.preview()
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                text: "缺少独立短音频的事件保持静音。BGM 与音效使用系统默认输出，音量分别控制。"
                cssPixelSize: 11
                wrapMode: Text.Wrap
                color: root.tokens.muted
            }
        }
        ArchiveText {
            tokens: root.tokens
            text: "输出设备"
            cssPixelSize: 12
        }
        ArchiveComboBox {
            id: output
            tokens: root.tokens
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
