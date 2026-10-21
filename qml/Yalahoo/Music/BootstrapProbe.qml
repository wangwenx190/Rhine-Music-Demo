import QtQuick
import "components"
import Yalahoo.Music

ProbeWindow {
    id: window
    recordProbeInput: true
    SceneItem {
        id: nativeScene
        z: -1
        objectName: "nativeScene"
        anchors.fill: parent
        adapter: window.sceneAdapter
        geometryOnly: window.sceneReview || window.geometryProbe
    }
    ArchiveText {
        tokens: window.tokens
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
        visible: window.preferencesError.length > 0 || nativeScene.errorText.length > 0

        text: window.preferencesError.length > 0 ? window.preferencesError :
                                                   nativeScene.errorText.length > 0 ? qsTr(
                                                                                          "Graphics configuration unavailable: %1").arg(
                                                                                          nativeScene.errorText) :
                                                                                      qsTr("Output reached the size protection limit. Quality preferences are preserved.")
        color: "#a13328"
    }
}
