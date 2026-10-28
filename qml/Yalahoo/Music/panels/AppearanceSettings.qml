pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Column {
    id: root
    required property DesignTokens tokens
    required property SettingsController settings
    property bool narrow: false
    function focusControl(): void {
        themeChoice.forceActiveFocus();
    }

    SettingsSection {
        tokens: root.tokens

        ArchiveText {
            tokens: root.tokens
            text: qsTranslate("SettingsPanel", "Theme")
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
            text: qsTranslate("SettingsPanel", "Library arrangement")
            cssPixelSize: 12
        }
        SettingChoice {
            objectName: "settingsSort"
            tokens: root.tokens
            label: qsTranslate("SettingsPanel", "Sort order")
            choices: [
                {
                    label: qsTranslate("SettingsPanel", "By genre"),
                    value: "genre"
                },
                {
                    label: qsTranslate("SettingsPanel", "By artist"),
                    value: "artist"
                },
                {
                    label: qsTranslate("SettingsPanel", "By album title"),
                    value: "album"
                }
            ]
            selected: root.settings.values.sortMode
            onChosen: value => root.settings.update("sortMode", value)
        }
        PanelDescription {
            tokens: root.tokens
            cssPixelSize: 12
            text: qsTranslate("SettingsPanel",
                              "Artist sorting puts each artist in one column. Album title sorting puts 12 albums in each column.")
        }
    }
}
