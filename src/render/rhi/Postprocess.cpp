#include "RenderData.h"
#include "Postprocess.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include <QImage>
#include <QtEndian>
#include <array>
#include <algorithm>
#include <cstring>

namespace {
// Source postprocessing parameters retain their original scalar precision and uniform order.
constexpr auto AoMinimumDistance = .001f;
constexpr auto AoMaximumDistance = .035f;
constexpr auto MaximumBokehBlur = .011f;
constexpr auto AoCompositeStrength = .22f;
}

using namespace Qt::StringLiterals;

namespace {
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
}

namespace yalahoo::rhi {
namespace {
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
struct Postprocess::State final
{
    Q_DISABLE_COPY_MOVE(State)
    State() = default;
    QRhi *api = nullptr;
    PostSettings settings;
#ifdef YALAHOO_RHI_AUDIT
    PostPrefix submittedBokeh{ };
#endif
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
Postprocess::Postprocess() = default;
Postprocess::~Postprocess() = default;
void Postprocess::reset()
{
    m_state.reset();
    ++m_revision;
}
void Postprocess::prepare(QRhi *api, const PostSettings &s, QRhiTexture *color, QRhiTexture *normal,
                          QRhiTexture *aoDepth, QRhiTexture *dofDepth)
{
    if (m_state && m_state->api == api && m_state->settings.size == s.size
        && m_state->settings.samples == s.samples && m_state->settings.bokeh == s.bokeh
        && m_state->settings.smaa == s.smaa && m_state->source == color && m_state->normal == normal
        && m_state->aoDepth == aoDepth && m_state->dofDepth == dofDepth) {
        m_state->settings = s;
        return;
    }
    reset();
    m_state = std::make_unique<State>();
    auto &p = *m_state;
    p.api = api;
    p.settings = s;
    p.source = color;
    p.normal = normal;
    p.aoDepth = aoDepth;
    p.dofDepth = dofDepth;
    p.output = color;
    const int colorPasses = int(s.samples > 0) + int(s.bokeh) + int(s.smaa);
    for (int i = 0; i < std::min(2, colorPasses); ++i)
        p.ping[i].create(api, s.size, QRhiTexture::RGBA16F);
    const QSize aoSize = s.size;
    if (s.samples) {
        // Both shaders output (r,r,r,1), and every consumer reads .r only. Keep precisely
        // the same half-float component and filtering; the other stored channels carry no data.
        require(api->isTextureFormatSupported(QRhiTexture::R16F, QRhiTexture::RenderTarget),
                "Selected GPU lacks the half-float scalar AO attachment");
        p.rawAo.create(api, aoSize, QRhiTexture::R16F);
        p.blur.create(api, aoSize, QRhiTexture::R16F);
    }
    if (s.smaa) {
        p.edges.create(api, s.size, QRhiTexture::RGBA16F);
        p.weights.create(api, s.size, QRhiTexture::RGBA16F);
    }
    auto sampler = [&](Owned<QRhiSampler> &dest, QRhiSampler::Filter filter,
                       QRhiSampler::AddressMode address) {
        dest.reset(api->newSampler(filter, filter, QRhiSampler::None, address, address));
        require(dest->create(), "Postprocess sampler failed");
    };
    sampler(p.linear, QRhiSampler::Linear, QRhiSampler::ClampToEdge);
    sampler(p.nearest, QRhiSampler::Nearest, QRhiSampler::ClampToEdge);
    sampler(p.repeat, QRhiSampler::Nearest, QRhiSampler::Repeat);
    auto texture = [&](Owned<QRhiTexture> &dest, QRhiTexture::Format format, QSize size) {
        dest.reset(api->newTexture(format, size));
        require(dest->create(), "Postprocess lookup failed");
    };
    if (s.samples) {
        texture(p.noise, QRhiTexture::RGBA32F, { renderData::NoiseSide, renderData::NoiseSide });
    }
    if (s.smaa) {
        texture(p.area, QRhiTexture::RGBA8,
                { renderData::SmaaAreaWidth, renderData::SmaaAreaHeight });
        texture(p.search, QRhiTexture::RGBA8,
                { renderData::SmaaSearchWidth, renderData::SmaaSearchHeight });
    }
    auto *current = color;
    int ping = 0;
    if (s.samples) {
        const auto name = s.samples == 32 ? u"ssao"_s : "ssao"_L1 + QString::number(s.samples);
        p.add(name, p.rawAo, color, color);
        p.add(u"ssao-blur"_s, p.blur, p.rawAo.texture.get(), color);
        p.add(u"ssao-composite"_s, p.ping[ping], color, p.blur.texture.get());
        current = p.output;
        ping ^= 1;
    }
    if (s.bokeh) {
        p.add(u"bokeh"_s, p.ping[ping], current, color);
        current = p.output;
        ping ^= 1;
    }
    if (s.smaa) {
        p.add(u"smaa-edges"_s, p.edges, current, color);
        p.add(u"smaa-weights"_s, p.weights, p.edges.texture.get(), color);
        p.add(u"smaa-blend"_s, p.ping[ping], current, p.weights.texture.get());
    }
}
void Postprocess::render(QRhiCommandBuffer *cb)
{
    auto &p = *m_state;
    const auto &s = p.settings;
    ResourceUpdates updates(p.api->nextResourceUpdateBatch());
    const bool firstUpload = p.upload;
    p.uniformUploadBytes = 0;
    if (p.upload) {
        auto floats = [&](QRhiTexture *texture, const QString &name) {
            const auto bytes =
                    preparedAssets().bytes.value(":/rhi/environment/"_L1 + name + ".rgba32f"_L1);
            require(bytes.size()
                            == texture->pixelSize().width() * texture->pixelSize().height()
                                    * renderData::VectorBytes,
                    "Invalid AO lookup size");
            updates->uploadTexture(texture,
                                   { { 0, 0, QRhiTextureSubresourceUploadDescription(bytes) } });
        };
        if (s.samples) {
            const auto bytes =
                    preparedAssets().bytes.value(":/rhi/environment/ssao-kernel-"_L1
                                                 + QString::number(s.samples) + ".rgba32f"_L1);
            require(bytes.size() == s.samples * renderData::VectorBytes,
                    "Invalid source AO kernel size");
            std::memcpy(p.kernelValues.data(), bytes.constData(), bytes.size());
            floats(p.noise.get(), "ssao-noise-"_L1 + QString::number(s.samples));
        }
        for (const auto &entry : { std::pair{ p.area.get(), u"smaa-area"_s },
                                   std::pair{ p.search.get(), u"smaa-search"_s } }) {
            if (!entry.first)
                continue;
            const auto image = preparedAssets().images.value(":/rhi/postprocess/"_L1 + entry.second
                                                             + ".png"_L1);
            require(image.size() == entry.first->pixelSize(), "Invalid SMAA lookup size");
            updates->uploadTexture(entry.first, image.convertToFormat(QImage::Format_RGBA8888));
        }
        p.upload = false;
    }
    for (const auto &pass : p.passes) {
        const auto size = pass.target->texture->pixelSize();
        PostPrefix u{ { float(size.width()), float(size.height()), s.projectionX, s.projectionY },
                      { s.nearPlane, s.farPlane, p.api->isYUpInFramebuffer() ? 1.0f : -1.0f, 0 },
                      { float(s.samples), s.radius, AoMinimumDistance, AoMaximumDistance },
                      { s.focus, s.aperture, MaximumBokehBlur,
                        float(s.size.width()) / s.size.height() },
                      { AoCompositeStrength, 0, 0, 0 } };
        if (firstUpload
            && (pass.name == "ssao"_L1 || pass.name == "ssao16"_L1 || pass.name == "ssao64"_L1)) {
            const int bytes = s.samples * renderData::VectorBytes;
            updates->updateDynamicBuffer(pass.uniform.get(), offsetof(PostUniform, kernel), bytes,
                                         p.kernelValues.data());
            p.uniformUploadBytes += bytes;
        }
#ifdef YALAHOO_RHI_AUDIT
        if (pass.name == "bokeh"_L1)
            p.submittedBokeh = u;
#endif
        updates->updateDynamicBuffer(pass.uniform.get(), 0, sizeof(u), &u);
        p.uniformUploadBytes += sizeof(u);
    }
    cb->resourceUpdate(updates.release());
    for (const auto &pass : p.passes) {
        const auto size = pass.target->texture->pixelSize();
        cb->beginPass(pass.target->target.get(), Qt::transparent, { 1, 0 });
        cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });
        cb->setGraphicsPipeline(pass.pipeline.get());
        cb->setShaderResources();
        cb->draw(yalahoo::rhi::renderData::FullscreenTriangleVertices);
        cb->endPass();
    }
}
QRhiTexture *Postprocess::output() const
{
    return m_state ? m_state->output : nullptr;
}
QRhiTexture *Postprocess::rawAo() const
{
    return m_state->rawAo.texture.get();
}
QRhiTexture *Postprocess::blurredAo() const
{
    return m_state->blur.texture.get();
}
QRhiTexture *Postprocess::smaaEdges() const
{
#ifdef YALAHOO_RHI_AUDIT
    return m_state ? m_state->edges.texture.get() : nullptr;
#else
    return nullptr;
#endif
}
QRhiTexture *Postprocess::smaaWeights() const
{
#ifdef YALAHOO_RHI_AUDIT
    return m_state ? m_state->weights.texture.get() : nullptr;
#else
    return nullptr;
#endif
}
QJsonArray Postprocess::passes() const
{
    QJsonArray result;
    if (m_state)
        for (const auto &pass : m_state->passes)
            result.append(pass.name);
    return result;
}
QJsonObject Postprocess::graph(QRhiTexture *expectedSource) const
{
#ifdef YALAHOO_RHI_AUDIT
    if (m_state) {
        const auto &p = *m_state;
        QJsonArray passes;
        auto *currentColor = p.source;
        for (const auto &pass : p.passes) {
            auto *expectedInput = currentColor;
            auto *expectedAuxiliary = p.source;
            if (pass.name == "ssao-blur"_L1)
                expectedInput = p.rawAo.texture.get();
            if (pass.name == "ssao-composite"_L1)
                expectedAuxiliary = p.blur.texture.get();
            if (pass.name == "smaa-weights"_L1)
                expectedInput = p.edges.texture.get();
            if (pass.name == "smaa-blend"_L1)
                expectedAuxiliary = p.weights.texture.get();
            auto matches = [&](int slot, QRhiTexture *texture, QRhiSampler *sampler) {
                const auto expected = QRhiShaderResourceBinding::sampledTexture(
                        slot, QRhiShaderResourceBinding::FragmentStage, texture, sampler);
                return std::find(pass.bindings->cbeginBindings(), pass.bindings->cendBindings(),
                                 expected)
                        != pass.bindings->cendBindings();
            };
            const bool chainMatches = matches(PostInputBinding, expectedInput, p.linear.get())
                    && matches(PostAuxiliaryBinding, expectedAuxiliary, p.linear.get())
                    && matches(PostDepthBinding, pass.name == "bokeh"_L1 ? p.dofDepth : p.aoDepth,
                               p.nearest.get());
            bool feedback = false;
            const auto *target = pass.target->texture.get();
            for (int binding :
                 { PostInputBinding, PostDepthBinding, PostNormalBinding, PostSamplesBinding,
                   PostNoiseBinding, PostAuxiliaryBinding, PostAreaBinding, PostSearchBinding }) {
                for (auto *sampler : { p.linear.get(), p.nearest.get(), p.repeat.get() }) {
                    const auto candidate = QRhiShaderResourceBinding::sampledTexture(
                            binding, QRhiShaderResourceBinding::FragmentStage,
                            pass.target->texture.get(), sampler);
                    feedback |= std::find(pass.bindings->cbeginBindings(),
                                          pass.bindings->cendBindings(), candidate)
                            != pass.bindings->cendBindings();
                }
            }
            passes.append(QJsonObject{ { u"name"_s, pass.name },
                                       { u"feedback"_s, feedback },
                                       { u"chainMatches"_s, chainMatches },
                                       { u"format"_s, int(target->format()) },
                                       { u"samples"_s, target->sampleCount() },
                                       { u"width"_s, target->pixelSize().width() },
                                       { u"height"_s, target->pixelSize().height() },
                                       { u"pipelineSamples"_s, pass.pipeline->sampleCount() } });
            if (pass.name == "ssao-composite"_L1 || pass.name == "bokeh"_L1
                || pass.name == "smaa-blend"_L1)
                currentColor = pass.target->texture.get();
        }
        return { { u"colorLinearClamp"_s,
                   p.linear->magFilter() == QRhiSampler::Linear
                           && p.linear->minFilter() == QRhiSampler::Linear
                           && p.linear->mipmapMode() == QRhiSampler::None
                           && p.linear->addressU() == QRhiSampler::ClampToEdge
                           && p.linear->addressV() == QRhiSampler::ClampToEdge },
                 { u"depthNearestClamp"_s,
                   p.nearest->magFilter() == QRhiSampler::Nearest
                           && p.nearest->minFilter() == QRhiSampler::Nearest
                           && p.nearest->mipmapMode() == QRhiSampler::None
                           && p.nearest->addressU() == QRhiSampler::ClampToEdge
                           && p.nearest->addressV() == QRhiSampler::ClampToEdge },
                 { u"sourceMatches"_s, p.source == expectedSource },
                 { u"revision"_s, qint64(m_revision) },
                 { u"sourceSamples"_s, p.source->sampleCount() },
                 { u"sourceFormat"_s, int(p.source->format()) },
                 { u"outputSamples"_s, p.output->sampleCount() },
                 { u"passes"_s, passes } };
    }
#else
    Q_UNUSED(expectedSource);
#endif
    return { };
}
QJsonObject Postprocess::submittedBokeh() const
{
#ifdef YALAHOO_RHI_AUDIT
    if (m_state && m_state->settings.bokeh) {
        const auto &p = *m_state;
        const auto &u = p.submittedBokeh;
        return { { u"focus"_s, u.dof[0] },
                 { u"aperture"_s, u.dof[1] },
                 { u"maxblur"_s, u.dof[2] },
                 { u"aspect"_s, u.dof[3] },
                 { u"near"_s, u.clip[0] },
                 { u"far"_s, u.clip[1] },
                 { u"width"_s, u.size[0] },
                 { u"height"_s, u.size[1] },
                 { u"depthWidth"_s, p.dofDepth->pixelSize().width() },
                 { u"depthHeight"_s, p.dofDepth->pixelSize().height() } };
    }
#endif
    return { };
}
qint64 Postprocess::uniformUploadBytes() const
{
    return m_state ? m_state->uniformUploadBytes : 0;
}
quint64 Postprocess::revision() const
{
    return m_revision;
}
void Postprocess::inventory(ResourceInventory &inventory) const
{
    if (!m_state)
        return;
    const auto &p = *m_state;
    for (int i = 0; i < 2; ++i)
        inventory.texture(u"post-ping-%1"_s.arg(i), p.ping[i].texture.get());
    inventory.texture(u"post-ao-raw"_s, p.rawAo.texture.get());
    inventory.texture(u"post-ao-blur"_s, p.blur.texture.get());
    inventory.texture(u"smaa-edges"_s, p.edges.texture.get());
    inventory.texture(u"smaa-weights"_s, p.weights.texture.get());
    inventory.texture(u"ao-noise"_s, p.noise.get());
    inventory.texture(u"smaa-area"_s, p.area.get());
    inventory.texture(u"smaa-search"_s, p.search.get());
    for (const auto &pass : p.passes)
        inventory.buffer("post-uniform-"_L1 + pass.name, pass.uniform.get());
}
}
