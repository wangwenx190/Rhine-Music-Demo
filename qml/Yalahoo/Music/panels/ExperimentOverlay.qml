// Owns the experiment panel loader and its responsive visual lifetime.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Loader {
    id: root
    required property DesignTokens tokens
    property SettingsController settingsController: null
    required property SelectionLighting lighting
    property OpeningController openingController: null
    property PanelController panelController: null
    required property SceneItem sceneSource
    required property real viewportWidth
    required property real viewportHeight
    required property bool experimentsVisible
    readonly property ExperimentPanels panel: item as ExperimentPanels
    signal closeRequested

    objectName: "experimentsLoader"
    x: root.viewportWidth <= 540 ? 20 : root.viewportWidth <= 1100 ? 25 : 40
    y: root.viewportWidth / root.viewportHeight < 1.05 ? 150 : root.viewportWidth >= 1800 ? 190 :
                                                                                            root.viewportWidth
                                                                                            <= 540 ? 125 :
                                                                                                     root.viewportWidth
                                                                                                     <= 1100 ? 140 :
                                                                                                               160
    width: Math.max(0, Math.min(root.viewportWidth <= 760 || root.viewportHeight <= 650 ? 248 :
                                                                                          root.viewportWidth
                                                                                          <= 1100 ? 254 :
                                                                                                    272, root.viewportWidth
                                - 2 * x))
    height: Math.max(0, root.viewportHeight - y - (root.viewportWidth / root.viewportHeight < 1.05
                                                   ? 85 : 100))
    active: root.experimentsVisible && root.settingsController !== null
    visible: active && (!root.openingController || !root.openingController.active) && (
                 !root.panelController || !root.panelController.surface.shown)
    sourceComponent: ExperimentPanels {
        compact: root.viewportWidth <= 760 || root.viewportHeight <= 650
        tokens: root.tokens
        settings: root.settingsController
        lighting: root.lighting
        sceneSource: root.sceneSource
        onCloseRequested: root.closeRequested()
    }
}
