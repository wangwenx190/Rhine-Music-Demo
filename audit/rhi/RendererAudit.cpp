// Inspects actual native attachments and isolates all Review shader substitutions.
#include "Renderer_p.h"
#include "RhiObservation.h"
#include <QObject>
using namespace Qt::StringLiterals;
namespace yalahoo::rhi {
bool opticalProbesAvailable()
{
#ifdef YALAHOO_RHI_AUDIT
    return true;
#else
    return false;
#endif
}
QString inspectedFragment(int probe, QString fragment)
{
#ifdef YALAHOO_RHI_AUDIT
    if (probe > 0)
        return u":/rhi/optics-probe.frag.qsb"_s;
#else
    Q_UNUSED(probe);
#endif
    return fragment;
}
void applySceneInspection(Input &next, const Input &previous, const QObject &item)
{
#ifdef YALAHOO_RHI_AUDIT
    next.opticalProbe = previous.opticalProbe;

    if (item.property("_yalahooReviewOpticsOnly").toBool()) {
        next.postprocess = false;
        next.aoSamples = 0;
        next.bokeh = false;
        next.smaa = false;
    }

#else
    Q_UNUSED(next);
    Q_UNUSED(previous);
    Q_UNUSED(item);
#endif
}
namespace renderDetail {
void Renderer::configureVisibilityProbe()
{
#ifdef YALAHOO_RHI_AUDIT
    constexpr int VisibilityProbe = 10;
    if (input.shells && visibilityProbeShader != (input.opticalProbe == VisibilityProbe)) {
        visibilityProbeShader = input.opticalProbe == VisibilityProbe;
        for (int instanced = 0; instanced < 2; ++instanced) {
            auto *p = instanced ? coverageInstances.get() : coveragePipeline.get();
            p->setShaderStages(
                    { { QRhiShaderStage::Vertex,
                        shader(instanced ? u":/rhi/geometry-instanced.vert.qsb"_s
                                         : u":/rhi/geometry.vert.qsb"_s) },
                      { QRhiShaderStage::Fragment,
                        shader(visibilityProbeShader ? u":/rhi/VisibilityProbe.frag.qsb"_s
                                                     : u":/rhi/coverage.frag.qsb"_s) } });
            QRhiGraphicsPipeline::TargetBlend color, disabled;
            disabled.colorWrite = { };
            if (!visibilityProbeShader)
                color.colorWrite = { };
            if (diagnosticTargets)
                p->setTargetBlends({ color, disabled, disabled });
            else
                p->setTargetBlends({ color });
            require(p->create(), "Visibility diagnostic pipeline failed");
        }
    }

#endif
}
void Renderer::inspectCapture(Capture &capture)
{
#ifdef YALAHOO_RHI_AUDIT

    QJsonArray resolveGraph;
    struct NamedTarget final
    {
        const char *name;
        Target *target;
    };
    for (const auto &[name, target] :
         { NamedTarget{ "opaque", &opaque }, NamedTarget{ "composite", &composite } }) {
        if (!target->rt)
            continue;
        QJsonArray attachments;
        const auto &description = target->rt->description();
        for (auto it = description.cbeginColorAttachments();
             it != description.cendColorAttachments(); ++it) {
            const auto *texture = it->texture();
            const auto *buffer = it->renderBuffer();
            const auto *resolved = it->resolveTexture();
            const auto size = texture ? texture->pixelSize() : buffer->pixelSize();
            attachments.append(QJsonObject{
                    { u"storage"_s, texture ? u"texture"_s : u"renderbuffer"_s },
                    { u"format"_s, int(texture ? texture->format() : buffer->backingFormat()) },
                    { u"samples"_s, texture ? texture->sampleCount() : buffer->sampleCount() },
                    { u"width"_s, size.width() },
                    { u"height"_s, size.height() },
                    { u"resourceId"_s,
                      qint64(texture ? texture->globalResourceId() : buffer->globalResourceId()) },
                    { u"resolveResourceId"_s,
                      resolved ? QJsonValue(qint64(resolved->globalResourceId())) : QJsonValue() },
                    { u"resolveFormat"_s, resolved ? int(resolved->format()) : 0 },
                    { u"resolveSamples"_s, resolved ? resolved->sampleCount() : 0 },
                    { u"resolveWidth"_s, resolved ? resolved->pixelSize().width() : 0 },
                    { u"resolveHeight"_s, resolved ? resolved->pixelSize().height() : 0 },
                    { u"separateResolve"_s, resolved && resolved != texture } });
        }
        resolveGraph.append(QJsonObject{ { u"name"_s, QString::fromUtf8(name) },
                                         { u"samples"_s, target->rt->sampleCount() },
                                         { u"pipelineSamples"_s,
                                           QString::fromUtf8(name) == u"opaque"_s
                                                   ? opaquePipeline->sampleCount()
                                                   : shellPipeline->sampleCount() },
                                         { u"depthSamples"_s, target->z->sampleCount() },
                                         { u"attachments"_s, attachments } });
    }
    capture.metadata[u"sampling"_s] =
            QJsonObject{ { u"coverTileLocalMipmaps"_s,
                           input.filterCovers && cover->flags().testFlag(QRhiTexture::MipMapped)
                                   && mipLinear->mipmapMode() == QRhiSampler::Linear },
                         { u"captureTrilinearClamp"_s,
                           mipLinear->magFilter() == QRhiSampler::Linear
                                   && mipLinear->minFilter() == QRhiSampler::Linear
                                   && mipLinear->mipmapMode() == QRhiSampler::Linear
                                   && mipLinear->addressU() == QRhiSampler::ClampToEdge
                                   && mipLinear->addressV() == QRhiSampler::ClampToEdge } };
    capture.metadata[u"resolveGraph"_s] = resolveGraph;
    capture.metadata[u"postprocessGraph"_s] = input.postprocess
            ? post.graph(input.shells ? composite.color.get() : opaque.color.get())
            : QJsonObject();
    const auto expectedOutput = QRhiShaderResourceBinding::sampledTexture(
            FrameUniformBinding, QRhiShaderResourceBinding::FragmentStage, outputSource,
            linear.get());
    capture.metadata[u"outputBindingMatches"_s] = outputSource
                    == (input.postprocess
                                ? post.output()
                                : (input.shells ? composite.color.get() : opaque.color.get()))
            && std::find(outputBindings->cbeginBindings(), outputBindings->cendBindings(),
                         expectedOutput)
                    != outputBindings->cendBindings();

#else
    Q_UNUSED(capture);
#endif
}
}
}
