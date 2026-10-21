// Computes attachment sizes and byte estimates from explicit formats and samples. Checked
// arithmetic bounds allocations before native resource creation.

#pragma once
#include "RenderData.h"
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "RhiItem.h"
#include "ResourceInventory.h"
#include <cmath>

namespace yalahoo::rhi {
namespace resourcePlanMetrics {
// Descriptor dimensions include conservative UI layer bounds at the actual requested DPR.
inline constexpr auto SmaaPassCount = 3;
inline constexpr auto AoPassCount = 3;
inline constexpr auto RulerWidthPixels = 130;
inline constexpr auto RulerHeightPixels = 30;
inline constexpr auto OrdinalWidthBoundPixels = 700;
inline constexpr auto OrdinalHeightBoundPixels = 65;
inline constexpr auto SmallWheelCount = 4;
inline constexpr auto SmallWheelWidthPixels = 160;
inline constexpr auto SmallWheelHeightPixels = 12;
inline constexpr auto SkipBlurWidthPixels = 220;
inline constexpr auto SkipBlurHeightPixels = 120;
}

inline QJsonObject resourcePlan(const Input &in, QSize size, bool diagnostic, int stride,
                                qint64 geometryBytes)
{
    ResourceInventory plan;
    const auto scaled = [&](int width, int height) {
        const double dpr = in.navigationDpr > 0 ? in.navigationDpr : 1;
        return QSize(qRound(width * dpr), qRound(height * dpr));
    };
    const QSize shadow(qMax(1, in.shadowSize), qMax(1, in.shadowSize));
    plan.plannedTexture(u"shadow-depth"_s, shadow, QRhiTexture::D32F);
    if (diagnostic)
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
                        QRhiTexture::RGBA8, 1, in.filterCovers);
    plan.plannedTexture(u"environment"_s,
                        { renderData::EnvironmentWidth, renderData::EnvironmentHeight },
                        QRhiTexture::RGBA16F);
    plan.plannedTexture(u"dfg"_s, { renderData::DfgSide, renderData::DfgSide },
                        QRhiTexture::RGBA32F);
    plan.plannedTexture(u"ltc1"_s, { renderData::LtcSide, renderData::LtcSide },
                        QRhiTexture::RGBA32F);
    plan.plannedTexture(u"ltc2"_s, { renderData::LtcSide, renderData::LtcSide },
                        QRhiTexture::RGBA32F);
    plan.plannedBuffer(u"geometry"_s, geometryBytes);
    plan.plannedBuffer(
            u"draw-uniforms"_s,
            qint64(stride)
                    * (renderData::MaximumCaseEntities * yalahoo::rhi::renderData::CaseSurfaceCount
                       + yalahoo::rhi::renderData::ExtraDrawUniforms));
    plan.plannedBuffer(u"instances"_s, renderData::MaximumCaseEntities * renderData::InstanceBytes);
    plan.plannedBuffer(u"output-uniform"_s, yalahoo::rhi::renderData::OutputUniformBytes);
    plan.plannedTexture(u"qt-item-output"_s, size,
                        in.hdrActive ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8);
    plan.plannedTexture(u"qt-item-depth-upper-bound"_s, size, QRhiTexture::D32FS8);
    if (in.hdrActive) {
        plan.plannedTexture(u"display-composition"_s, in.displayLayerSize, QRhiTexture::RGBA16F);
        plan.plannedTexture(u"display-composition-depth-upper-bound"_s, in.displayLayerSize,
                            QRhiTexture::D32FS8);
    }
    if (in.navigationDpr > 0) {
        // Qt owns these UI attachments. They join the cumulative descriptor plan, not RHI allocation.
        plan.plannedTexture(u"navigation-ruler-color"_s,
                            scaled(resourcePlanMetrics::RulerWidthPixels,
                                   resourcePlanMetrics::RulerHeightPixels),
                            QRhiTexture::RGBA8);
        plan.plannedTexture(u"navigation-ruler-depth-upper-bound"_s,
                            scaled(resourcePlanMetrics::RulerWidthPixels,
                                   resourcePlanMetrics::RulerHeightPixels),
                            QRhiTexture::D32FS8);
        // Decimal wheels publish at most ten digits; widths include conservative glyph/spacing room.
        plan.plannedTexture(u"navigation-ordinal-layer-upper-bound"_s,
                            scaled(resourcePlanMetrics::OrdinalWidthBoundPixels,
                                   resourcePlanMetrics::OrdinalHeightBoundPixels),
                            QRhiTexture::RGBA8);
        for (int i = 0; i < resourcePlanMetrics::SmallWheelCount; ++i)
            plan.plannedTexture(u"navigation-small-wheel-%1-upper-bound"_s.arg(i),
                                scaled(resourcePlanMetrics::SmallWheelWidthPixels,
                                       resourcePlanMetrics::SmallWheelHeightPixels),
                                QRhiTexture::RGBA8);
    }
    if (!in.panelLayerSize.isEmpty()) {
        // CSS sigma=8 blur uses two surfaces at the actual output DPR.
        // Their owners are the visible Popup Loader; closing destroys both captures.
        for (int i = 0; i < 2; ++i) {
            plan.plannedTexture(u"panel-blur-%1-color"_s.arg(i), in.panelLayerSize,
                                in.hdrActive ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, 1, true);
            plan.plannedTexture(u"panel-blur-%1-depth-upper-bound"_s.arg(i), in.panelLayerSize,
                                QRhiTexture::D32FS8);
        }
    }
    if (!in.openingLayerSize.isEmpty()) {
        // Skip backdrop includes 3 sigma padding at the actual output DPR.
        for (int i = 0; i < 2; ++i) {
            plan.plannedTexture(u"opening-skip-blur-%1-color-upper-bound"_s.arg(i),
                                scaled(resourcePlanMetrics::SkipBlurWidthPixels,
                                       resourcePlanMetrics::SkipBlurHeightPixels),
                                in.hdrActive ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, 1, true);
            plan.plannedTexture(u"opening-skip-blur-%1-depth-upper-bound"_s.arg(i),
                                scaled(resourcePlanMetrics::SkipBlurWidthPixels,
                                       resourcePlanMetrics::SkipBlurHeightPixels),
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
        const QSize ao = size;
        auto geometry = [&](const QString &name, QSize size) {
            plan.plannedTexture(name + "-normal"_L1, size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-depth"_L1, size, QRhiTexture::RGBA8);
            plan.plannedTexture(name + "-hardware-depth-upper-bound"_L1, size, QRhiTexture::D32FS8);
        };
        const int colorPasses = int(in.aoSamples > 0) + int(in.bokeh) + int(in.smaa);
        for (int i = 0; i < qMin(2, colorPasses); ++i)
            plan.plannedTexture(u"post-ping-%1"_s.arg(i), size, QRhiTexture::RGBA16F);
        int passes = int(in.bokeh) + resourcePlanMetrics::SmaaPassCount * int(in.smaa);
        if (in.aoSamples) {
            geometry(u"ao"_s, ao);
            plan.plannedTexture(u"ao-raw"_s, ao, QRhiTexture::R16F);
            plan.plannedTexture(u"ao-blur"_s, ao, QRhiTexture::R16F);
            plan.plannedTexture(u"ao-noise"_s, { renderData::NoiseSide, renderData::NoiseSide },
                                QRhiTexture::RGBA32F);
            passes += resourcePlanMetrics::AoPassCount;
        }
        if (in.bokeh && (!in.aoSamples || ao != size))
            geometry(u"dof"_s, size);
        if (in.smaa) {
            plan.plannedTexture(u"smaa-edges"_s, size, QRhiTexture::RGBA16F);
            plan.plannedTexture(u"smaa-weights"_s, size, QRhiTexture::RGBA16F);
            plan.plannedTexture(u"smaa-area"_s,
                                { renderData::SmaaAreaWidth, renderData::SmaaAreaHeight },
                                QRhiTexture::RGBA8);
            plan.plannedTexture(u"smaa-search"_s,
                                { renderData::SmaaSearchWidth, renderData::SmaaSearchHeight },
                                QRhiTexture::RGBA8);
        }
        plan.plannedBuffer(u"post-uniforms"_s, renderData::PostUniformBytes * passes);
    }
    return {
        { u"entries"_s, plan.entries() },
        { u"estimatedBytesUpperBound"_s, plan.bytes() },
        { u"scope"_s,
          u"Preallocation descriptor plan; private depth formats use D32FS8 upper bound. Driver padding, PSO/SRB, swapchain/compositor and retired native storage are not measured."_s }
    };
}
}
