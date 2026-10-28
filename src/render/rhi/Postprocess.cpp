#include "Postprocess.h"
#include "PostprocessState_p.h"
#include <vector>
#include "RenderData.h"
#include "PreparedAssets.h"
#include <QImage>
#include <QtEndian>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstddef>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
Postprocess::Private::Private() = default;
Postprocess::Private::~Private() = default;
Postprocess::Postprocess() : d(std::make_unique<Private>())
{
}
Postprocess::~Postprocess() = default;
void Postprocess::reset()
{
    d->state.reset();
    ++d->revision;
}
void Postprocess::prepare(QRhi *api, const PostSettings &s, QRhiTexture *color, QRhiTexture *normal,
                          QRhiTexture *aoDepth, QRhiTexture *dofDepth)
{
    if (d->state && d->state->api == api && d->state->settings.size == s.size
        && d->state->settings.samples == s.samples && d->state->settings.bokeh == s.bokeh
        && d->state->settings.smaa == s.smaa && d->state->source == color
        && d->state->normal == normal && d->state->aoDepth == aoDepth
        && d->state->dofDepth == dofDepth) {
        d->state->settings = s;
        return;
    }
    reset();
    d->state = std::make_unique<Private::State>();
    auto &p = *d->state;
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
    auto &p = *d->state;
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
        struct LookupTexture final
        {
            QRhiTexture *texture;
            QLatin1StringView name;
        };
        for (const auto &entry : { LookupTexture{ p.area.get(), "smaa-area"_L1 },
                                   LookupTexture{ p.search.get(), "smaa-search"_L1 } }) {
            if (!entry.texture)
                continue;
            const auto image =
                    preparedAssets().images.value(":/rhi/postprocess/"_L1 + entry.name + ".png"_L1);
            require(image.size() == entry.texture->pixelSize(), "Invalid SMAA lookup size");
            updates->uploadTexture(entry.texture, image.convertToFormat(QImage::Format_RGBA8888));
        }
        p.upload = false;
    }
    for (auto &pass : p.passes) {
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
        if (firstUpload || std::memcmp(pass.uploadedPrefix.data(), &u, sizeof(u)) != 0) {
            updates->updateDynamicBuffer(pass.uniform.get(), 0, sizeof(u), &u);
            std::memcpy(pass.uploadedPrefix.data(), &u, sizeof(u));
            p.uniformUploadBytes += sizeof(u);
        }
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
    return d->state ? d->state->output : nullptr;
}
QRhiTexture *Postprocess::rawAo() const
{
    return d->state->rawAo.texture.get();
}
QRhiTexture *Postprocess::blurredAo() const
{
    return d->state->blur.texture.get();
}
qint64 Postprocess::uniformUploadBytes() const
{
    return d->state ? d->state->uniformUploadBytes : 0;
}
quint64 Postprocess::revision() const
{
    return d->revision;
}

}
