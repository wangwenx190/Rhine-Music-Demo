// Runs the rhifog component probe against production objects. Anonymous inputs and explicit
// completion keep observations separate from expected source values.

#pragma once
#include "Require.h"
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "RhiItem.h"
#include "RhiCaptureHarness.h"
#include <QJsonArray>
namespace yalahoo::review {
inline void checkSubmittedFog(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto uniforms = capture.metadata[u"submittedFogUniforms"_s].toArray();
    yalahoo::require(!uniforms.isEmpty(), "No submitted fog uniforms");
    bool paper = false, shell = false;
    for (const auto &value : uniforms) {
        const auto row = value.toObject();
        const auto range = row[u"range"_s].toArray(), color = row[u"color"_s].toArray();
        yalahoo::require(range.size() == 3 && color.size() == 3, "Incomplete submitted fog values");
        yalahoo::require(range[0].toDouble() == float(input.scene.fogNear)
                                 && range[1].toDouble() == float(input.scene.fogFar)
                                 && range[2].toDouble() == int(input.fogEnabled),
                         "Actual draw fog differs from scene snapshot");
        for (int i = 0; i < 3; ++i)
            yalahoo::require(color[i].toDouble() == input.background[i],
                             "Actual draw fog color is not the linear theme color");
        paper = paper || row[u"kind"_s].toInt() == 0;
        shell = shell || row[u"kind"_s].toInt() >= 3;
    }
    yalahoo::require(!input.showPaper || paper, "No paper fog submission");
    yalahoo::require(!input.shells || shell, "No shell fog submission");
    const auto output = capture.metadata[u"outputOptions"_s].toArray();
    yalahoo::require(output.size() == 4 && output[3].toDouble() == input.exposure,
                     "Output exposure differs from actual uniform");
    if (!input.rawOutput && input.lightingEnabled && input.shells)
        yalahoo::require(output[1].toDouble() == 0 && output[2].toDouble() == 1,
                         "Production output must apply one ACES and sRGB encoding");
}
}
