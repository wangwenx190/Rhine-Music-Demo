// Owns source shell endpoints and evaluates them at the caller's numeric precision.
// QML Review and the RHI uniform path consume the same continuous material equations.

#pragma once
#include <QStringView>
#include <algorithm>
#include <concepts>

namespace yalahoo::opticalSource {
using namespace Qt::StringLiterals;
enum Surface {
    Lid,
    Spine,
    Tray,
    SurfaceCount
};
struct Endpoints final
{
    double transmission, transmissionLoss, roughness, roughnessGain;
    double thickness, thicknessGain, distance, distanceLoss;
};
inline constexpr Endpoints Surfaces[] = { { .96, .02, 0, 0, .018, .004, 5, 1.2 },
                                          { .88, .04, .25, .05, .06, .02, 4.5, 1.3 },
                                          { .8, .1, .38, .08, .038, .012, 4.5, 2.5 } };
inline constexpr QLatin1StringView DayTint[] = { "#fffdf8"_L1, "#e7e1d8"_L1, "#ece6dc"_L1 };
inline constexpr QLatin1StringView NightTint[] = { "#f6fbff"_L1, "#f6fbff"_L1, "#c6d6e5"_L1 };
inline constexpr auto DayAttenuation = "#f3e9db"_L1;
inline constexpr auto NightAttenuation = "#dceafd"_L1;
inline constexpr auto FrostedRoughness = .34;
inline constexpr auto FrostedWarmthGain = .06;
inline constexpr auto ClearRoughness = .27;
inline constexpr auto ClearWarmthGain = .07;
inline constexpr auto SpineCoat = .16;
inline constexpr auto OtherCoat = .08;
inline constexpr auto SpineCoatRoughness = .2;
inline constexpr auto OtherCoatRoughness = .38;
inline constexpr auto FallbackEnvironment = .68;
inline constexpr auto FallbackEnvironmentWarmthLoss = .20;
inline constexpr auto FallbackExposure = 1.08;
inline constexpr auto FallbackExposureWarmthLoss = .03;

template <std::floating_point T>
constexpr T lidRoughness(T warmth, T clarity)
{
    return (T(FrostedRoughness) + T(FrostedWarmthGain) * warmth) * (1 - clarity)
            + (T(ClearRoughness) + T(ClearWarmthGain) * warmth) * clarity;
}
template <std::floating_point T>
struct Parameters final
{
    T transmission, roughness, thickness, attenuationDistance, clearcoat, coatRoughness;
};
template <std::floating_point T>
constexpr Parameters<T> parameters(int surface, T warmth, T clarity)
{
    const auto &source = Surfaces[surface];
    return { T(source.transmission) - T(source.transmissionLoss) * warmth,
             surface == Lid ? lidRoughness(warmth, clarity)
                            : T(source.roughness) + T(source.roughnessGain) * warmth,
             T(source.thickness) + T(source.thicknessGain) * warmth,
             T(source.distance) - T(source.distanceLoss) * warmth,
             T(surface == Spine ? SpineCoat : OtherCoat),
             T(surface == Spine ? SpineCoatRoughness : OtherCoatRoughness) };
}
}
