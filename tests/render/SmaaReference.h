// Evaluates the source SMAA equations on deterministic sample images. Reference sampling and
// coordinate conventions match the production shader algorithm.

#pragma once
#include "ImageSampling.h"
// Independent double-precision transcription of locked Three.js SMAAShader.js (MIT).
// Original algorithm authors and license: resources/licenses/three-LICENSE.
#include <QImage>
#include <QFloat16>
#include <QSize>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

namespace smaa_reference {
using Pixel = std::array<double, 4>;
struct Image final
{
    QSize size;
    std::vector<Pixel> pixels;
    explicit Image(QSize extent) : size(extent), pixels(extent.width() * extent.height())
    {
    }
    Pixel &at(int x, int y)
    {
        return pixels[y * size.width() + x];
    }
    Pixel sample(double x, double y) const
    {
        return yalahoo::review::sampleBilinear(pixels, size, x, y);
    }
    void quantize()
    {
        for (auto &pixel : pixels)
            for (auto &value : pixel)
                value = float(qfloat16(float(value)));
    }
};
inline double delta(const Pixel &a, const Pixel &b)
{
    return std::max({ std::abs(a[0] - b[0]), std::abs(a[1] - b[1]), std::abs(a[2] - b[2]) });
}
inline Image edges(const Image &input)
{
    Image result(input.size);
    for (int y = 0; y < input.size.height(); ++y) {
        for (int x = 0; x < input.size.width(); ++x) {
            const double u = x + .5, v = y + .5;
            const auto center = input.sample(u, v);
            const double left = delta(center, input.sample(u - 1, v));
            const double top = delta(center, input.sample(u, v + 1));
            const double maximum = std::max({ left, top, delta(center, input.sample(u + 1, v)),
                                              delta(center, input.sample(u, v - 1)),
                                              delta(center, input.sample(u - 2, v)),
                                              delta(center, input.sample(u, v + 2)) });
            result.at(x, y) = { double(left >= .05 && left >= .5 * maximum),
                                double(top >= .05 && top >= .5 * maximum), 0, 0 };
        }
    }
    return result;
}
inline double searchLength(const QImage &search, Pixel edge, bool vertical, bool positive)
{
    const double u = ((positive != vertical) ? .5 : 0) + edge[vertical ? 1 : 0] * .5;
    const double v = edge[vertical ? 0 : 1];
    const auto color =
            search.pixelColor(std::clamp(int(u * search.width()), 0, search.width() - 1),
                              std::clamp(int(v * search.height()), 0, search.height() - 1));
    return color.red();
}
inline double searchEnd(const Image &input, const QImage &search, double x, double y, bool vertical,
                        bool positive)
{
    // Source WebGL port signs are intentional, including the Y loop end comparisons.
    const double direction = positive ? 1 : -1;
    const double start = vertical ? y : x;
    const double end = start + direction * (vertical ? -64 : 64);
    Pixel edge{ };
    for (int i = 0; i < 32; ++i) {
        edge = input.sample(x, y);
        (vertical ? y : x) += direction * 2;
        const double coordinate = vertical ? y : x;
        const bool within = positive ? coordinate < end : coordinate > end;
        const bool keepGoing = vertical ? (positive ? coordinate > end : coordinate < end) : within;
        if (!(keepGoing && edge[vertical ? 0 : 1] > .8281 && edge[vertical ? 1 : 0] == 0))
            break;
    }
    return (vertical ? y : x) - direction * 3.25
            + direction * searchLength(search, edge, vertical, positive);
}
inline Image lookup(const QImage &image)
{
    Image result(image.size());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto c = image.pixelColor(x, y);
            result.at(x, y) = { c.red() / 255.0, c.green() / 255.0, c.blue() / 255.0,
                                c.alpha() / 255.0 };
        }
    }
    return result;
}
inline Pixel area(const Image &table, double d0, double d1, double e0, double e1)
{
    return table.sample(16 * std::round(4 * e0) + std::sqrt(std::abs(d0)) + .5,
                        16 * std::round(4 * e1) + std::sqrt(std::abs(d1)) + .5);
}
inline Image weights(const Image &input, const QImage &areaImage, const QImage &search)
{
    const auto table = lookup(areaImage);
    Image result(input.size);
    for (int y = 0; y < input.size.height(); ++y) {
        for (int x = 0; x < input.size.width(); ++x) {
            const double u = x + .5, v = y + .5;
            const auto edge = input.sample(u, v);
            auto &w = result.at(x, y);
            if (edge[1] > 0) {
                const double left = searchEnd(input, search, u - .25, v + .125, false, false);
                const double right = searchEnd(input, search, u + 1.25, v + .125, false, true);
                const double e0 = input.sample(left, v + .25)[0];
                // Source float(ivec2(1,0)) is scalar 1: both axes advance.
                const double e1 = input.sample(right + 1, v + .25)[0];
                const auto a = area(table, left - u, right - u, e0, e1);
                w[0] = a[0];
                w[1] = a[1];
            }
            if (edge[0] > 0) {
                // Up uses search bias 0; down uses .5, opposite the X direction.
                const double up = searchEnd(input, search, u - .125, v + .25, true, true);
                const double down = searchEnd(input, search, u - .125, v - 1.25, true, false);
                const double e0 = input.sample(u - .25, up)[1];
                // Source float(ivec2(0,1)) is scalar 0: no axis advances.
                const double e1 = input.sample(u - .25, down - 1)[1];
                const auto a = area(table, up - v, down - v, e0, e1);
                w[2] = a[0];
                w[3] = a[1];
            }
        }
    }
    result.quantize();
    return result;
}
inline Image blend(const Image &input, const Image &weights)
{
    Image result(input.size);
    for (int y = 0; y < input.size.height(); ++y) {
        for (int x = 0; x < input.size.width(); ++x) {
            const double u = x + .5, v = y + .5;
            const auto own = weights.sample(u, v);
            const Pixel a{ own[0], weights.sample(u, v - 1)[1], own[2],
                           weights.sample(u + 1, v)[3] };
            auto color = input.sample(u, v);
            if (a[0] + a[1] + a[2] + a[3] >= 1e-5) {
                double dx = a[3] > a[2] ? a[3] : -a[2];
                double dy = a[1] > a[0] ? -a[1] : a[0];
                if (std::abs(dx) > std::abs(dy))
                    dy = 0;
                else
                    dx = 0;
                const auto opposite =
                        input.sample(u + (dx > 0) - (dx < 0), v + (dy > 0) - (dy < 0));
                const double amount = std::max(std::abs(dx), std::abs(dy));
                for (int c = 0; c < 4; ++c) {
                    if (c < 3)
                        color[c] = std::pow(std::pow(color[c], 2.2) * (1 - amount)
                                                    + std::pow(opposite[c], 2.2) * amount,
                                            1 / 2.2);
                    else
                        color[c] = color[c] * (1 - amount) + opposite[c] * amount;
                }
            }
            result.at(x, y) = color;
        }
    }
    result.quantize();
    return result;
}
}
