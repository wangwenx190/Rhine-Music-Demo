// Bounds artistic effect inputs before persistence and production float-uniform conversion.
#pragma once
#include <cmath>
#include <limits>

namespace yalahoo {
namespace renderQuality {
// The default output follows the source quality contract without a DPR-dependent sample reduction.
inline constexpr auto AoSamples = 64;
inline constexpr auto DefaultDepthOfFieldPercent = 100;
inline constexpr auto BrowseAperture = .0003;
inline constexpr auto DetailApertureGain = .0005;
inline constexpr auto PercentScale = 100;
// At full detail the aperture must remain representable by the production float uniform.
inline constexpr double MaximumDepthOfFieldPercent = double(std::numeric_limits<float>::max())
        / (BrowseAperture + DetailApertureGain) * PercentScale;
inline bool validDepthOfField(double amount)
{
    return std::isfinite(amount) && amount >= 0 && amount <= MaximumDepthOfFieldPercent;
}
}

}
