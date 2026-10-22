// Runs the rhishadow component probe against production objects. Anonymous inputs and explicit
// completion keep observations separate from expected source values.

#pragma once
#include "Require.h"
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "RhiCaptureHarness.h"
#include <QJsonArray>
#include <cmath>
namespace yalahoo::review {
inline void checkSubmittedShadow(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto state = capture.metadata[u"shadowState"_s].toObject();
    yalahoo::require(state[u"enabled"_s].toInt() == int(input.shadows)
                             && state[u"size"_s].toInt() == input.shadowSize
                             && std::abs(state[u"radius"_s].toDouble() - 4) < 1e-7
                             && std::abs(state[u"strength"_s].toDouble() - .32) < 1e-7
                             && std::abs(state[u"normalBias"_s].toDouble() - .035) < 1e-7
                             && std::abs(state[u"bias"_s].toDouble() + .0003) < 1e-9,
                     "Submitted shadow values differ from source music override");
    yalahoo::require(state[u"frontCull"_s].toBool() && state[u"lessEqual"_s].toBool()
                             && state[u"linearCompare"_s].toBool(),
                     "Shadow cull/comparison/filter differs from source");
    for (const auto &value : capture.metadata[u"submittedFogUniforms"_s].toArray()) {
        const auto submitted = value.toObject()[u"shadow"_s].toArray();
        const char *keys[] = { "enabled", "size", "radius", "strength", "normalBias", "bias" };
        yalahoo::require(submitted.size() == 6, "Missing actual draw shadow values");
        for (int i = 0; i < 6; ++i)
            yalahoo::require(submitted[i] == state[QString::fromUtf8(keys[i])],
                             "Actual paper/shell shadow uniforms differ from receiver");
    }
    const int entities = input.entities.isEmpty() && !input.poolFrame ? 1 : input.entities.size();
    yalahoo::require(state[u"casters"_s].toInt() == (input.shadows ? entities : 0),
                     "Shadow caster pool differs from source diffuser instances");
    const auto resources = capture.metadata[u"resources"_s].toObject()[u"entries"_s].toArray();
    int attachments = 0;
    for (const auto &value : resources) {
        const auto row = value.toObject();
        const auto name = row[u"name"_s].toString();
        if (name != "shadow-color-resolve"_L1 && name != "shadow-hardware-depth"_L1)
            continue;
        ++attachments;
        yalahoo::require(row[u"format"_s].toInt()
                                         == int(name == "shadow-hardware-depth"_L1
                                                        ? QRhiTexture::D32F
                                                        : QRhiTexture::R32F)
                                 && row[u"width"_s].toInt() == input.shadowSize
                                 && row[u"height"_s].toInt() == input.shadowSize
                                 && row[u"samples"_s].toInt() == 1,
                         "Shadow attachment format, size or sampling changed");
    }
    yalahoo::require(attachments == 2, "Missing distinct shadow color/depth attachments");
}
}
