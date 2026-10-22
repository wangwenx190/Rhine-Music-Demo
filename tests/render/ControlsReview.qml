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
            text: qsTr("Real album columns")
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
                    label: qsTr("By genre"),
                    value: "genre"
                },
                {
                    label: qsTr("By artist"),
                    value: "artist"
                },
                {
                    label: qsTr("By album title"),
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
            placeholderText: qsTr("Search albums and tracks")
        }
        ArchiveTextArea {
            objectName: "controlArea"
            tokens: design
            width: parent.width
            text: "\u4e00\u884c\n\u7b2c\u4e8c\u884c"
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
            text: qsTr("Hold for original")
        }
    }
}
