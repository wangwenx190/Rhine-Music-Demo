pragma ComponentBehavior: Bound
import QtQuick
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
    property bool stacked: false
    property real captionSize: 12
    signal moved(real value)
    width: parent ? parent.width : 300
    implicitHeight: stacked ? 40 : Math.max(caption.implicitHeight, slider.implicitHeight + 4)
    ArchiveText {
        id: caption
        tokens: root.tokens
        anchors.verticalCenter: root.stacked ? undefined : parent.verticalCenter
        y: root.stacked ? 5 : 0
        width: root.stacked ? parent.width - output.implicitWidth - 8 : Math.max(0, parent.width
                                                                                 - slider.width
                                                                                 - 20)
        text: root.label + (root.stacked ? "" : " \u00b7 " + (root.value
                                                              * root.displayFactor).toFixed(
                                               root.decimals) + root.suffix)
        cssPixelSize: root.captionSize
        cssLineHeight: root.stacked ? 15 : Math.round(cssAscent) + Math.round(cssDescent)
        wrapMode: Text.Wrap
    }
    ArchiveText {
        id: output
        tokens: root.tokens
        anchors.right: parent.right
        y: 5
        visible: root.stacked
        text: (root.value * root.displayFactor).toFixed(root.decimals) + root.suffix
        cssPixelSize: 10
        cssLineHeight: 15
        color: root.tokens.muted
    }
    ArchiveSlider {
        id: slider
        implicitHeight: root.stacked ? 20 : 16
        disabledOpacity: root.stacked ? .3 : .38
        tokens: root.tokens
        objectName: root.objectName + "Control"
        anchors.right: parent.right
        y: root.stacked ? parent.height - height : (parent.height - height) / 2
        width: root.stacked ? parent.width : Math.min(150, parent.width * .48)
        from: root.minimum
        to: root.maximum
        stepSize: root.step
        value: root.value
        onMoved: root.moved(value)
        Accessible.name: root.label
    }
}
