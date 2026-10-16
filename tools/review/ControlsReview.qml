pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "components"

Rectangle {
    id: root
    property bool night: false
    color: design.surface
    width: 640
    height: 480
    DesignTokens {
        id: design
        night: root.night
        reducedMotion: true
        offscreenRendering: true
    }
    Column {
        x: 20
        y: 20
        width: parent.width - 40
        spacing: 16
        ArchiveCheckBox {
            objectName: "controlCheck"
            tokens: design
            text: "真实专辑列"
        }
        ArchiveSlider {
            objectName: "controlSlider"
            tokens: design
            from: .25
            to: 3
            stepSize: .05
            value: 1
        }
        ArchiveComboBox {
            objectName: "controlChoice"
            tokens: design
            model: [
                {
                    label: "按流派",
                    value: "genre"
                },
                {
                    label: "按歌手",
                    value: "artist"
                },
                {
                    label: "按专辑名",
                    value: "album"
                }
            ]
            textRole: "label"
            valueRole: "value"
        }
        ArchiveTextField {
            objectName: "controlField"
            tokens: design
            width: parent.width
            placeholderText: "搜索专辑与歌曲"
        }
        ArchiveTextArea {
            objectName: "controlArea"
            tokens: design
            width: parent.width
            text: "一行\n第二行"
        }
        ArchiveProgressBar {
            objectName: "controlProgress"
            tokens: design
            width: parent.width
            value: .5
        }
        ArchiveButton {
            objectName: "controlButton"
            tokens: design
            framed: true
            cssPixelSize: 10
            text: "按住看原版"
        }
    }
}
