// Evaluates the frozen 49-point critically damped reel curve for titles and decimal wheels.
// Callers retain their own retarget, endpoint and visibility policies. Sampling allocates nothing.

#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace yalahoo::sampledReel {
inline constexpr int Intervals = 48;
inline constexpr int SampleCount = Intervals + 1;
inline constexpr double TransitionSeconds = .460;
inline constexpr double Decay = 10;
inline constexpr double VelocityLimitGain = 12;
inline constexpr double AlignmentTolerance = .001;
inline constexpr double GlyphSidePaddingEm = .15;
using Points = std::array<double, SampleCount>;
struct Sample final
{
    double value = 0, velocity = 0;
};
inline double limitedVelocity(double delta, double velocity, double duration)
{
    const double limit = std::max(std::abs(delta), 1.0) * VelocityLimitGain / duration;
    return std::clamp(velocity, -limit, limit);
}
inline Sample analytic(double start, double target, double velocity, double duration, double t)
{
    const double delta = start - target;
    const double advance = velocity * duration + Decay * delta;
    const double decay = std::exp(-Decay * t);
    return { target + (delta + advance * t) * decay,
             (advance - Decay * (delta + advance * t)) * decay / duration };
}
inline void fill(Points &points, double start, double target, double velocity, double duration)
{
    for (int i = 0; i < SampleCount; ++i)
        points[i] = i == Intervals
                ? target
                : analytic(start, target, velocity, duration, double(i) / Intervals).value;
}
inline Sample interpolate(const Points &points, double position, double duration)
{
    const int index = std::min(Intervals - 1, int(position));
    const double delta = points[index + 1] - points[index];
    return { points[index] + delta * (position - index), delta * Intervals / duration };
}
}
