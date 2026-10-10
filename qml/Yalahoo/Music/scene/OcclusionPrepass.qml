pragma ComponentBehavior: Bound
import QtQuick
import QtQuick3D
import Yalahoo.Music

View3D {
    id: prepass
    required property SceneAdapter adapter
    required property vector3d origin
    property real cameraNear: adapter.clipNear
    property real cameraFar: adapter.clipFar
    property bool packedDepth: false
    property var neighbors: []
    width: adapter.quality.aoSize.width
    height: adapter.quality.aoSize.height
    explicitTextureWidth: width
    explicitTextureHeight: height
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "white"
        tonemapMode: SceneEnvironment.TonemapModeNone
        effects: Effect {
            property vector3d backgroundLinear: prepass.adapter.lighting.backgroundLinear
            property real cameraNear: prepass.cameraNear
            property real cameraFar: prepass.cameraFar
            property bool packedDepth: prepass.packedDepth
            passes: Pass {
                output: Buffer { format: Buffer.RGBA32F }
                shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/ao-prepass.frag" }
            }
        }
    }
    camera: PerspectiveCamera {
        position: prepass.adapter.cameraPosition
        rotation: prepass.adapter.cameraRotation
        fieldOfView: prepass.adapter.fieldOfView
        clipNear: prepass.cameraNear
        clipFar: prepass.cameraFar
    }
    PrincipledMaterial { id: opaque; baseColor: "white"; cullMode: Material.BackFaceCulling }
    CaseGeometry { position: prepass.origin; lid: opaque; spine: opaque; tray: opaque; paper: opaque }
    Repeater3D {
        model: prepass.neighbors
        delegate: CaseGeometry {
            required property var modelData
            position: Qt.vector3d(modelData[0],modelData[1],modelData[2])
            lid: opaque; spine: opaque; tray: opaque; paper: opaque
        }
    }
}
