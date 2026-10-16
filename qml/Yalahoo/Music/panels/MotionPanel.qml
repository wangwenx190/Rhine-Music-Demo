pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Column {
    id: root
    objectName: "motionPanel"
    required property DesignTokens tokens
    required property SettingsController settings
    property bool expanded: true
    spacing: 10
    ArchiveButton {
        tokens: root.tokens
        text: "DEV · MOTION 整体切换速度 " + (root.expanded ? "−" : "+")
        cssPixelSize: 11
        onClicked: root.expanded = !root.expanded
    }
    Column {
        width: parent.width
        spacing: 10
        visible: root.expanded
        SettingRange {
            objectName: "motionSpeed"
            tokens: root.tokens
            label: "动画速度"
            minimum: .25
            maximum: 3
            step: .05
            suffix: "×"
            value: root.settings.values.motionSpeed
            onMoved: value => root.settings.update("motionSpeed", value)
        }
        ArchiveText {
            tokens: root.tokens
            width: parent.width
            cssPixelSize: 10
            color: root.tokens.muted
            wrapMode: Text.Wrap
            text: "专辑、镜头、文字与光带同步变速。光带实际时长 = 光效面板时长 ÷ 此倍率。歌曲播放和开场片头不变。"
        }
        ArchiveButton {
            objectName: "motionReset"
            tokens: root.tokens
            text: "恢复 1×"
            cssPixelSize: 11
            onClicked: root.settings.update("motionSpeed", 1)
        }
    }
}
