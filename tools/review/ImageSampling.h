// Samples CPU reference images at pixel-center coordinates with clamped bilinear filtering.
// Rendering equations and expected outputs remain independent in each reference algorithm.

#pragma once
#include <QSize>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace yalahoo::review {
template <typename Pixels>
typename Pixels::value_type sampleBilinear(const Pixels &pixels, QSize size, double x, double y)
{
    x -= .5;
    y -= .5;
    const int ix = int(std::floor(x)), iy = int(std::floor(y));
    const double fx = x - ix, fy = y - iy;
    typename Pixels::value_type result{ };
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            const auto &pixel = pixels[std::clamp(iy + row, 0, size.height() - 1) * size.width()
                                       + std::clamp(ix + column, 0, size.width() - 1)];
            const double weight = (row ? fy : 1 - fy) * (column ? fx : 1 - fx);
            for (std::size_t channel = 0; channel < result.size(); ++channel)
                result[channel] += pixel[channel] * weight;
        }
    }
    return result;
}
}
