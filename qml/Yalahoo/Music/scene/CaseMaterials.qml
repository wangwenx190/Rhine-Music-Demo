import QtQuick
import QtQuick3D
import Yalahoo.Music

Node {
    id: materials
    property real warmth: 1
    property real clarity: 0
    property url coverSource: "qrc:/assets/demo-covers/square.png"
    property vector3d paperIrradiance: Qt.vector3d(1,1,1)
    readonly property Texture room: Texture {
        textureData: FixedTextureData { asset: "room-pmrem" }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        generateMipmaps: false
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    readonly property Texture integration: Texture {
        textureData: FixedTextureData { asset: "dfg" }
        minFilter: Texture.Linear
        magFilter: Texture.Linear
        generateMipmaps: false
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
    }
    readonly property GlassMaterial lid: GlassMaterial {
        surface: 0
        roomTexture: materials.room
        integrationTexture: materials.integration
        warmth: materials.warmth
        clarity: Math.max(0,Math.min(1,materials.clarity))
        paperIrradiance: materials.paperIrradiance
    }
    readonly property GlassMaterial spine: GlassMaterial {
        surface: 1
        roomTexture: materials.room
        integrationTexture: materials.integration
        warmth: materials.warmth
        paperIrradiance: materials.paperIrradiance
    }
    readonly property GlassMaterial tray: GlassMaterial {
        surface: 2
        roomTexture: materials.room
        integrationTexture: materials.integration
        warmth: materials.warmth
        paperIrradiance: materials.paperIrradiance
    }
    readonly property CustomMaterial paper: CustomMaterial {
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
