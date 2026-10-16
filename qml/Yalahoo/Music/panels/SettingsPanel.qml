pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    objectName: "settingsPanel"
    required property SettingsController settings
    required property LibrarySession librarySession
    property LibraryStore store: null
    property PlaybackController playback: null
    property BackgroundMusicController backgroundMusic: null
    property InterfaceSoundController interfaceSound: null
    property Component heading: null
    property bool navigationInputOwner: true
    property bool narrow: false
    property bool qualityExpanded: false
    property bool musicBrainzExpanded: false
    signal replayRequested
    signal experimentsRequested
    signal fullscreenRequested
    contentWidth: availableWidth

    function focusControl() {
        themeChoice.forceActiveFocus();
    }
    function qualityField(key, value) {
        const values = Object.assign({}, settings.values.renderQuality);
        values[key] = value;
        settings.update("renderQuality", values);
    }
    Column {
        width: root.availableWidth
        spacing: 0
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "外观主题"
                cssPixelSize: 12
            }
            Row {
                width: parent.width
                spacing: root.narrow ? 8 : 12
                ThemeCard {
                    id: themeChoice
                    objectName: "settingsDayTheme"
                    width: (parent.width - parent.spacing) / 2
                    tokens: root.tokens
                    night: false
                    narrow: root.narrow
                    selected: root.settings.values.theme === "day"
                    onClicked: root.settings.update("theme", "day")
                }
                ThemeCard {
                    objectName: "settingsNightTheme"
                    width: (parent.width - parent.spacing) / 2
                    tokens: root.tokens
                    night: true
                    narrow: root.narrow
                    selected: root.settings.values.theme === "night"
                    onClicked: root.settings.update("theme", "night")
                }
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "音乐库排列"
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsSort"
                tokens: root.tokens
                label: "排列方式"
                choices: [
                    {
                        label: "按流派",
                        value: "genre"
                    },
                    {
                        label: "按歌手",
                        value: "artist"
                    },
                    {
                        label: "按专辑名",
                        value: "album"
                    }
                ]
                selected: root.settings.values.sortMode
                onChosen: value => root.settings.update("sortMode", value)
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 12
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: "按歌手时，同一歌手的专辑放在同一列；按专辑名时，每 12 张一列。"
            }
        }
        SettingsSection {
            tokens: root.tokens

            MetadataSection {
                tokens: root.tokens
                librarySession: root.librarySession
                kind: "introduction"
                title: "专辑介绍"
                explanation: "公开百科介绍与资料来源保存在本机。"
            }
        }
        SettingsSection {
            tokens: root.tokens

            MetadataSection {
                tokens: root.tokens
                librarySession: root.librarySession
                kind: "credits"
                title: "制作信息"
                explanation: "保留本地标签、参与人员与已有资料来源。"
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "高级设置 · 专辑阵列"
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsArray"
                tokens: root.tokens
                label: "专辑列显示"
                choices: [
                    {
                        label: "填充画面",
                        value: "filled"
                    },
                    {
                        label: "真实专辑列",
                        value: "realistic"
                    }
                ]
                selected: root.settings.values.arrayMode
                onChosen: value => root.settings.update("arrayMode", value)
            }
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "settingsRemember"
                text: "保留每列浏览位置"
                checked: root.settings.values.rememberColumnPosition
                onToggled: root.settings.update("rememberColumnPosition", checked)
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "RENDER QUALITY · 渲染画质"
                cssPixelSize: 12
            }
            SettingChoice {
                objectName: "settingsQuality"
                tokens: root.tokens
                label: "画质预设"
                choices: [
                    {
                        label: "性能",
                        value: "performance"
                    },
                    {
                        label: "原始",
                        value: "original"
                    },
                    {
                        label: "高",
                        value: "high"
                    },
                    {
                        label: "极高",
                        value: "ultra"
                    }
                ].concat(root.settings.values.quality === "custom" ? [
                                                                         {
                                                                             label: "自定义",
                                                                             value: "custom"
                                                                         }
                                                                     ] : [])
                selected: root.settings.values.quality
                onChosen: value => root.settings.update("quality", value)
            }
            ArchiveButton {
                objectName: "settingsQualityExpand"
                tokens: root.tokens
                text: (root.qualityExpanded ? "▾ " : "▸ ") + "精细设置 · 清晰度 / 材质 / 阴影"
                onClicked: root.qualityExpanded = !root.qualityExpanded
            }
            Column {
                width: parent.width
                spacing: 16
                visible: root.qualityExpanded
                SettingRange {
                    objectName: "settingsscale"
                    tokens: root.tokens
                    label: "渲染比例"
                    minimum: 50
                    maximum: 200
                    suffix: "%"
                    decimals: 0
                    step: 5
                    value: root.settings.values.renderQuality.scale
                    onMoved: value => root.qualityField("scale", value)
                }
                SettingChoice {
                    objectName: "settingspixelRatio"
                    tokens: root.tokens
                    label: "像素密度上限"
                    choices: [
                        {
                            label: "1×",
                            value: 1
                        },
                        {
                            label: "1.5×",
                            value: 1.5
                        },
                        {
                            label: "2×",
                            value: 2
                        },
                        {
                            label: "3×",
                            value: 3
                        }
                    ]
                    selected: root.settings.values.renderQuality.pixelRatio
                    onChosen: value => root.qualityField("pixelRatio", value)
                }
                SettingChoice {
                    objectName: "settingsantialias"
                    tokens: root.tokens
                    label: "抗锯齿"
                    choices: [
                        {
                            label: "原始",
                            value: "off"
                        },
                        {
                            label: "SMAA",
                            value: "smaa"
                        }
                    ]
                    selected: root.settings.values.renderQuality.antialias
                    onChosen: value => root.qualityField("antialias", value)
                }
                SettingChoice {
                    objectName: "settingsanisotropy"
                    tokens: root.tokens
                    label: "各向异性过滤"
                    choices: [
                        {
                            label: "1×",
                            value: 1
                        },
                        {
                            label: "2×",
                            value: 2
                        },
                        {
                            label: "4×",
                            value: 4
                        },
                        {
                            label: "8×",
                            value: 8
                        },
                        {
                            label: "16×",
                            value: 16
                        }
                    ]
                    selected: root.settings.values.renderQuality.anisotropy
                    onChosen: value => root.qualityField("anisotropy", value)
                }
                SettingChoice {
                    objectName: "settingstransmission"
                    tokens: root.tokens
                    label: "透射分辨率"
                    choices: [
                        {
                            label: "25%",
                            value: 0.25
                        },
                        {
                            label: "50%",
                            value: 0.5
                        },
                        {
                            label: "75%",
                            value: 0.75
                        },
                        {
                            label: "100%",
                            value: 1
                        }
                    ]
                    selected: root.settings.values.renderQuality.transmission
                    onChosen: value => root.qualityField("transmission", value)
                }
                SettingChoice {
                    objectName: "settingsshadows"
                    tokens: root.tokens
                    label: "阴影精度"
                    choices: [
                        {
                            label: "关闭",
                            value: 0
                        },
                        {
                            label: "1024",
                            value: 1024
                        },
                        {
                            label: "2048",
                            value: 2048
                        },
                        {
                            label: "4096",
                            value: 4096
                        }
                    ]
                    selected: root.settings.values.renderQuality.shadows
                    onChosen: value => root.qualityField("shadows", value)
                }
                SettingChoice {
                    objectName: "settingsaoSamples"
                    tokens: root.tokens
                    label: "环境遮蔽采样"
                    choices: [
                        {
                            label: "关闭",
                            value: 0
                        },
                        {
                            label: "16 采样",
                            value: 16
                        },
                        {
                            label: "32 采样",
                            value: 32
                        },
                        {
                            label: "64 采样",
                            value: 64
                        }
                    ]
                    selected: root.settings.values.renderQuality.aoSamples
                    onChosen: value => root.qualityField("aoSamples", value)
                }
                SettingChoice {
                    objectName: "settingsaoResolution"
                    tokens: root.tokens
                    label: "环境遮蔽分辨率"
                    choices: [
                        {
                            label: "50%",
                            value: 0.5
                        },
                        {
                            label: "75%",
                            value: 0.75
                        },
                        {
                            label: "100%",
                            value: 1
                        }
                    ]
                    selected: root.settings.values.renderQuality.aoResolution
                    onChosen: value => root.qualityField("aoResolution", value)
                }
                SettingRange {
                    objectName: "settingsdepthOfField"
                    tokens: root.tokens
                    label: "景深强度 · 阵列"
                    minimum: 0
                    maximum: 150
                    suffix: "%"
                    decimals: 0
                    step: 5
                    value: root.settings.values.renderQuality.depthOfField
                    onMoved: value => root.qualityField("depthOfField", value)
                }
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 11
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: "即时生效并自动保存。受设备尺寸及 829 万像素目标上限保护，用户画质偏好保持所选值。"
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "动效与显示"
                cssPixelSize: 12
            }
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "settingsReduced"
                text: "减少动态效果"
                checked: root.settings.values.reduced
                onToggled: root.settings.update("reduced", checked)
            }
            SettingRange {
                objectName: "settingsSpeed"
                tokens: root.tokens
                label: "动画速度"
                suffix: "×"
                minimum: .25
                maximum: 3
                step: .05
                value: root.settings.values.motionSpeed
                onMoved: value => root.settings.update("motionSpeed", value)
            }
            ArchiveButton {
                tokens: root.tokens
                text: "切换全屏 ↗"
                onClicked: root.fullscreenRequested()
            }
            ArchiveButton {
                objectName: "settingsReplay"
                tokens: root.tokens
                text: "重播开场 ↗"
                onClicked: root.replayRequested()
            }
            ArchiveButton {
                objectName: "settingsReload"
                tokens: root.tokens
                text: "重新载入界面 ↻"
                onClicked: root.settings.reload()
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveText {
                tokens: root.tokens
                text: "声音"
                cssPixelSize: 12
            }
            SettingRange {
                tokens: root.tokens
                label: "歌曲音量"
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.playback !== null
                value: root.playback ? root.playback.volume : 0
                onMoved: value => {
                    if (root.playback)
                        root.playback.volume = value;
                }
            }
            ArchiveCheckBox {
                tokens: root.tokens
                text: "界面音效"
                visible: root.interfaceSound !== null
                checked: root.interfaceSound ? root.interfaceSound.enabled : false
                onToggled: {
                    if (root.interfaceSound)
                        root.interfaceSound.enabled = checked;
                }
            }
            SettingRange {
                tokens: root.tokens
                label: "音效音量"
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.interfaceSound !== null
                value: root.interfaceSound ? root.interfaceSound.volume : 0
                onMoved: value => {
                    if (root.interfaceSound)
                        root.interfaceSound.volume = value;
                }
            }
            ArchiveCheckBox {
                tokens: root.tokens
                text: "氛围 BGM"
                visible: root.backgroundMusic !== null
                checked: root.backgroundMusic ? root.backgroundMusic.enabled : false
                onToggled: {
                    if (root.backgroundMusic)
                        root.backgroundMusic.enabled = checked;
                }
            }
            SettingRange {
                tokens: root.tokens
                label: "BGM 音量"
                suffix: "%"
                decimals: 0
                displayFactor: 100
                visible: root.backgroundMusic !== null
                value: root.backgroundMusic ? root.backgroundMusic.volume : 0
                onMoved: value => {
                    if (root.backgroundMusic)
                        root.backgroundMusic.volume = value;
                }
            }
            ArchiveButton {
                tokens: root.tokens
                text: "试听界面音效 ↗"
                enabled: root.interfaceSound !== null && root.interfaceSound.enabled
                         && root.interfaceSound.available
                onClicked: root.interfaceSound.preview()
            }
            SettingChoice {
                tokens: root.tokens
                label: "歌曲输出设备"
                visible: root.playback !== null
                choices: root.playback ? root.playback.devices.map(device => ({
                    label: device.name,
                    value: device.id
                })) : []
                selected: root.playback ? root.playback.selectedDevice : ""
                onChosen: value => {
                    if (root.playback)
                        root.playback.selectDevice(value);
                }
            }
            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 11
                color: root.tokens.muted
                wrapMode: Text.Wrap
                text: "BGM 和界面音效使用系统默认输出，三路音量独立。缺少独立短音频的事件保持静音。"
            }
        }
        SettingsSection {
            tokens: root.tokens

            ArchiveButton {
                objectName: "settingsMusicBrainzExpand"
                tokens: root.tokens
                text: (root.musicBrainzExpanded ? "▾ " : "▸ ") + "高级设置 · MusicBrainz"
                onClicked: root.musicBrainzExpanded = !root.musicBrainzExpanded
            }
            Column {
                width: parent.width
                spacing: 16
                visible: root.musicBrainzExpanded
                MetadataSection {
                    tokens: root.tokens
                    librarySession: root.librarySession
                    kind: "enrichment"
                    title: "MusicBrainz"
                    explanation: "本机缓存保留专辑名称、艺术家、流派、制作人员及来源，人工分类优先。"
                }
                ArchiveTextField {
                    id: contact
                    tokens: root.tokens
                    objectName: "settingsMetadataContact"
                    width: parent.width
                    text: root.librarySession.metadataConfiguration.musicBrainzContact || ""
                    placeholderText: "联系邮箱或公开项目网址"
                    Accessible.name: "MusicBrainz 联系邮箱或项目网址"
                }
                ArchiveCheckBox {
                    id: onlineEnabled
                    tokens: root.tokens
                    text: "扫描后自动补充新专辑资料（联网暂未启用）"
                    checked: root.librarySession.metadataConfiguration.onlineEnabled || false
                    enabled: root.librarySession.providerCapabilities.networkEnabled && root.store
                             !== null && !root.store.busy
                    onToggled: root.store.setMetadataConfiguration({
                                                                       onlineEnabled: checked
                                                                   })
                }
                ArchiveButton {
                    tokens: root.tokens
                    text: "保存资料库设置 ↗"
                    enabled: root.store !== null && !root.store.busy
                    onClicked: root.store.setMetadataConfiguration({
                                                                       musicBrainzContact:
                                                                       contact.text
                                                                   })
                }
                ArchiveText {
                    tokens: root.tokens
                    width: parent.width
                    cssPixelSize: 11
                    color: root.tokens.muted
                    wrapMode: Text.Wrap
                    text: root.store ? root.store.error : "资料配置由显式导入或原生音乐库保存。"
                }
            }
        }
        SettingsSection {
            tokens: root.tokens
            ArchiveButton {
                objectName: "settingsExperiments"
                tokens: root.tokens
                text: "光照 / 速度调试面板 ↗"
                onClicked: root.experimentsRequested()
            }
        }
        SettingsSection {
            tokens: root.tokens
            last: true

            ArchiveText {
                tokens: root.tokens
                width: parent.width
                cssPixelSize: 12
                wrapMode: Text.Wrap
                text: "开发与资源 · yalahOO!!!\n音乐适配与维护：RonaldDeng\n原版界面：LBEILC / RhineLabUI\n代码 MIT 许可 · 资源与字体许可随应用保留。"
            }
        }
    }
}
