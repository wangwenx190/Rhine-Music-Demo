// Connects visual invalidations and the sole Qt frame callback to the shared C++ clock.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    required property SceneAdapter sceneAdapter
    required property MotionController motionController
    property InputRouter inputRouter: null
    property StartupCoordinator startupCoordinator: null
    property TrackFocusController trackFocusController: null
    property PresentationController presentationController: null
    property OpeningController openingController: null
    required property bool visualActive
    required property bool offscreenRendering

    Connections {
        target: root.sceneAdapter
        function onFrameChanged() {
            root.motionController.clock.requestVisualFrame();
        }
    }
    Connections {
        target: root.motionController
        function onStateChanged() {
            root.motionController.clock.requestVisualFrame();
        }
        function onReducedMotionChanged() {
            root.motionController.clock.requestVisualFrame();
        }
    }
    Connections {
        target: root.inputRouter
        function onInputReceived() {
            root.motionController.clock.requestVisualFrame();
        }
    }
    Binding {
        target: root.startupCoordinator
        property: "visualActive"
        value: root.visualActive
        when: root.startupCoordinator !== null
    }

    FrameAnimation {
        running: !root.offscreenRendering && root.visualActive && (
                     !root.motionController.reducedMotion
                     || root.motionController.clock.workPending || (root.trackFocusController
                                                                    && root.trackFocusController.active)
                     || (root.presentationController && root.presentationController.phase
                         !== "archive" && root.presentationController.phase !== "detail") || (
                         root.openingController && root.openingController.active &&
                         !root.openingController.preparing))
        onRunningChanged: root.motionController.clock.active = running
        Component.onCompleted: root.motionController.clock.active = running
        onTriggered: root.motionController.clock.advance(frameTime)
    }
}
