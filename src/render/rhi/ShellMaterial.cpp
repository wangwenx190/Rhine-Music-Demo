#include "ShellMaterial.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
// Source optical endpoints use float precision; warmth and per-entity clarity interpolate without retuning.
constexpr auto SrgbDecodeKnee = .04045f;
constexpr auto SrgbLinearSlope = 12.92f;
constexpr auto SrgbOffset = .055f;
constexpr auto SrgbScale = 1.055f;
constexpr auto SrgbExponent = 2.4f;
constexpr auto FrostedLidRoughness = .34f;
constexpr auto FrostedLidWarmthGain = .06f;
constexpr auto ClearLidRoughness = .27f;
constexpr auto ClearLidWarmthGain = .07f;
}

using namespace Qt::StringLiterals;

namespace {
enum ShellSurface {
    LidSurface,
    SpineSurface,
    TraySurface
};
struct OpticalEndpoints final
{
    float transmission, transmissionWarmthLoss;
    float roughness, roughnessWarmthGain;
    float thickness, thicknessWarmthGain;
    float attenuationDistance, distanceWarmthLoss;
};
constexpr OpticalEndpoints Lid{ .96f, .02f, 0, 0, .018f, .004f, 5, 1.2f };
constexpr OpticalEndpoints Spine{ .88f, .04f, .25f, .05f, .06f, .02f, 4.5f, 1.3f };
constexpr OpticalEndpoints Tray{ .8f, .1f, .38f, .08f, .038f, .012f, 4.5f, 2.5f };
constexpr float SpineCoat = .16f, OtherCoat = .08f;
constexpr float SpineCoatRoughness = .2f, OtherCoatRoughness = .38f;
}

namespace yalahoo::rhi {
namespace {
QVector3D linear(QLatin1StringView hex)
{
    QColor c(hex);
    const auto decode = [](float x) {
        return x <= SrgbDecodeKnee ? x / SrgbLinearSlope
                                   : std::pow((x + SrgbOffset) / SrgbScale, SrgbExponent);
    };
    return { decode(c.redF()), decode(c.greenF()), decode(c.blueF()) };
}
}
float lidRoughness(float warmth, float clarity)
{
    const float w = std::clamp(warmth, 0.0f, 1.0f), c = std::clamp(clarity, 0.0f, 1.0f);
    return (FrostedLidRoughness + FrostedLidWarmthGain * w) * (1 - c)
            + (ClearLidRoughness + ClearLidWarmthGain * w) * c;
}
ShellMaterial shellMaterial(int surface, float warmth, float clarity, float environment)
{
    if (surface < 0 || surface > TraySurface || !std::isfinite(warmth) || !std::isfinite(clarity)
        || !std::isfinite(environment))
        throw std::invalid_argument("Invalid RHI shell parameters");
    const float w = std::clamp(warmth, 0.0f, 1.0f), c = std::clamp(clarity, 0.0f, 1.0f);
    const QLatin1StringView day[] = { "#fffdf8"_L1, "#e7e1d8"_L1, "#ece6dc"_L1 };
    ShellMaterial m;
    m.tint = linear(surface == TraySurface ? "#c6d6e5"_L1 : "#f6fbff"_L1) * (1 - w)
            + linear(day[surface]) * w;
    m.attenuation = linear(surface == TraySurface ? "#f3e9db"_L1 : "#dceafd"_L1) * (1 - w)
            + linear("#f3e9db"_L1) * w;
    const auto &source = surface == LidSurface ? Lid : surface == SpineSurface ? Spine : Tray;
    // Vector order is transmission, roughness, thickness, attenuation distance in frame.glsl.
    m.transport = { source.transmission - source.transmissionWarmthLoss * w,
                    surface == LidSurface ? lidRoughness(w, c)
                                          : source.roughness + source.roughnessWarmthGain * w,
                    source.thickness + source.thicknessWarmthGain * w,
                    source.attenuationDistance - source.distanceWarmthLoss * w };
    m.coat = { surface == SpineSurface ? SpineCoat : OtherCoat,
               surface == SpineSurface ? SpineCoatRoughness : OtherCoatRoughness, float(surface),
               environment };
    return m;
}
}
