#include "CaseClarity.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace yalahoo {
namespace {
// Returning cases decay independently; only negligible clarity is snapped to zero.
constexpr auto ReturnDecayRate = 9;
constexpr auto InvisibleClarityThreshold = .0001;

constexpr double start = 34.12, end = 39.56, rate = 1.5;
constexpr std::array<std::array<double, 2>, 6> knots{
    { { 38.84, 0 }, { 38.92, .28 }, { 39, .51 }, { 39.16, .74 }, { 39.32, .94 }, { 39.56, 1 } }
};
// Use a weighted harmonic mean of neighboring secants (monotone cubic interpolation).
// Flat or sign-changing secants get zero tangent so the reveal cannot overshoot its knots.
constexpr double slope(size_t i)
{
    if (i == 0 || i == knots.size() - 1)
        return 0;
    const double left = knots[i][0] - knots[i - 1][0];
    const double right = knots[i + 1][0] - knots[i][0];
    const double a = (knots[i][1] - knots[i - 1][1]) / left;
    const double b = (knots[i + 1][1] - knots[i][1]) / right;
    if (a * b <= 0)
        return 0;
    const double w1 = 2 * right + left, w2 = right + 2 * left;
    return (w1 + w2) / (w1 / a + w2 / b);
}
constexpr auto derivatives = [] {
    std::array<double, knots.size()> result{ };
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = slope(i);
    return result;
}();
}
double CaseClarity::reveal(double time)
{
    if (time <= knots.front()[0])
        return knots.front()[1];
    if (time >= knots.back()[0])
        return knots.back()[1];
    size_t i = 0;
    while (time > knots[i + 1][0])
        ++i;
    const double span = knots[i + 1][0] - knots[i][0];
    const double t = (time - knots[i][0]) / span, t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * knots[i][1] + (t3 - 2 * t2 + t) * span * derivatives[i]
            + (-2 * t3 + 3 * t2) * knots[i + 1][1] + (t3 - t2) * span * derivatives[i + 1];
}
void CaseClarity::enter(bool alreadyClear)
{
    if (m_active)
        return;
    m_active = true;
    m_elapsed = alreadyClear ? std::optional<double>((end - start) / rate) : std::nullopt;
    if (alreadyClear)
        m_value = 1;
}
void CaseClarity::leave()
{
    m_active = false;
    m_elapsed.reset();
}
void CaseClarity::select(double clarity)
{
    leave();
    m_value = std::clamp(clarity, 0.0, 1.0);
}
void CaseClarity::advance(double dt, bool ready, bool reduced)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    if (!m_active) {
        m_value = reduced ? 0 : m_value * std::exp(-dt * ReturnDecayRate);
        if (m_value < InvisibleClarityThreshold)
            m_value = 0;
        return;
    }
    if (reduced) {
        m_elapsed = (end - start) / rate;
        m_value = 1;
        return;
    }
    if (!m_elapsed && ready)
        m_elapsed = 0;
    else if (m_elapsed)
        m_elapsed = std::min(*m_elapsed + dt, (end - start) / rate);
    if (m_elapsed) {
        const double time = start + *m_elapsed * rate;
        const double value = reveal(time);
        m_value = value > m_value ? value
                : time >= end     ? 1
                                  : m_value * std::exp(-dt * ReturnDecayRate);
    }
}
}
