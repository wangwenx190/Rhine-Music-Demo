// Evaluates display transfer expectations from linear-light color values. Reference equations
// retain the standard SDR/HDR units used by production shaders.

#pragma once
#include <QVector3D>
#include <algorithm>

namespace yalahoo::review {
namespace outputFit {
// Independent locked ACES fit coefficients; reference precision remains double.
inline constexpr auto ExposureNormalization = .6;
inline constexpr auto NumeratorLinear = .0245786;
inline constexpr auto NumeratorBias = .000090537;
inline constexpr auto DenominatorQuadratic = .983729;
inline constexpr auto DenominatorLinear = .4329510;
inline constexpr auto DenominatorBias = .238081;
}

// Independent double-precision transcription of locked Three's OutputShader.
inline QVector3D outputReference(QVector3D c, double exposure)
{
    const double scale = exposure / outputFit::ExposureNormalization;
    const double r = c.x() * scale, g = c.y() * scale, b = c.z() * scale;
    double values[3] = { .59719 * r + .35458 * g + .04823 * b, .07600 * r + .90834 * g + .01566 * b,
                         .02840 * r + .13383 * g + .83777 * b };
    for (auto &v : values)
        v = (v * (v + outputFit::NumeratorLinear) - outputFit::NumeratorBias)
                / (v * (outputFit::DenominatorQuadratic * v + outputFit::DenominatorLinear)
                   + outputFit::DenominatorBias);
    return {
        float(std::clamp(1.60475 * values[0] - .53108 * values[1] - .07367 * values[2], 0., 1.)),
        float(std::clamp(-.10208 * values[0] + 1.10813 * values[1] - .00605 * values[2], 0., 1.)),
        float(std::clamp(-.00327 * values[0] - .07276 * values[1] + 1.07602 * values[2], 0., 1.))
    };
}
}
