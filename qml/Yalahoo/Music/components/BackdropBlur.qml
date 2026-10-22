// Owns the shared crop and intermediate target of a separable local backdrop blur.
// Each caller supplies its shader passes and source-authored blur extent.
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    readonly property real outputDpr: Window.window ? Window.window.devicePixelRatio : 1
    property bool hdrActive: false
    required property Item sceneSource
    required property real sceneX
    required property real sceneY
    required property real blurRadius
    required property Item horizontalPass
    property real targetWidth: width + 2 * blurRadius
    property real targetHeight: height + 2 * blurRadius
    property string cropName: ""
    property string intermediateName: ""
    readonly property ShaderEffectSource sceneTexture: crop
    readonly property ShaderEffectSource horizontalTexture: intermediate
    clip: true
    BackdropTexture {
        id: crop
        objectName: root.cropName
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.sceneSource
        sourceRect: Qt.rect(root.sceneX - root.blurRadius, root.sceneY - root.blurRadius, root.targetWidth,
                            root.targetHeight)
        width: root.targetWidth
        height: root.targetHeight
    }
    BackdropTexture {
        id: intermediate
        objectName: root.intermediateName
        hdrActive: root.hdrActive
        outputDpr: root.outputDpr
        sourceItem: root.horizontalPass
        width: root.targetWidth
        height: root.targetHeight
        hideSource: true
    }
}
