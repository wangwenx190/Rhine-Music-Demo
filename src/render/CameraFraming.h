// Owns shared source camera bases and responsive browse-aim offsets.
// Opening supplies its animated base aim/span; the settled camera supplies its final base values.

#pragma once
#include "SceneFrame.h"
#include "SceneGeometry.h"
#include "MotionFunctions.h"

namespace yalahoo::cameraFraming {
inline constexpr double StandardAspectWidth = 16.0;
inline constexpr double AspectHeightUnits = 9;
inline constexpr double UltrawideAspectWidth = 21.0;
inline constexpr double PortraitPreviewLift = .4;
inline constexpr double PortraitPreviewY = .36;
inline constexpr double WideAimRightRatio = .035;
inline constexpr double BrowseAimUpRatio = .08;
inline constexpr double UltrawideAimUpGain = .02;
inline constexpr double PortraitPreviewSpan = 8.4;
inline constexpr double ReferenceHeightPixels = 1080;
inline constexpr double LightOffsetX = 48;
inline constexpr float LightOffsetY = 17.6f;
inline constexpr double LightOffsetZ = 36;
inline constexpr double LocalLightAimX = 2.085;
inline constexpr double LocalLightAimY = 2.252;

struct Basis final
{
    SceneVector direction, right, up;
};
inline Basis basis(double yaw, double elevation)
{
    const SceneVector direction{ -std::sin(yaw) * std::cos(elevation), std::sin(elevation),
                                 std::cos(yaw) * std::cos(elevation) };
    const auto right = cross({ 0, 1, 0 }, direction).normalized();
    return { direction, right, cross(direction, right).normalized() };
}
inline SceneVector browseAim(SceneVector base, const Basis &basis, double span, double aspect,
                             bool portrait)
{
    if (portrait)
        return SceneVector{ 0,
                            -sceneGeometry::FloorDepth
                                    + settlingWave(0, sceneGeometry::SettledSceneSeconds)
                                    + PortraitPreviewLift + sceneGeometry::HingeHeight,
                            -sceneGeometry::ArchiveDepth }
        + basis.up * ((PortraitPreviewY - .5) * span);
    const double wide = smooth(
            (aspect - StandardAspectWidth / AspectHeightUnits)
            / (UltrawideAspectWidth / AspectHeightUnits - StandardAspectWidth / AspectHeightUnits));
    return base + basis.right * (WideAimRightRatio * wide * span * aspect)
            + basis.up * ((BrowseAimUpRatio + UltrawideAimUpGain * wide) * span);
}
}
