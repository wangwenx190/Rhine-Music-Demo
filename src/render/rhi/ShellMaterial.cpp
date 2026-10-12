#include "ShellMaterial.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace yalahoo::rhi {
namespace {
QVector3D linear(const char *hex)
{
    QColor c(QString::fromLatin1(hex));
    const auto decode = [](float x) {
        return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
    };
    return { decode(c.redF()), decode(c.greenF()), decode(c.blueF()) };
}
}
ShellMaterial shellMaterial(int surface, float warmth, float clarity, float environment)
{
    if (surface < 0 || surface > 2 || !std::isfinite(warmth) || !std::isfinite(clarity)
        || !std::isfinite(environment))
        throw std::invalid_argument("Invalid RHI shell parameters");
    const float w = std::clamp(warmth, 0.0f, 1.0f), c = std::clamp(clarity, 0.0f, 1.0f);
    const char *day[] = { "#fffdf8", "#e7e1d8", "#ece6dc" };
    ShellMaterial m;
    m.tint = linear(surface == 2 ? "#c6d6e5" : "#f6fbff") * (1 - w) + linear(day[surface]) * w;
    m.attenuation = linear(surface == 2 ? "#f3e9db" : "#dceafd") * (1 - w) + linear("#f3e9db") * w;
    if (surface == 0)
        m.transport = { .96f - .02f * w, (.34f + .06f * w) * (1 - c) + (.27f + .07f * w) * c,
                        .018f + .004f * w, 5 - 1.2f * w };
    else if (surface == 1)
        m.transport = { .88f - .04f * w, .25f + .05f * w, .06f + .02f * w, 4.5f - 1.3f * w };
    else
        m.transport = { .8f - .1f * w, .38f + .08f * w, .038f + .012f * w, 4.5f - 2.5f * w };
    m.coat = { surface == 1 ? .16f : .08f, surface == 1 ? .2f : .38f, float(surface), environment };
    return m;
}
}
