pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

ArchiveButton {
    id: root
    required property InterfaceSoundController interfaceSound
    text: qsTranslate("PlaybackPanel", "Preview interface sound \u2197")
    enabled: interfaceSound !== null && interfaceSound.enabled && interfaceSound.available
    onClicked: root.interfaceSound.preview()
}
