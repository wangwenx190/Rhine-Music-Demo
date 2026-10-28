// Private postprocess resources and exact uniform storage shared with inspectors.
#pragma once
#include "Postprocess.h"
#include "RenderData.h"
#include <array>
#include <vector>
using Qt::StringLiterals::operator""_L1;
using Qt::StringLiterals::operator""_s;
namespace yalahoo::rhi {
namespace postDetail {
// Source postprocessing parameters retain their original scalar precision and uniform order.
constexpr auto AoMinimumDistance = .001f;
constexpr auto AoMaximumDistance = .035f;
constexpr auto MaximumBokehBlur = .011f;
constexpr auto AoCompositeStrength = .22f;

// Shader resource binding numbers match the postprocessing shader declarations.
enum PostBinding {
    PostInputBinding = 0,
    PostUniformBinding = 1,
    PostDepthBinding = 2,
    PostNormalBinding = 3,
    PostSamplesBinding = 4,
    PostNoiseBinding = 5,
    PostAuxiliaryBinding = 6,
    PostAreaBinding = 7,
    PostSearchBinding = 8
};
constexpr int UniformAlignmentBytes = 16;
constexpr int UniformVectorScalars = 4;
constexpr int MaximumKernelSamples = 64;
constexpr int PostPrefixBytes = 80;
constexpr int KernelScalarCount = 256;

template <class T>
using Owned = std::unique_ptr<T>;
struct ColorTarget final
{
    Q_DISABLE_COPY_MOVE(ColorTarget)
    ColorTarget() = default;
    Owned<QRhiTexture> texture;
    Owned<QRhiRenderPassDescriptor> pass;
    Owned<QRhiTextureRenderTarget> target;
    void create(QRhi *api, QSize size, QRhiTexture::Format format)
    {
        texture.reset(api->newTexture(
                format, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        require(texture->create(), "Postprocess texture creation failed");
        target.reset(api->newTextureRenderTarget({ texture.get() }));
        pass.reset(target->newCompatibleRenderPassDescriptor());
        target->setRenderPassDescriptor(pass.get());
        require(target->create(), "Postprocess target creation failed");
    }
};
struct Pass final
{
    Q_DISABLE_COPY(Pass)
    Pass() = default;
    Pass(Pass &&) = default;
    Pass &operator=(Pass &&) = default;
    QString name;
    ColorTarget *target = nullptr;
    Owned<QRhiBuffer> uniform;
    Owned<QRhiShaderResourceBindings> bindings;
    Owned<QRhiGraphicsPipeline> pipeline;
    std::array<std::byte, PostPrefixBytes> uploadedPrefix{ };
};
struct alignas(UniformAlignmentBytes) PostPrefix final
{
    float size[UniformVectorScalars], clip[UniformVectorScalars], ao[UniformVectorScalars],
            dof[UniformVectorScalars], options[UniformVectorScalars];
};
struct alignas(UniformAlignmentBytes) PostUniform final
{
    PostPrefix prefix;
    float kernel[MaximumKernelSamples][UniformVectorScalars];
};
static_assert(sizeof(PostPrefix) == PostPrefixBytes);
static_assert(sizeof(PostUniform) == renderData::PostUniformBytes
              && offsetof(PostUniform, kernel) == PostPrefixBytes);
}
using namespace postDetail;
struct Postprocess::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private();
    ~Private();
    struct State;
    std::unique_ptr<State> state;
    quint64 revision = 0;
};
struct Postprocess::Private::State final
{
    Q_DISABLE_COPY_MOVE(State)
    State() = default;
    QRhi *api = nullptr;
    PostSettings settings;
    std::array<ColorTarget, 2> ping;
    ColorTarget rawAo, blur, edges, weights;
    Owned<QRhiTexture> noise, area, search;
    Owned<QRhiSampler> linear, nearest, repeat;
    QRhiTexture *source = nullptr, *normal = nullptr, *aoDepth = nullptr, *dofDepth = nullptr;
    QRhiTexture *output = nullptr;
    bool upload = true;
    qint64 uniformUploadBytes = 0;
    std::array<float, KernelScalarCount> kernelValues{ };
    std::vector<Pass> passes;
    void add(const QString &name, ColorTarget &target, QRhiTexture *input, QRhiTexture *auxiliary)
    {
        Pass p;
        p.name = name;
        p.target = &target;
        p.uniform.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                       sizeof(PostUniform)));
        require(p.uniform->create(), "Postprocess uniform creation failed");
        p.bindings.reset(api->newShaderResourceBindings());
        const auto fragment = QRhiShaderResourceBinding::FragmentStage;
        p.bindings->setBindings(
                { QRhiShaderResourceBinding::sampledTexture(PostInputBinding, fragment, input,
                                                            linear.get()),
                  QRhiShaderResourceBinding::uniformBuffer(PostUniformBinding, fragment,
                                                           p.uniform.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          PostDepthBinding, fragment, p.name == "bokeh"_L1 ? dofDepth : aoDepth,
                          nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(PostNormalBinding, fragment, normal,
                                                            nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(PostSamplesBinding, fragment, source,
                                                            nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          PostNoiseBinding, fragment, noise ? noise.get() : source, repeat.get()),
                  QRhiShaderResourceBinding::sampledTexture(PostAuxiliaryBinding, fragment,
                                                            auxiliary, linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          PostAreaBinding, fragment, area ? area.get() : source, linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(PostSearchBinding, fragment,
                                                            search ? search.get() : source,
                                                            nearest.get()) });
        require(p.bindings->create(), "Postprocess bindings failed");
        p.pipeline.reset(api->newGraphicsPipeline());
        const auto path = ":/rhi/"_L1 + name + ".frag.qsb"_L1;
        p.pipeline->setShaderStages(
                { { QRhiShaderStage::Vertex, shader(u":/rhi/output.vert.qsb"_s) },
                  { QRhiShaderStage::Fragment, shader(path) } });
        p.pipeline->setShaderResourceBindings(p.bindings.get());
        p.pipeline->setRenderPassDescriptor(target.pass.get());
        require(p.pipeline->create(), "Postprocess pipeline failed");
        passes.push_back(std::move(p));
        output = target.texture.get();
    }
};
}
