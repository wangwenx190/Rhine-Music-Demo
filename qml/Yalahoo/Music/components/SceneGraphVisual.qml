pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Loader {
    id: root
    active: lifecycle.available
    SceneGraphLifecycle {
        id: lifecycle
        window: root.Window.window
    }
}
