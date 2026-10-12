import QtQuick
import QtQuick3D
import Yalahoo.RhiReview

View3D {
    id: root
    property var fixture
    property int channel: 0
    property string cover: "square"
    property bool geometry: false
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: root.channel === 2 ? "white" : root.fixture.backgroundSrgb
        tonemapMode: SceneEnvironment.TonemapModeNone
        effects: root.channel === 0 ? [] : [prepassOutput]
    }
    Effect {
        id: prepassOutput
        property vector3d backgroundLinear: root.fixture.background
        property real cameraNear: root.fixture.near
        property real cameraFar: root.fixture.far
        property bool packedDepth: root.channel === 2
        passes: Pass {
            output: Buffer {
                format: Buffer.RGBA32F
            }
            shaders: Shader {
                stage: Shader.Fragment
                shader: "qrc:/assets/shaders/ao-prepass.frag"
            }
        }
    }
    camera: PerspectiveCamera {
        position: root.fixture.position
        rotation: root.fixture.rotation
        fieldOfView: root.fixture.fov
        clipNear: root.fixture.near
        clipFar: root.fixture.far
    }
    PrincipledMaterial {
        id: diagnostic
        baseColor: "white"
        cullMode: Material.BackFaceCulling
    }
    CustomMaterial {
        id: paperMaterial
        property TextureInput coverMap: TextureInput {
            texture: Texture {
                source: "qrc:/assets/demo-covers/" + root.cover + ".png"
                minFilter: Texture.Linear
                magFilter: Texture.Linear
            }
        }
        property TextureInput environmentMap: TextureInput {
            texture: Texture {
                textureData: FixedTextureData {
                    asset: "room-pmrem"
                }
                minFilter: Texture.Linear
                magFilter: Texture.Linear
            }
        }
        property bool lightingEnabled: true
        property vector3d paperIrradiance: Qt.vector3d(1, 1, 1)
        property real environmentIntensity: root.fixture.environmentIntensity
        property vector3d rowColumn: root.fixture.rowColumn
        property vector4d rowSettings: root.fixture.rowSettings
        property real rowDetail: root.fixture.rowDetail
        property real rowFill: root.fixture.rowFill
        property vector3d lightColumn: root.fixture.lightColumn
        property real printAmbient: root.fixture.printAmbient
        property vector3d scatterColor: Qt.vector3d(0, 0, 0)
        property real scatterStrength: 0
        property vector3d spotPosition: root.fixture.spotPosition
        property vector3d spotTarget: root.fixture.spotTarget
        property vector3d spotColor: root.fixture.spotColor
        property real spotIntensity: root.fixture.spotIntensity
        property vector3d groundColor: root.fixture.groundColor
        property vector3d skyColor: root.fixture.skyColor
        property vector3d keyColor: root.fixture.keyColor
        property vector3d lightIntensities: root.fixture.lightIntensities
        vertexShader: "qrc:/assets/shaders/album-paper.vert"
        fragmentShader: "qrc:/assets/shaders/album-paper.frag"
    }
    Node {
        position: root.fixture.origin
        Model {
            source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"
            materials: diagnostic
            visible: root.geometry
        }
        Model {
            source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"
            materials: diagnostic
            visible: root.geometry
        }
        Model {
            source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"
            materials: diagnostic
            visible: root.geometry
        }
        Model {
            source: "#Rectangle"
            position: Qt.vector3d(.17, 1.85, .027)
            scale: Qt.vector3d(.0306, .0306, 1)
            materials: root.channel === 0 ? paperMaterial : diagnostic
        }
    }
}
