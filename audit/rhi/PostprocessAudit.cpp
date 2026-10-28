// Inspects submitted postprocess bindings, attachments and retained upload bytes.
#include "PostprocessState_p.h"
#include "ResourceInventory.h"
#include <algorithm>
#include <cstring>
using namespace Qt::StringLiterals;
namespace yalahoo::rhi {
QRhiTexture *Postprocess::smaaEdges() const
{
#ifdef YALAHOO_RHI_AUDIT
    return d->state ? d->state->edges.texture.get() : nullptr;
#else
    return nullptr;
#endif
}
QRhiTexture *Postprocess::smaaWeights() const
{
#ifdef YALAHOO_RHI_AUDIT
    return d->state ? d->state->weights.texture.get() : nullptr;
#else
    return nullptr;
#endif
}
QJsonArray Postprocess::passes() const
{
    QJsonArray result;
    if (d->state)
        for (const auto &pass : d->state->passes)
            result.append(pass.name);
    return result;
}
QJsonObject Postprocess::graph(QRhiTexture *expectedSource) const
{
#ifdef YALAHOO_RHI_AUDIT
    if (d->state) {
        const auto &p = *d->state;
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
                 { u"revision"_s, qint64(d->revision) },
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
    if (d->state && d->state->settings.bokeh) {
        const auto &p = *d->state;
        const auto pass = std::find_if(p.passes.cbegin(), p.passes.cend(), [](const auto &entry) {
            return entry.name == "bokeh"_L1;
        });
        if (pass == p.passes.cend())
            return { };
        PostPrefix u;
        std::memcpy(&u, pass->uploadedPrefix.data(), sizeof(u));
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
void Postprocess::inventory(ResourceInventory &inventory) const
{
    if (!d->state)
        return;
    const auto &p = *d->state;
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
