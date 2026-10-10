import QtQuick
import QtQuick3D
import Yalahoo.Music

Effect {
    id: effect
    required property RenderQuality quality
    required property Texture geometryTexture
    required property Texture depthTexture
    property int channel: 0 // 0 composite, 1 rawAO, 2 blur, 3 depth, 4 normal, 5 linearColor
    property real cameraNear: 5
    property real cameraFar: 300
    property real kernelRadius: .18
    property real minDistance: .001
    property real maxDistance: .035
    property real aoStrength: .22
    property vector3d projectionPlane: Qt.vector3d(1,1,0)
    property int kernelSize: quality.aoSamples
    property TextureInput geometryMap: TextureInput { texture: effect.geometryTexture }
    property TextureInput depthMap: TextureInput { texture: effect.depthTexture }
    // BufferInput replaces these placeholders per pass; declared properties give
    // the Qt shader generator responsibility for cross-backend bindings.
    property TextureInput rawMap: TextureInput { texture: Texture {} }
    property TextureInput blurMap: TextureInput { texture: Texture {} }
    property TextureInput samplesMap: TextureInput {
        texture: Texture {
            textureData: FixedTextureData { asset: "ssao-kernel-"+Math.max(16,effect.kernelSize) }
            minFilter: Texture.Nearest; magFilter: Texture.Nearest
        }
    }
    property TextureInput noiseMap: TextureInput {
        texture: Texture {
            textureData: FixedTextureData { asset: "ssao-noise-"+Math.max(16,effect.kernelSize) }
            minFilter: Texture.Nearest; magFilter: Texture.Nearest
            tilingModeHorizontal: Texture.Repeat; tilingModeVertical: Texture.Repeat
        }
    }
    Buffer { id: ao; name: "ssaoRaw"; format: Buffer.RGBA16F; sizeMultiplier: effect.quality.aoResolution; textureFilterOperation: Buffer.Linear }
    Buffer { id: blur; name: "ssaoBlur"; format: Buffer.RGBA16F; sizeMultiplier: effect.quality.aoResolution; textureFilterOperation: Buffer.Linear }
    passes: [
        Pass {
            output: ao
            shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/ssao.frag" }
        },
        Pass {
            output: blur
            commands: BufferInput { buffer: ao }
            shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/ssao-blur.frag" }
        },
        Pass {
            commands: [BufferInput { buffer: ao; sampler: "rawMap" },BufferInput { buffer: blur; sampler: "blurMap" }]
            shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/ssao-composite.frag" }
        }
    ]
}
