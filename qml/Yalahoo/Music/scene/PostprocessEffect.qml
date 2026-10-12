import QtQuick
import QtQuick3D
import Yalahoo.Music

Effect {
    id: effect
    required property Texture depthTexture
    property bool bokehEnabled: true
    property bool smaaEnabled: false
    property string bufferPrefix: "post"
    property real cameraNear: 5
    property real cameraFar: 300
    property real focus: 25
    property real aperture: .0018
    property real maxblur: .011
    property real aspect: 1
    property TextureInput depthMap: TextureInput { texture: effect.depthTexture }
    property TextureInput areaMap: TextureInput {
        texture: Texture { textureData: FixedTextureData { asset: "smaa-area" } minFilter: Texture.Linear; magFilter: Texture.Linear }
    }
    property TextureInput searchMap: TextureInput {
        texture: Texture { textureData: FixedTextureData { asset: "smaa-search" } minFilter: Texture.Nearest; magFilter: Texture.Nearest }
    }
    property TextureInput weightMap: TextureInput { texture: Texture {} }
    Buffer { id: dof; name: effect.bufferPrefix+"BokehColor"; format: Buffer.RGBA16F; textureFilterOperation: Buffer.Linear }
    Buffer { id: edges; name: effect.bufferPrefix+"SmaaEdges"; format: Buffer.RGBA16F; textureFilterOperation: Buffer.Linear }
    Buffer { id: weights; name: effect.bufferPrefix+"SmaaWeights"; format: Buffer.RGBA16F; textureFilterOperation: Buffer.Linear }
    readonly property Pass bokehPass: Pass {
        output: effect.smaaEnabled ? dof : null
        shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/bokeh.frag" }
    }
    readonly property Pass edgePass: Pass {
        output: edges
        commands: BufferInput { buffer: effect.bokehEnabled ? dof : null }
        shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/smaa-edges.frag" }
    }
    readonly property Pass weightPass: Pass {
        output: weights
        commands: BufferInput { buffer: edges }
        shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/smaa-weights.frag" }
    }
    readonly property Pass blendPass: Pass {
        // Final passes target the effect output; named buffers are intermediates.
        commands: [BufferInput { buffer: effect.bokehEnabled ? dof : null }, BufferInput { buffer: weights; sampler: "weightMap" }]
        shaders: Shader { stage: Shader.Fragment; shader: "qrc:/assets/shaders/smaa-blend.frag" }
    }
    passes: smaaEnabled ? bokehEnabled ? [bokehPass,edgePass,weightPass,blendPass] : [edgePass,weightPass,blendPass] : [bokehPass]
}
