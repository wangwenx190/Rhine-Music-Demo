#include "Postprocess.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include <QImage>
#include <QtEndian>
#include <array>
#include <cstring>

namespace yalahoo::rhi {
namespace {
template <class T>
using Owned = std::unique_ptr<T>;
struct ColorTarget
{
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
struct Pass
{
    QString name;
    ColorTarget *target = nullptr;
    Owned<QRhiBuffer> uniform;
    Owned<QRhiShaderResourceBindings> bindings;
    Owned<QRhiGraphicsPipeline> pipeline;
};
struct alignas(16) PostPrefix
{
    float size[4], clip[4], ao[4], dof[4], options[4];
};
struct alignas(16) PostUniform
{
    PostPrefix prefix;
    float kernel[64][4];
};
static_assert(sizeof(PostPrefix) == 80);
static_assert(sizeof(PostUniform) == 1104 && offsetof(PostUniform, kernel) == 80);
}
struct Postprocess::State
{
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
    std::array<float, 256> kernelValues{ };
    std::vector<Pass> passes;
    void add(const char *name, ColorTarget &target, QRhiTexture *input, QRhiTexture *auxiliary)
    {
        Pass p;
        p.name = QString::fromLatin1(name);
        p.target = &target;
        p.uniform.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                       sizeof(PostUniform)));
        require(p.uniform->create(), "Postprocess uniform creation failed");
        p.bindings.reset(api->newShaderResourceBindings());
        const auto fragment = QRhiShaderResourceBinding::FragmentStage;
        p.bindings->setBindings(
                { QRhiShaderResourceBinding::sampledTexture(0, fragment, input, linear.get()),
                  QRhiShaderResourceBinding::uniformBuffer(1, fragment, p.uniform.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          2, fragment, p.name == "bokeh" ? dofDepth : aoDepth, nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(3, fragment, normal, nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(4, fragment, source, nearest.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          5, fragment, noise ? noise.get() : source, repeat.get()),
                  QRhiShaderResourceBinding::sampledTexture(6, fragment, auxiliary, linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(7, fragment, area ? area.get() : source,
                                                            linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          8, fragment, search ? search.get() : source, nearest.get()) });
        require(p.bindings->create(), "Postprocess bindings failed");
        p.pipeline.reset(api->newGraphicsPipeline());
        const auto path = QByteArray(":/rhi/") + name + ".frag.qsb";
        p.pipeline->setShaderStages({ { QRhiShaderStage::Vertex, shader(":/rhi/output.vert.qsb") },
                                      { QRhiShaderStage::Fragment, shader(path.constData()) } });
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
        && m_state->settings.samples == s.samples && m_state->settings.resolution == s.resolution
        && m_state->settings.bokeh == s.bokeh && m_state->settings.smaa == s.smaa
        && m_state->source == color && m_state->normal == normal && m_state->aoDepth == aoDepth
        && m_state->dofDepth == dofDepth) {
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
    const QSize aoSize(std::max(1, int(s.size.width() * s.resolution)),
                       std::max(1, int(s.size.height() * s.resolution)));
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
        texture(p.noise, QRhiTexture::RGBA32F, { 4, 4 });
    }
    if (s.smaa) {
        texture(p.area, QRhiTexture::RGBA8, { 160, 560 });
        texture(p.search, QRhiTexture::RGBA8, { 66, 33 });
    }
    auto *current = color;
    int ping = 0;
    if (s.samples) {
        const auto name = s.samples == 32 ? QByteArray("ssao")
                                          : QByteArray("ssao") + QByteArray::number(s.samples);
        p.add(name.constData(), p.rawAo, color, color);
        p.add("ssao-blur", p.blur, p.rawAo.texture.get(), color);
        p.add("ssao-composite", p.ping[ping], color, p.blur.texture.get());
        current = p.output;
        ping ^= 1;
    }
    if (s.bokeh) {
        p.add("bokeh", p.ping[ping], current, color);
        current = p.output;
        ping ^= 1;
    }
    if (s.smaa) {
        p.add("smaa-edges", p.edges, current, color);
        p.add("smaa-weights", p.weights, p.edges.texture.get(), color);
        p.add("smaa-blend", p.ping[ping], current, p.weights.texture.get());
    }
}
void Postprocess::render(QRhiCommandBuffer *cb)
{
    auto &p = *m_state;
    const auto &s = p.settings;
    auto *updates = p.api->nextResourceUpdateBatch();
    const bool firstUpload = p.upload;
    p.uniformUploadBytes = 0;
    if (p.upload) {
        auto floats = [&](QRhiTexture *texture, const QString &name) {
            const auto bytes =
                    preparedAssets().bytes.value(":/rhi/environment/" + name + ".rgba32f");
            require(bytes.size()
                            == texture->pixelSize().width() * texture->pixelSize().height() * 16,
                    "Invalid AO lookup size");
            updates->uploadTexture(texture,
                                   { { 0, 0, QRhiTextureSubresourceUploadDescription(bytes) } });
        };
        if (s.samples) {
            const auto bytes = preparedAssets().bytes.value(
                    ":/rhi/environment/ssao-kernel-" + QString::number(s.samples) + ".rgba32f");
            require(bytes.size() == s.samples * 16, "Invalid source AO kernel size");
            std::memcpy(p.kernelValues.data(), bytes.constData(), bytes.size());
            floats(p.noise.get(), "ssao-noise-" + QString::number(s.samples));
        }
        for (const auto &entry : { std::pair{ p.area.get(), "smaa-area" },
                                   std::pair{ p.search.get(), "smaa-search" } }) {
            if (!entry.first)
                continue;
            const auto image = preparedAssets().images.value(
                    ":/rhi/postprocess/" + QString::fromLatin1(entry.second) + ".png");
            require(image.size() == entry.first->pixelSize(), "Invalid SMAA lookup size");
            updates->uploadTexture(entry.first, image.convertToFormat(QImage::Format_RGBA8888));
        }
        p.upload = false;
    }
    for (const auto &pass : p.passes) {
        const auto size = pass.target->texture->pixelSize();
        PostPrefix u{ { float(size.width()), float(size.height()), s.projectionX, s.projectionY },
                      { s.nearPlane, s.farPlane, p.api->isYUpInFramebuffer() ? 1.0f : -1.0f, 0 },
                      { float(s.samples), s.radius, .001f, .035f },
                      { s.focus, s.aperture, .011f, float(s.size.width()) / s.size.height() },
                      { .22f, 0, 0, 0 } };
        if (firstUpload
            && (pass.name == "ssao" || pass.name == "ssao16" || pass.name == "ssao64")) {
            const int bytes = s.samples * 16;
            updates->updateDynamicBuffer(pass.uniform.get(), offsetof(PostUniform, kernel), bytes,
                                         p.kernelValues.data());
            p.uniformUploadBytes += bytes;
        }
        updates->updateDynamicBuffer(pass.uniform.get(), 0, sizeof(u), &u);
        p.uniformUploadBytes += sizeof(u);
    }
    cb->resourceUpdate(updates);
    for (const auto &pass : p.passes) {
        const auto size = pass.target->texture->pixelSize();
        cb->beginPass(pass.target->target.get(), Qt::transparent, { 1, 0 });
        cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });
        cb->setGraphicsPipeline(pass.pipeline.get());
        cb->setShaderResources();
        cb->draw(3);
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
QJsonArray Postprocess::passes() const
{
    QJsonArray result;
    if (m_state)
        for (const auto &pass : m_state->passes)
            result.append(pass.name);
    return result;
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
        inventory.texture(QString("post-ping-%1").arg(i), p.ping[i].texture.get());
    inventory.texture("post-ao-raw", p.rawAo.texture.get());
    inventory.texture("post-ao-blur", p.blur.texture.get());
    inventory.texture("smaa-edges", p.edges.texture.get());
    inventory.texture("smaa-weights", p.weights.texture.get());
    inventory.texture("ao-noise", p.noise.get());
    inventory.texture("smaa-area", p.area.get());
    inventory.texture("smaa-search", p.search.get());
    for (const auto &pass : p.passes)
        inventory.buffer("post-uniform-" + pass.name, pass.uniform.get());
}
}
