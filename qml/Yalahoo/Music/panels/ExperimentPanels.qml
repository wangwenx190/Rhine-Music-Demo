pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Rectangle {
    id: root
    antialiasing: true
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
    onVisibleChanged: {
        if (!visible)
            diagnostics = false;
    }
    ArchiveScrollView {
        id: scroll
        tokens: root.tokens
        objectName: "experimentScroll"
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        font.family: root.tokens.regular.family

        Column {
            width: scroll.availableWidth
            spacing: 16
            ArchiveButton {
                objectName: "experimentsClose"
                tokens: root.tokens
                text: qsTr("Close tools \u00d7")
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
            ArchiveCheckBox {
                tokens: root.tokens
                objectName: "diagnosticsToggle"
                text: qsTr("Sample runtime diagnostics")
                checked: root.diagnostics
                onToggled: root.diagnostics = checked
            }
            PanelDescription {
                id: diagnosticText
                objectName: "experimentDiagnostics"
                tokens: root.tokens
                cssPixelSize: 10
                text: root.sceneSource.diagnosticText
            }
        }
    }
}
