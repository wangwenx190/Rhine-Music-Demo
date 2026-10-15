#pragma once
#include "RhiItem.h"
#include "RhiCaptureHarness.h"
#include <QJsonArray>
namespace yalahoo::review {
inline void checkSubmittedFog(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto uniforms = capture.metadata["submittedFogUniforms"].toArray();
    RhiCaptureHarness::require(!uniforms.isEmpty(), "No submitted fog uniforms");
    bool paper = false, shell = false;
    for (const auto &value : uniforms) {
        const auto row = value.toObject();
        const auto range = row["range"].toArray(), color = row["color"].toArray();
        RhiCaptureHarness::require(range.size() == 3 && color.size() == 3,
                                   "Incomplete submitted fog values");
        RhiCaptureHarness::require(range[0].toDouble() == float(input.scene.fogNear)
                                           && range[1].toDouble() == float(input.scene.fogFar)
                                           && range[2].toDouble() == int(input.fogEnabled),
                                   "Actual draw fog differs from scene snapshot");
        for (int i = 0; i < 3; ++i)
            RhiCaptureHarness::require(color[i].toDouble() == input.background[i],
                                       "Actual draw fog color is not the linear theme color");
        paper = paper || row["kind"].toInt() == 0;
        shell = shell || row["kind"].toInt() >= 3;
    }
    RhiCaptureHarness::require(!input.showPaper || paper, "No paper fog submission");
    RhiCaptureHarness::require(!input.shells || shell, "No shell fog submission");
    const auto output = capture.metadata["outputOptions"].toArray();
    RhiCaptureHarness::require(output.size() == 4 && output[3].toDouble() == input.exposure,
                               "Output exposure differs from actual uniform");
    if (!input.rawOutput && input.lightingEnabled && input.shells)
        RhiCaptureHarness::require(output[1].toDouble() == 0 && output[2].toDouble() == 1,
                                   "Production output must apply one ACES and sRGB encoding");
}
}
