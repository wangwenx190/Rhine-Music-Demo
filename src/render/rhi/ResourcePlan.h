#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "RhiItem.h"
#include "ResourceInventory.h"
#include <cmath>

namespace yalahoo::rhi {
inline QJsonObject resourcePlan(const Input &in, QSize size, bool diagnostic, int stride,
                                qint64 geometryBytes)
{
    ResourceInventory plan;
    const QSize shadow(qMax(1, in.shadowSize), qMax(1, in.shadowSize));
    plan.plannedTexture(u"shadow-depth"_s, shadow, QRhiTexture::D32F);
    plan.plannedTexture(u"shadow-readback-color"_s, shadow, QRhiTexture::R32F);
    auto color = [&](const QString &name, bool mips) {
        plan.plannedTexture(name + "-resolve"_L1, size, QRhiTexture::RGBA16F, 1, mips);
        if (in.captureSamples > 1)
            plan.plannedTexture(name + "-msaa"_L1, size, QRhiTexture::RGBA16F, in.captureSamples);
        if (diagnostic)
            for (const auto &channel : { u"normal"_s, u"depth"_s }) {
                plan.plannedTexture(name + "-"_L1 + channel, size, QRhiTexture::RGBA8);
                if (in.captureSamples > 1)
                    plan.plannedTexture(name + "-"_L1 + channel + "-msaa"_L1, size,
                                        QRhiTexture::RGBA8, in.captureSamples);
            }
    };
    color(u"opaque"_s, in.shells);
    if (in.shells)
        color(u"composite"_s, false);
    // Shared opaque/composite depth is counted once. Backend-private depth format upper bound.
    plan.plannedTexture(u"shared-main-depth-upper-bound"_s, size, QRhiTexture::D32FS8,
                        in.captureSamples);
    plan.plannedTexture(u"cover-or-atlas"_s, in.atlasGeneration ? in.atlasSize : in.cover.size(),
                        QRhiTexture::RGBA8);
    plan.plannedTexture(u"environment"_s, { 768, 1024 }, QRhiTexture::RGBA16F);
    plan.plannedTexture(u"dfg"_s, { 16, 16 }, QRhiTexture::RGBA32F);
    plan.plannedTexture(u"ltc1"_s, { 64, 64 }, QRhiTexture::RGBA32F);
    plan.plannedTexture(u"ltc2"_s, { 64, 64 }, QRhiTexture::RGBA32F);
    plan.plannedBuffer(u"geometry"_s, geometryBytes);
    plan.plannedBuffer(u"draw-uniforms"_s, qint64(stride) * (436 * 3 + 6));
    plan.plannedBuffer(u"instances"_s, 436 * 112);
    plan.plannedBuffer(u"output-uniform"_s, 16);
    plan.plannedTexture(u"qt-item-output"_s, size, QRhiTexture::RGBA8);
    plan.plannedTexture(u"qt-item-depth-upper-bound"_s, size, QRhiTexture::D32FS8);
    if (in.navigationDpr > 0) {
        const auto scaled = [&](int width, int height) {
            return QSize(int(std::ceil(width * in.navigationDpr)),
                         int(std::ceil(height * in.navigationDpr)));
        };
        // Qt owns these UI attachments. They join the cumulative descriptor plan, not RHI allocation.
        plan.plannedTexture(u"navigation-ruler-color"_s, scaled(130, 30), QRhiTexture::RGBA8);
        plan.plannedTexture(u"navigation-ruler-depth-upper-bound"_s, scaled(130, 30),
                            QRhiTexture::D32FS8);
        // Decimal wheels publish at most ten digits; widths include conservative glyph/spacing room.
        plan.plannedTexture(u"navigation-ordinal-layer-upper-bound"_s, scaled(700, 65),
                            QRhiTexture::RGBA8);
        for (int i = 0; i < 4; ++i)
            plan.plannedTexture(u"navigation-small-wheel-%1-upper-bound"_s.arg(i), scaled(160, 12),
                                QRhiTexture::RGBA8);
    }
    if (!in.panelLayerSize.isEmpty()) {
        // CSS sigma=8 logical-pixel blur uses two logical-pixel RGBA8 filter surfaces.
        // Their owners are the visible Popup Loader; closing destroys both captures.
        for (int i = 0; i < 2; ++i) {
            plan.plannedTexture(u"panel-blur-%1-color"_s.arg(i), in.panelLayerSize,
                                QRhiTexture::RGBA8);
            plan.plannedTexture(u"panel-blur-%1-depth-upper-bound"_s.arg(i), in.panelLayerSize,
                                QRhiTexture::D32FS8);
        }
    }
    if (!in.openingLayerSize.isEmpty()) {
        // Skip backdrop has two bounded logical-pixel textures, including 3 sigma padding.
        for (int i = 0; i < 2; ++i) {
            plan.plannedTexture(u"opening-skip-blur-%1-color-upper-bound"_s.arg(i), { 220, 120 },
                                QRhiTexture::RGBA8);
            plan.plannedTexture(u"opening-skip-blur-%1-depth-upper-bound"_s.arg(i), { 220, 120 },
                                QRhiTexture::D32FS8);
        }
        plan.plannedTexture(u"opening-canvas-color-upper-bound"_s, in.openingLayerSize,
                            QRhiTexture::RGBA8);
        plan.plannedTexture(u"opening-canvas-depth-upper-bound"_s, in.openingLayerSize,
                            QRhiTexture::D32FS8);
    }
    for (int i = 0; i < in.textLayerSizes.size(); ++i)
        plan.plannedTexture(u"title-reel-layer-%1"_s.arg(i), in.textLayerSizes[i],
                            QRhiTexture::RGBA8);
    for (int i = 0; i < in.textLayerSizes.size(); ++i)
        plan.plannedTexture(u"title-reel-depth-%1-upper-bound"_s.arg(i), in.textLayerSizes[i],
                            QRhiTexture::D32FS8);
    if (in.postprocess) {
        const QSize ao(qMax(1, int(size.width() * in.aoResolution)),
                       qMax(1, int(size.height() * in.aoResolution)));
        auto geometry = [&](const QString &name, QSize size) {
            plan.plannedTexture(name + "-normal"_L1, size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-depth"_L1, size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-hardware-depth-upper-bound"_L1, size, QRhiTexture::D32FS8);
        };
        const int colorPasses = int(in.aoSamples > 0) + int(in.bokeh) + int(in.smaa);
        for (int i = 0; i < qMin(2, colorPasses); ++i)
            plan.plannedTexture(u"post-ping-%1"_s.arg(i), size, QRhiTexture::RGBA16F);
        int passes = int(in.bokeh) + 3 * int(in.smaa);
        if (in.aoSamples) {
            geometry(u"ao"_s, ao);
            plan.plannedTexture(u"ao-raw"_s, ao, QRhiTexture::R16F);
            plan.plannedTexture(u"ao-blur"_s, ao, QRhiTexture::R16F);
            plan.plannedTexture(u"ao-noise"_s, { 4, 4 }, QRhiTexture::RGBA32F);
            passes += 3;
        }
        if (in.bokeh && (!in.aoSamples || ao != size))
            geometry(u"dof"_s, size);
        if (in.smaa) {
            plan.plannedTexture(u"smaa-edges"_s, size, QRhiTexture::RGBA16F);
            plan.plannedTexture(u"smaa-weights"_s, size, QRhiTexture::RGBA16F);
            plan.plannedTexture(u"smaa-area"_s, { 160, 560 }, QRhiTexture::RGBA8);
            plan.plannedTexture(u"smaa-search"_s, { 66, 33 }, QRhiTexture::RGBA8);
        }
        plan.plannedBuffer(u"post-uniforms"_s, 1104 * passes);
    }
    return {
        { u"entries"_s, plan.entries() },
        { u"estimatedBytesUpperBound"_s, plan.bytes() },
        { u"scope"_s,
          u"Preallocation descriptor plan; private depth formats use D32FS8 upper bound. Driver padding, PSO/SRB, swapchain/compositor and retired native storage are not measured."_s }
    };
}
}
