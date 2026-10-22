// Restores borrowed visual focus targets after opening, detail and overview transitions.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Item {
    id: root
    property OpeningController openingController: null
    property PresentationController presentationController: null
    property OverviewController overviewController: null
    property Opening openingOverlay: null
    required property Browse browse
    property Item openingReturnFocus: null
    property Item presentationReturnFocus: null
    readonly property var hostWindow: Window.window
    signal preparationRequested

    function restorePresentationFocus(): void {
        if (!root.presentationController || !root.presentationController.archiveEnabled)
            return;
        const target = root.presentationReturnFocus;
        if (target && target.visible && target.enabled)
            target.forceActiveFocus(Qt.OtherFocusReason);
        else
            root.browse.forceActiveFocus(Qt.OtherFocusReason);
        root.presentationReturnFocus = null;
    }
    Connections {
        target: root.openingController
        function onStarted() {
            root.preparationRequested();
            if (!root.openingReturnFocus)
                root.openingReturnFocus = (root.hostWindow ? root.hostWindow.activeFocusItem :
                                                             null);
            if (root.openingOverlay)
                root.openingOverlay.focusSkip();
        }
        function onCompleted() {
            const target = root.openingReturnFocus;
            if (target && target.visible && target.enabled)
                target.forceActiveFocus(Qt.OtherFocusReason);
            else
                root.browse.forceActiveFocus(Qt.OtherFocusReason);
            root.openingReturnFocus = null;
        }
    }
    Connections {
        target: root.overviewController
        function onFocusRequested(overview: bool) {
            if (!overview)
                root.browse.focusOverviewToggle();
        }
    }
    Connections {
        target: root.presentationController
        function onPrepared() {
            if (root.presentationController.phase === "opening" && !root.presentationReturnFocus)
                root.presentationReturnFocus = (root.hostWindow ? root.hostWindow.activeFocusItem :
                                                                  null);
        }
        function onFocusRequested(detail) {
            if (!detail)
                Qt.callLater(root.restorePresentationFocus);
        }
    }
}
