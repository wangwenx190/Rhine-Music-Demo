#pragma once
#include "RhiItem.h"
#include "ResourceInventory.h"
#include <cmath>

namespace yalahoo::rhi {
inline QJsonObject resourcePlan(const Input &in, QSize size, bool diagnostic, int stride,
                                qint64 geometryBytes)
{
    ResourceInventory plan;
    const QSize shadow(qMax(1, in.shadowSize), qMax(1, in.shadowSize));
    plan.plannedTexture("shadow-depth", shadow, QRhiTexture::D32F);
    plan.plannedTexture("shadow-readback-color", shadow, QRhiTexture::R32F);
    auto color = [&](const QString &name, bool mips) {
        plan.plannedTexture(name + "-resolve", size, QRhiTexture::RGBA16F, 1, mips);
        if (in.captureSamples > 1)
            plan.plannedTexture(name + "-msaa", size, QRhiTexture::RGBA16F, in.captureSamples);
        if (diagnostic)
            for (const auto &channel : { "normal", "depth" }) {
                plan.plannedTexture(name + "-" + channel, size, QRhiTexture::RGBA8);
                if (in.captureSamples > 1)
                    plan.plannedTexture(name + "-" + channel + "-msaa", size, QRhiTexture::RGBA8,
                                        in.captureSamples);
            }
    };
    color("opaque", in.shells);
    if (in.shells)
        color("composite", false);
    // Shared opaque/composite depth is counted once. Backend-private depth format upper bound.
    plan.plannedTexture("shared-main-depth-upper-bound", size, QRhiTexture::D32FS8,
                        in.captureSamples);
    plan.plannedTexture("cover-or-atlas", in.atlasGeneration ? in.atlasSize : in.cover.size(),
                        QRhiTexture::RGBA8);
    plan.plannedTexture("environment", { 768, 1024 }, QRhiTexture::RGBA16F);
    plan.plannedTexture("dfg", { 16, 16 }, QRhiTexture::RGBA32F);
    plan.plannedTexture("ltc1", { 64, 64 }, QRhiTexture::RGBA32F);
    plan.plannedTexture("ltc2", { 64, 64 }, QRhiTexture::RGBA32F);
    plan.plannedBuffer("geometry", geometryBytes);
    plan.plannedBuffer("draw-uniforms", qint64(stride) * (436 * 3 + 6));
    plan.plannedBuffer("instances", 436 * 112);
    plan.plannedBuffer("output-uniform", 16);
    plan.plannedTexture("qt-item-output", size, QRhiTexture::RGBA8);
    plan.plannedTexture("qt-item-depth-upper-bound", size, QRhiTexture::D32FS8);
    if (in.navigationDpr > 0) {
        const auto scaled = [&](int width, int height) {
            return QSize(int(std::ceil(width * in.navigationDpr)),
                         int(std::ceil(height * in.navigationDpr)));
        };
        // Qt owns these UI attachments. They join the cumulative descriptor plan, not RHI allocation.
        plan.plannedTexture("navigation-ruler-color", scaled(130, 30), QRhiTexture::RGBA8);
        plan.plannedTexture("navigation-ruler-depth-upper-bound", scaled(130, 30),
                            QRhiTexture::D32FS8);
        // Decimal wheels publish at most ten digits; widths include conservative glyph/spacing room.
        plan.plannedTexture("navigation-ordinal-layer-upper-bound", scaled(700, 65),
                            QRhiTexture::RGBA8);
        for (int i = 0; i < 4; ++i)
            plan.plannedTexture(QString("navigation-small-wheel-%1-upper-bound").arg(i),
                                scaled(160, 12), QRhiTexture::RGBA8);
    }
    for (int i = 0; i < in.textLayerSizes.size(); ++i)
        plan.plannedTexture(QString("title-reel-layer-%1").arg(i), in.textLayerSizes[i],
                            QRhiTexture::RGBA8);
    for (int i = 0; i < in.textLayerSizes.size(); ++i)
        plan.plannedTexture(QString("title-reel-depth-%1-upper-bound").arg(i), in.textLayerSizes[i],
                            QRhiTexture::D32FS8);
    if (in.postprocess) {
        const QSize ao(qMax(1, int(size.width() * in.aoResolution)),
                       qMax(1, int(size.height() * in.aoResolution)));
        auto geometry = [&](const QString &name, QSize size) {
            plan.plannedTexture(name + "-normal", size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-depth", size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-hardware-depth-upper-bound", size, QRhiTexture::D32FS8);
        };
        const int colorPasses = int(in.aoSamples > 0) + int(in.bokeh) + int(in.smaa);
        for (int i = 0; i < qMin(2, colorPasses); ++i)
            plan.plannedTexture(QString("post-ping-%1").arg(i), size, QRhiTexture::RGBA16F);
        int passes = int(in.bokeh) + 3 * int(in.smaa);
        if (in.aoSamples) {
            geometry("ao", ao);
            plan.plannedTexture("ao-raw", ao, QRhiTexture::R16F);
            plan.plannedTexture("ao-blur", ao, QRhiTexture::R16F);
            plan.plannedTexture("ao-kernel", { in.aoSamples, 1 }, QRhiTexture::RGBA32F);
            plan.plannedTexture("ao-noise", { 4, 4 }, QRhiTexture::RGBA32F);
            passes += 3;
        }
        if (in.bokeh && (!in.aoSamples || ao != size))
            geometry("dof", size);
        if (in.smaa) {
            plan.plannedTexture("smaa-edges", size, QRhiTexture::RGBA16F);
            plan.plannedTexture("smaa-weights", size, QRhiTexture::RGBA16F);
            plan.plannedTexture("smaa-area", { 160, 560 }, QRhiTexture::RGBA8);
            plan.plannedTexture("smaa-search", { 66, 33 }, QRhiTexture::RGBA8);
        }
        plan.plannedBuffer("post-uniforms", 1104 * passes);
    }
    return {
        { "entries", plan.entries() },
        { "estimatedBytesUpperBound", plan.bytes() },
        { "scope",
          "Preallocation descriptor plan; private depth formats use D32FS8 upper bound. Driver padding, PSO/SRB, swapchain/compositor and retired native storage are not measured." }
    };
}
}
