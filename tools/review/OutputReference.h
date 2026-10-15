#pragma once
#include <QVector3D>
#include <algorithm>

namespace yalahoo::review {
// Independent double-precision transcription of locked Three's OutputShader.
inline QVector3D outputReference(QVector3D c, double exposure)
{
    const double scale = exposure / .6;
    const double r = c.x() * scale, g = c.y() * scale, b = c.z() * scale;
    double values[3] = { .59719 * r + .35458 * g + .04823 * b, .07600 * r + .90834 * g + .01566 * b,
                         .02840 * r + .13383 * g + .83777 * b };
    for (auto &v : values)
        v = (v * (v + .0245786) - .000090537) / (v * (.983729 * v + .4329510) + .238081);
    return {
        float(std::clamp(1.60475 * values[0] - .53108 * values[1] - .07367 * values[2], 0., 1.)),
        float(std::clamp(-.10208 * values[0] + 1.10813 * values[1] - .00605 * values[2], 0., 1.)),
        float(std::clamp(-.00327 * values[0] - .07276 * values[1] + 1.07602 * values[2], 0., 1.))
    };
}
}
