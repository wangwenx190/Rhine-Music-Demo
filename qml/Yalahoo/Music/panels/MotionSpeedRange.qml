pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

SettingRange {
    id: root
    required property SettingsController settings
    label: qsTranslate("SettingsPanel", "Animation speed")
    suffix: "\u00d7"
    minimum: settings.minimumMotionSpeed
    maximum: settings.maximumMotionSpeed
    step: settings.motionSpeedStep
    value: settings.values.motionSpeed
    onMoved: value => root.settings.update("motionSpeed", value)
}
