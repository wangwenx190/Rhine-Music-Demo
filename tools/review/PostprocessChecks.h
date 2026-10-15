#pragma once
#include "RhiCaptureHarness.h"
#include <rhi/qrhi.h>

namespace yalahoo::review {
inline void checkPostprocessGraph(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto require = RhiCaptureHarness::require;
    require(capture.metadata["outputBindingMatches"].toBool(), "Final output binding is stale");
    const auto graph = capture.metadata["postprocessGraph"].toObject();
    if (!input.postprocess) {
        require(graph.isEmpty(), "Disabled postprocess retained an active graph");
        return;
    }
    QJsonArray expected;
    if (input.aoSamples) {
        expected.append(input.aoSamples == 32 ? "ssao" : QString("ssao%1").arg(input.aoSamples));
        expected.append("ssao-blur");
        expected.append("ssao-composite");
    }
    if (input.bokeh)
        expected.append("bokeh");
    if (input.smaa) {
        expected.append("smaa-edges");
        expected.append("smaa-weights");
        expected.append("smaa-blend");
    }
    require(graph["sourceMatches"].toBool() && graph["sourceSamples"].toInt() == 1
                    && graph["outputSamples"].toInt() == 1
                    && graph["sourceFormat"].toInt() == int(QRhiTexture::RGBA16F),
            "Postprocess does not consume resolved linear half-float color");
    const auto passes = graph["passes"].toArray();
    require(passes.size() == expected.size(), "Missing or unexpected postprocess passes");
    for (int i = 0; i < passes.size(); ++i) {
        const auto pass = passes[i].toObject();
        const bool scalar = expected[i] == "ssao" || expected[i] == "ssao16"
                || expected[i] == "ssao64" || expected[i] == "ssao-blur";
        require(pass["name"] == expected[i] && !pass["feedback"].toBool()
                        && pass["chainMatches"].toBool() && pass["samples"].toInt() == 1
                        && pass["pipelineSamples"].toInt() == 1
                        && pass["format"].toInt()
                                == int(scalar ? QRhiTexture::R16F : QRhiTexture::RGBA16F),
                "Postprocess ordering, format, sampling or feedback is invalid");
        const double scale = scalar ? input.aoResolution : 1;
        require(pass["width"].toInt() == std::max(1, int(capture.color.width() * scale))
                        && pass["height"].toInt()
                                == std::max(1, int(capture.color.height() * scale)),
                "Postprocess target dimensions are stale");
    }
}
}
