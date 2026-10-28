// Implements the IEC sRGB channel transfer with the caller's floating-point precision.
// Alpha, clipping, premultiplication and tone mapping remain explicit at the call site.

#pragma once
#include <cmath>
#include <concepts>

namespace yalahoo::colorTransfer {
inline constexpr double DecodeKnee = .04045;
inline constexpr double EncodeKnee = .0031308;
inline constexpr double LinearSlope = 12.92;
inline constexpr double Offset = .055;
inline constexpr double Scale = 1.055;
inline constexpr double Exponent = 2.4;

template <std::floating_point T>
T decode(T value)
{
    return value <= T(DecodeKnee) ? value / T(LinearSlope)
                                  : std::pow((value + T(Offset)) / T(Scale), T(Exponent));
}

template <std::floating_point T>
T encode(T value)
{
    return value <= T(EncodeKnee) ? T(LinearSlope) * value
                                  : T(Scale) * std::pow(value, T(1) / T(Exponent)) - T(Offset);
}
}
