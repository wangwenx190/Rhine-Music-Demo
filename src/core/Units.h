// Defines binary byte and clock conversion units shared by storage, rendering and diagnostics.
// Integer units retain exact byte arithmetic; floating time ratios avoid integral truncation.

#pragma once
#include <numbers>

namespace yalahoo::units {
inline constexpr int BytesPerKiB = 1024;
inline constexpr int MillisecondsPerSecond = 1000;
inline constexpr double NanosecondsPerMillisecond = 1e6;
inline constexpr int MaximumChannelByte = 255;
inline constexpr int RgbaBytesPerPixel = 4;
inline constexpr int DecimalBase = 10;
inline constexpr auto DegreesPerHalfTurn = 180;
inline constexpr auto RadiansPerDegree = std::numbers::pi / DegreesPerHalfTurn;
inline constexpr auto VectorAxes = 3;
}
