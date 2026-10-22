pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Column {
    id: root
    required property DesignTokens tokens
    required property SettingsController settings

    SettingsSection {
        tokens: root.tokens

        ArchiveText {
            tokens: root.tokens
            text: qsTranslate("SettingsPanel", "Advanced \u00b7 Album array")
            cssPixelSize: 12
        }
        SettingChoice {
            objectName: "settingsArray"
            tokens: root.tokens
            label: qsTranslate("SettingsPanel", "Album columns")
            choices: [
                {
                    label: qsTranslate("SettingsPanel", "Fill the view"),
                    value: "filled"
                },
                {
                    label: qsTranslate("SettingsPanel", "Real album columns"),
                    value: "realistic"
                }
            ]
            selected: root.settings.values.arrayMode
            onChosen: value => root.settings.update("arrayMode", value)
        }
        ArchiveCheckBox {
            tokens: root.tokens
            objectName: "settingsRemember"
            text: qsTranslate("SettingsPanel", "Remember each column's position")
            checked: root.settings.values.rememberColumnPosition
            onToggled: root.settings.update("rememberColumnPosition", checked)
        }
    }
    SettingsSection {
        tokens: root.tokens

        ArchiveText {
            tokens: root.tokens
            text: qsTranslate("SettingsPanel", "RENDER QUALITY")
            cssPixelSize: 12
        }
        PanelDescription {
            tokens: root.tokens
            text: qsTranslate("SettingsPanel",
                              "Renders at the display's native pixel density. Antialiasing and full-resolution effects remain enabled.")
        }
        Column {
            width: parent.width
            spacing: 16
            SettingRange {
                objectName: "settingsdepthOfField"
                tokens: root.tokens
                label: qsTranslate("SettingsPanel", "Array depth of field")
                minimum: 0
                maximum: 300
                suffix: "%"
                decimals: 0
                step: 5
                value: root.settings.values.renderQuality.depthOfField
                onMoved: value => root.settings.updateQuality("depthOfField", value)
            }
        }
    }
}
