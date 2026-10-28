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
        text: qsTr("DEV \u00b7 MOTION Overall speed %1").arg(root.expanded ? "\u2212" : "+")
        cssPixelSize: 11
        onClicked: root.expanded = !root.expanded
    }
    Column {
        width: parent.width
        spacing: 10
        visible: root.expanded
        MotionSpeedRange {
            stacked: true
            captionSize: 10
            objectName: "motionSpeed"
            tokens: root.tokens
            settings: root.settings
        }
        PanelDescription {
            tokens: root.tokens
            cssPixelSize: 10
            text: qsTr(
                      "Albums, camera, text, and light transitions change speed together. Actual light duration = panel duration / this multiplier. Track playback and the opening sequence are unchanged.")
        }
        ArchiveButton {
            objectName: "motionReset"
            tokens: root.tokens
            text: qsTr("Restore 1x")
            cssPixelSize: 11
            onClicked: root.settings.update("motionSpeed", 1)
        }
    }
}
