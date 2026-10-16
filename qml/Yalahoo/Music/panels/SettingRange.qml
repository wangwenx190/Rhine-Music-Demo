pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "../components"

Item {
    id: root
    required property DesignTokens tokens
    required property string label
    required property real value
    property string suffix: ""
    property int decimals: 2
    property real displayFactor: 1
    property real minimum: 0
    property real maximum: 1
    property real step: .01
    signal moved(real value)
    width: parent ? parent.width : 300
    implicitHeight: Math.max(caption.implicitHeight, slider.implicitHeight)
    ArchiveText {
        id: caption
        tokens: root.tokens
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, parent.width - slider.width - 20)
        text: root.label + " · " + (root.value * root.displayFactor).toFixed(root.decimals)
              + root.suffix
        cssPixelSize: 12
        wrapMode: Text.Wrap
    }
    Slider {
        id: slider
        objectName: root.objectName + "Control"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(150, parent.width * .48)
        from: root.minimum
        to: root.maximum
        stepSize: root.step
        value: root.value
        onMoved: root.moved(value)
        Accessible.name: root.label
    }
}
