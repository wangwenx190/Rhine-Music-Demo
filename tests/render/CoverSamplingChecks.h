// Executes the production cover sampler with analytic UVs. The independent scalar reference
// decodes stored bytes before interpolation; no scene projection or image scoring is involved.
#pragma once
#include "Require.h"
#include "OffscreenRhi.h"
#include "PreparedAssets.h"
#include "CoverCache.h"
#include <QFloat16>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace yalahoo::review {
using Qt::StringLiterals::operator""_s;
namespace coverSamplingChecks {
using Pixel = std::array<double, 4>;
constexpr int ProbeCount = 12;
struct Probe final
{
    float u, v, lod, footprint;
};
static_assert(sizeof(Probe) == 4 * sizeof(float));
inline Pixel sample(const QImage &image, double u, double v)
{
    const double x = std::clamp(u * image.width() - .5, 0.0, double(image.width() - 1));
    const double y = std::clamp(v * image.height() - .5, 0.0, double(image.height() - 1));
    const int left = int(x), top = int(y);
    Pixel result{ };
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column) {
            const auto *bytes = image.constScanLine(std::min(top + row, image.height() - 1))
                    + 4 * std::min(left + column, image.width() - 1);
            const double weight =
                    (column ? x - left : 1 - (x - left)) * (row ? y - top : 1 - (y - top));
            const double alpha = bytes[3] / 255.0;
            for (int channel = 0; channel < 3; ++channel) {
                const double encoded = alpha > 0 ? bytes[channel] / (255.0 * alpha) : 0;
                const double linear = encoded <= .04045 ? encoded / 12.92
                                                        : std::pow((encoded + .055) / 1.055, 2.4);
                result[channel] += linear * alpha * weight;
            }
            result[3] += alpha * weight;
        }
    return result;
}
inline Pixel trilinear(const QList<QImage> &levels, double u, double v, double lod)
{
    const auto first = sample(levels[int(std::floor(lod))], u, v);
    const auto last = sample(levels[int(std::ceil(lod))], u, v);
    Pixel result;
    for (int channel = 0; channel < 4; ++channel)
        result[channel] = first[channel] * (1 - (lod - std::floor(lod)))
                + last[channel] * (lod - std::floor(lod));
    return result;
}
inline Pixel reference(const QList<QImage> &levels, const Probe &probe)
{
    if (!probe.footprint)
        return trilinear(levels, probe.u, probe.v, probe.lod);
    // The analytic footprint is four texels long and 1.5 texels wide: three overlapping taps.
    Pixel result{ };
    for (int tap = 0; tap < 3; ++tap) {
        const auto value = trilinear(levels, probe.u + (tap - 1) / 3.0, probe.v, std::log2(1.5));
        for (int channel = 0; channel < 4; ++channel)
            result[channel] += value[channel] / 3;
    }
    return result;
}
}
inline QJsonObject checkLinearCoverSampling()
{
    using namespace coverSamplingChecks;
    constexpr std::array<Probe, ProbeCount> probes{ { { .5f, .125f, 0, 0 },
                                                      { .25f, .375f, 0, 0 },
                                                      { .5f, .625f, 0, 0 },
                                                      { .375f, .875f, 0, 0 },
                                                      { -.5f, .375f, 0, 0 },
                                                      { 1.5f, .375f, 0, 0 },
                                                      { .5f, .5f, .5f, 0 },
                                                      { .3f, .7f, 1.25f, 0 },
                                                      { .5f, .5f, 2, 0 },
                                                      { .5f, .5f, 0, 1 },
                                                      { -.25f, .25f, 0, 1 },
                                                      { 1.25f, .75f, 0, 1 } } };
    QImage source(4, 4, QImage::Format_RGBA8888_Premultiplied);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const QColor colors[]{ Qt::black, Qt::white, QColor(220, 70, 30, 128),
                                   QColor(30, 160, 240, 64), Qt::transparent };
            source.setPixelColor(x, y, colors[y == 0 ? x % 2 : (x + y) % 5]);
        }
    QList<QImage> levels{ source };
    levels.append(CoverCache::makeMipmaps(source));
    rhi::preparedAssets();
    OffscreenRhi backend;
    auto *api = backend.get();
    const QSize outputSize(ProbeCount, 1);
    std::unique_ptr<QRhiTexture> atlas(
            api->newTexture(QRhiTexture::RGBA8, { 8, 4 }, 1, QRhiTexture::MipMapped));
    std::unique_ptr<QRhiTexture> output(
            api->newTexture(QRhiTexture::RGBA16F, outputSize, 1,
                            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    yalahoo::require(atlas->create() && output->create(), "Cannot create cover sampling targets");
    std::unique_ptr<QRhiTextureRenderTarget> target(api->newTextureRenderTarget({ output.get() }));
    std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    yalahoo::require(target->create(), "Cannot create cover sampling render target");
    std::array<Probe, ProbeCount * 2> uniforms;
    std::copy(probes.begin(), probes.end(), uniforms.begin());
    std::fill(uniforms.begin() + ProbeCount, uniforms.end(), Probe{ 1, 0, 0, .375f });
    std::unique_ptr<QRhiBuffer> buffer(
            api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(uniforms)));
    std::unique_ptr<QRhiSampler> sampler(
            api->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    yalahoo::require(buffer->create() && sampler->create(), "Cannot create cover sampling inputs");
    std::unique_ptr<QRhiShaderResourceBindings> bindings(api->newShaderResourceBindings());
    bindings->setBindings(
            { QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                        atlas.get(), sampler.get()),
              QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage,
                                                       buffer.get()) });
    yalahoo::require(bindings->create(), "Cannot create cover sampling bindings");
    std::unique_ptr<QRhiGraphicsPipeline> pipeline(api->newGraphicsPipeline());
    pipeline->setShaderStages(
            { { QRhiShaderStage::Vertex, rhi::shader(u":/rhi/output.vert.qsb"_s) },
              { QRhiShaderStage::Fragment, rhi::shader(u":/rhi/CoverSamplingProbe.frag.qsb"_s) } });
    pipeline->setShaderResourceBindings(bindings.get());
    pipeline->setRenderPassDescriptor(pass.get());
    yalahoo::require(pipeline->create(), "Cannot create cover sampling pipeline");
    QRhiCommandBuffer *cb = nullptr;
    yalahoo::require(api->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess,
                     "Cannot begin cover sampling frame");
    auto *updates = api->nextResourceUpdateBatch();
    for (int level = 0; level < levels.size(); ++level) {
        const auto &image = levels[level];
        QImage page(image.width() * 2, image.height(), image.format());
        page.fill(Qt::magenta);
        for (int row = 0; row < image.height(); ++row)
            std::memcpy(page.scanLine(row), image.constScanLine(row), image.width() * 4);
        updates->uploadTexture(atlas.get(),
                               { { 0, level, QRhiTextureSubresourceUploadDescription(page) } });
    }
    updates->updateDynamicBuffer(buffer.get(), 0, sizeof(uniforms), uniforms.data());
    cb->beginPass(target.get(), Qt::transparent, { 1, 0 }, updates);
    cb->setViewport({ 0, 0, float(ProbeCount), 1 });
    cb->setGraphicsPipeline(pipeline.get());
    cb->setShaderResources();
    cb->draw(3);
    cb->endPass();
    QRhiReadbackResult readback;
    auto *reads = api->nextResourceUpdateBatch();
    reads->readBackTexture(QRhiReadbackDescription(output.get()), &readback);
    cb->resourceUpdate(reads);
    yalahoo::require(api->endOffscreenFrame() == QRhi::FrameOpSuccess
                             && readback.data.size() == ProbeCount * 8,
                     "Cannot read cover sampling output");
    double maximumError = 0;
    for (int index = 0; index < ProbeCount; ++index) {
        const auto expected = reference(levels, probes[index]);
        for (int channel = 0; channel < 4; ++channel) {
            qfloat16 value;
            std::memcpy(&value, readback.data.constData() + index * 8 + channel * 2, 2);
            const double actual = float(value);
            yalahoo::require(std::isfinite(actual), "Nonfinite linear cover sampling output");
            maximumError = std::max(maximumError, std::abs(expected[channel] - actual));
        }
    }
    yalahoo::require(maximumError < .001,
                     "Cover filtering differs from linear premultiplied interpolation");
    return { { u"probes"_s, ProbeCount }, { u"maximumError"_s, maximumError } };
}
}
