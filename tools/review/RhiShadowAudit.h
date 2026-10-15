#pragma once
#include "RhiCaptureHarness.h"
#include <QJsonArray>
#include <cmath>
namespace yalahoo::review {
inline void checkSubmittedShadow(const rhi::Input &input, const rhi::Capture &capture)
{
    const auto state = capture.metadata["shadowState"].toObject();
    RhiCaptureHarness::require(state["enabled"].toInt() == int(input.shadows)
                                       && state["size"].toInt() == input.shadowSize
                                       && std::abs(state["radius"].toDouble() - 4) < 1e-7
                                       && std::abs(state["strength"].toDouble() - .32) < 1e-7
                                       && std::abs(state["normalBias"].toDouble() - .035) < 1e-7
                                       && std::abs(state["bias"].toDouble() + .0003) < 1e-9,
                               "Submitted shadow values differ from source music override");
    RhiCaptureHarness::require(state["frontCull"].toBool() && state["lessEqual"].toBool()
                                       && state["linearCompare"].toBool(),
                               "Shadow cull/comparison/filter differs from source");
    for (const auto &value : capture.metadata["submittedFogUniforms"].toArray()) {
        const auto submitted = value.toObject()["shadow"].toArray();
        const char *keys[] = { "enabled", "size", "radius", "strength", "normalBias", "bias" };
        RhiCaptureHarness::require(submitted.size() == 6, "Missing actual draw shadow values");
        for (int i = 0; i < 6; ++i)
            RhiCaptureHarness::require(submitted[i] == state[keys[i]],
                                       "Actual paper/shell shadow uniforms differ from receiver");
    }
    const int entities = input.entities.isEmpty() && !input.poolFrame ? 1 : input.entities.size();
    RhiCaptureHarness::require(state["casters"].toInt() == (input.shadows ? entities : 0),
                               "Shadow caster pool differs from source diffuser instances");
    const auto resources = capture.metadata["resources"].toObject()["entries"].toArray();
    int attachments = 0;
    for (const auto &value : resources) {
        const auto row = value.toObject();
        const auto name = row["name"].toString();
        if (name != "shadow-color-resolve" && name != "shadow-hardware-depth")
            continue;
        ++attachments;
        RhiCaptureHarness::require(
                row["format"].toInt()
                                == int(name == "shadow-hardware-depth" ? QRhiTexture::D32F
                                                                       : QRhiTexture::R32F)
                        && row["width"].toInt() == input.shadowSize
                        && row["height"].toInt() == input.shadowSize && row["samples"].toInt() == 1,
                "Shadow attachment format, size or sampling changed");
    }
    RhiCaptureHarness::require(attachments == 2, "Missing distinct shadow color/depth attachments");
}
}
