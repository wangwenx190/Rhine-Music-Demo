#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "RhiCaptureHarness.h"
#include <rhi/qrhi.h>

namespace yalahoo::review {
inline void checkPostprocessGraph(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto require = RhiCaptureHarness::require;
    require(capture.metadata[u"outputBindingMatches"_s].toBool(), "Final output binding is stale");
    const auto graph = capture.metadata[u"postprocessGraph"_s].toObject();
    if (!input.postprocess) {
        require(graph.isEmpty(), "Disabled postprocess retained an active graph");
        return;
    }
    require(graph[u"colorLinearClamp"_s].toBool() && graph[u"depthNearestClamp"_s].toBool(),
            "Postprocess color/depth filtering differs from source");
    QJsonArray expected;
    if (input.aoSamples) {
        expected.append(input.aoSamples == 32 ? u"ssao"_s : u"ssao%1"_s.arg(input.aoSamples));
        expected.append(u"ssao-blur"_s);
        expected.append(u"ssao-composite"_s);
    }
    if (input.bokeh)
        expected.append(u"bokeh"_s);
    if (input.smaa) {
        expected.append(u"smaa-edges"_s);
        expected.append(u"smaa-weights"_s);
        expected.append(u"smaa-blend"_s);
    }
    require(graph[u"sourceMatches"_s].toBool() && graph[u"sourceSamples"_s].toInt() == 1
                    && graph[u"outputSamples"_s].toInt() == 1
                    && graph[u"sourceFormat"_s].toInt() == int(QRhiTexture::RGBA16F),
            "Postprocess does not consume resolved linear half-float color");
    const auto passes = graph[u"passes"_s].toArray();
    require(passes.size() == expected.size(), "Missing or unexpected postprocess passes");
    for (int i = 0; i < passes.size(); ++i) {
        const auto pass = passes[i].toObject();
        const bool scalar = expected[i] == u"ssao"_s || expected[i] == u"ssao16"_s
                || expected[i] == u"ssao64"_s || expected[i] == u"ssao-blur"_s;
        require(pass[u"name"_s] == expected[i] && !pass[u"feedback"_s].toBool()
                        && pass[u"chainMatches"_s].toBool() && pass[u"samples"_s].toInt() == 1
                        && pass[u"pipelineSamples"_s].toInt() == 1
                        && pass[u"format"_s].toInt()
                                == int(scalar ? QRhiTexture::R16F : QRhiTexture::RGBA16F),
                "Postprocess ordering, format, sampling or feedback is invalid");
        require(pass[u"width"_s].toInt() == capture.color.width()
                        && pass[u"height"_s].toInt() == capture.color.height(),
                "Postprocess target dimensions are stale");
    }
}
}
