#include "ColorTransfer.h"
#include "ShellMaterial.h"
#include "OpticalParameters.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <stdexcept>

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
    ShellMaterial m;
    m.tint = linear(opticalSource::NightTint[surface]) * (1 - w)
            + linear(opticalSource::DayTint[surface]) * w;
    m.attenuation = linear(surface == opticalSource::Tray ? opticalSource::DayAttenuation
                                                          : opticalSource::NightAttenuation)
                    * (1 - w)
            + linear(opticalSource::DayAttenuation) * w;
    m.transport = { source.transmission, source.roughness, source.thickness,
                    source.attenuationDistance };
    m.coat = { source.clearcoat, source.coatRoughness, float(surface), environment };
    return m;
}
}
