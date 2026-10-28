#include "ColorTransfer.h"
#include "ShellMaterial.h"
#include "OpticalParameters.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <array>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
namespace {
QVector3D linear(QLatin1StringView hex)
{
    QColor c(hex);
    const auto decode = [](float x) {
        return colorTransfer::decode(x);
    };
    return { decode(c.redF()), decode(c.greenF()), decode(c.blueF()) };
}
struct LinearColors final
{
    std::array<QVector3D, opticalSource::SurfaceCount> day, night;
    QVector3D dayAttenuation, nightAttenuation;
};
const LinearColors &linearColors()
{
    // Hex endpoints are immutable. Decode at the original float precision once;
    // per-frame material interpolation keeps the original arithmetic order.
    static const auto colors = [] {
        LinearColors result;
        for (int surface = 0; surface < opticalSource::SurfaceCount; ++surface) {
            result.day[surface] = linear(opticalSource::DayTint[surface]);
            result.night[surface] = linear(opticalSource::NightTint[surface]);
        }
        result.dayAttenuation = linear(opticalSource::DayAttenuation);
        result.nightAttenuation = linear(opticalSource::NightAttenuation);
        return result;
    }();
    return colors;
}
}
float lidRoughness(float warmth, float clarity)
{
    return opticalSource::lidRoughness(std::clamp(warmth, 0.0f, 1.0f),
                                       std::clamp(clarity, 0.0f, 1.0f));
}
ShellMaterial shellMaterial(int surface, float warmth, float clarity, float environment)
{
    if (surface < 0 || surface >= opticalSource::SurfaceCount || !std::isfinite(warmth)
        || !std::isfinite(clarity) || !std::isfinite(environment))
        throw std::invalid_argument("Invalid RHI shell parameters");
    const float w = std::clamp(warmth, 0.0f, 1.0f), c = std::clamp(clarity, 0.0f, 1.0f);
    const auto source = opticalSource::parameters(surface, w, c);
    const auto &colors = linearColors();
    ShellMaterial m;
    m.tint = colors.night[surface] * (1 - w) + colors.day[surface] * w;
    m.attenuation =
            (surface == opticalSource::Tray ? colors.dayAttenuation : colors.nightAttenuation)
                    * (1 - w)
            + colors.dayAttenuation * w;
    m.transport = { source.transmission, source.roughness, source.thickness,
                    source.attenuationDistance };
    m.coat = { source.clearcoat, source.coatRoughness, float(surface), environment };
    return m;
}
}
