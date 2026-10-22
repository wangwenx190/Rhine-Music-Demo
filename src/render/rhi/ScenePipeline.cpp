#include "ScenePipeline.h"
#include "DrawUniform.h"

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
namespace {
constexpr int NormalOffsetBytes = 12;
constexpr int UvOffsetBytes = 24;

}
std::unique_ptr<QRhiGraphicsPipeline> createScenePipeline(QRhi *api, const Input &input,
                                                          const Target &target,
                                                          QRhiShaderResourceBindings *bindings,
                                                          ScenePass pass, InstanceMode instances)
{
    const bool isShadow = pass == ScenePass::Shadow;
    const bool isShell = pass == ScenePass::Shell;
    const bool instanced = instances == InstanceMode::Instanced;
    std::unique_ptr<QRhiGraphicsPipeline> p(api->newGraphicsPipeline());
    QString fragment = isShadow
            ? (target.color ? u":/rhi/shadow.frag.qsb"_s : u":/rhi/shadow-depth.frag.qsb"_s)
            : target.prepass ? u":/rhi/prepass.frag.qsb"_s
                             : u":/rhi/opaque.frag.qsb"_s;
    if (isShell) {
        fragment = input.areaIntensity > 0 ? u":/rhi/shell"_s : u":/rhi/shell-basic"_s;
        if (api->backend() != QRhi::OpenGLES2 || api->isFeatureSupported(QRhi::Compute))
            fragment += "-early"_L1;
        fragment += ".frag.qsb"_L1;
#ifdef YALAHOO_RHI_AUDIT
        if (input.opticalProbe > 0)
            fragment = u":/rhi/optics-probe.frag.qsb"_s;
#endif
    }
    p->setShaderStages({ { QRhiShaderStage::Vertex,
                           shader(isShadow ? (instanced ? u":/rhi/shadow-instanced.vert.qsb"_s
                                                        : u":/rhi/shadow.vert.qsb"_s)
                                           : (instanced ? u":/rhi/geometry-instanced.vert.qsb"_s
                                                        : u":/rhi/geometry.vert.qsb"_s)) },
                         { QRhiShaderStage::Fragment, shader(fragment) } });
    QRhiVertexInputLayout layout;
    layout.setBindings({ { renderData::VertexBytes } });
    layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                           { 0, 1, QRhiVertexInputAttribute::Float3, NormalOffsetBytes },
                           { 0, 2, QRhiVertexInputAttribute::Float2, UvOffsetBytes } });
    if (instanced) {
        layout.setBindings({ { renderData::VertexBytes },
                             { renderData::InstanceBytes, QRhiVertexInputBinding::PerInstance } });
        layout.setAttributes(
                { { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                  { 0, 1, QRhiVertexInputAttribute::Float3, NormalOffsetBytes },
                  { 0, 2, QRhiVertexInputAttribute::Float2, UvOffsetBytes },
                  { 1, 3, QRhiVertexInputAttribute::Float4, 0 },
                  { 1, 4, QRhiVertexInputAttribute::Float4, renderData::VectorBytes },
                  { 1, 5, QRhiVertexInputAttribute::Float4, InstanceMatrixSecondOffsetBytes },
                  { 1, 6, QRhiVertexInputAttribute::Float4, InstanceMatrixThirdOffsetBytes },
                  { 1, 7, QRhiVertexInputAttribute::Float4, InstanceUvOffsetBytes },
                  { 1, 8, QRhiVertexInputAttribute::Float4, InstanceLightOffsetBytes },
                  { 1, 9, QRhiVertexInputAttribute::Float4, InstanceSecondaryLightOffsetBytes } });
    }
    p->setVertexInputLayout(layout);
    p->setShaderResourceBindings(bindings);
    p->setRenderPassDescriptor(target.pass.get());
    p->setSampleCount(target.rt->sampleCount());
    p->setDepthTest(true);
    p->setDepthWrite(true);
    p->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    p->setFrontFace(QRhiGraphicsPipeline::CCW);
    // Three's shadow pass renders back faces for source FrontSide materials.
    p->setCullMode(isShadow ? QRhiGraphicsPipeline::Front : QRhiGraphicsPipeline::Back);
    if (!isShadow) {
        if (target.prepass)
            p->setTargetBlends({ { }, { } });
        else if (target.mrt)
            p->setTargetBlends({ { }, { }, { } });
        else
            p->setTargetBlends({ { } });
    } else if (!target.color) {
        p->setTargetBlends({ });
    }
    require(p->create(), "RHI graphics pipeline creation failed");
    return p;
}
}
