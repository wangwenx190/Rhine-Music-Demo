pragma ComponentBehavior: Bound
import QtQuick
import QtQuick3D
import Yalahoo.Music

Node {
    id: materials
    required property SelectionLighting lighting
    property bool lightingEnabled: true
    property real warmth: lighting.warmth
    property real clarity: 0
    property bool compositorOutput: false
    property url coverSource: "qrc:/assets/demo-covers/square.png"
    property vector3d paperIrradiance: Qt.vector3d(1, 1, 1)
    readonly property Texture room: Texture {
        textureData: FixedTextureData {
            asset: "room-pmrem"
        }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        generateMipmaps: false
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    readonly property Texture integration: Texture {
        textureData: FixedTextureData {
            asset: "dfg"
        }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        generateMipmaps: false
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    readonly property Texture ltc1: Texture {
        textureData: FixedTextureData {
            asset: "ltc1"
        }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    readonly property Texture ltc2: Texture {
        textureData: FixedTextureData {
            asset: "ltc2"
        }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    component CaseGlass: GlassMaterial {
        lighting: materials.lighting
        lightingEnabled: materials.lightingEnabled
        roomTexture: materials.room
        integrationTexture: materials.integration
        ltc1Texture: materials.ltc1
        ltc2Texture: materials.ltc2
        warmth: materials.warmth
        compositorOutput: materials.compositorOutput
        paperIrradiance: materials.paperIrradiance
    }
    readonly property GlassMaterial lid: CaseGlass {
        surface: 0
        clarity: Math.max(0, Math.min(1, materials.clarity))
    }
    readonly property GlassMaterial spine: CaseGlass {
        surface: 1
    }
    readonly property GlassMaterial tray: CaseGlass {
        surface: 2
    }
    readonly property LightingMaterial paper: LightingMaterial {
        property real environmentIntensity: lighting.environmentIntensity
        property TextureInput environmentMap: TextureInput {
            texture: materials.room
        }
        lighting: materials.lighting
        lightingEnabled: materials.lightingEnabled
        property TextureInput coverMap: TextureInput {
            texture: Texture {
                source: materials.coverSource
                minFilter: Texture.Linear
                magFilter: Texture.Linear
                generateMipmaps: true
                mipFilter: Texture.Linear
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
            }
        }
        property vector3d paperIrradiance: materials.paperIrradiance
        shadingMode: CustomMaterial.Shaded
        cullMode: Material.BackFaceCulling
        vertexShader: "qrc:/assets/shaders/album-paper.vert"
        fragmentShader: "qrc:/assets/shaders/album-paper.frag"
    }
}
