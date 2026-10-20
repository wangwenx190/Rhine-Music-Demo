#include "SceneGeometry.h"
#include "OpeningFrame.h"
#include "MotionFunctions.h"
#include <algorithm>
#include <numbers>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
double unit(double t, double from, double to)
{
    return std::clamp((t - from) / (to - from), 0.0, 1.0);
}
double openingSmooth(double t)
{
    return t * t * (3 - 2 * t);
}
double settle(double t)
{
    return 1 - std::pow(1 - t, 3);
}
}
OpeningFrame openingFrame(double time)
{
    // Evaluate overlapping authored time windows directly; no timer completion can drift between layers.
    constexpr auto TitleEnterStartSeconds = 2.72;
    constexpr auto TitleEnterEndSeconds = 3.58;
    constexpr auto TitleExitStartSeconds = 5.7;
    constexpr auto TitleExitEndSeconds = 6.32;
    constexpr auto LogoExitStartSeconds = 1.8;
    constexpr auto SloganPhaseStartSeconds = 2.9;
    constexpr auto ArrayPhaseStartSeconds = 6.4;
    constexpr auto BackgroundExitEndSeconds = 6.82;
    constexpr auto MarkEnterStartSeconds = .2;
    constexpr auto MarkEnterEndSeconds = .48;
    constexpr auto MarkExitEndSeconds = 2.26;
    constexpr auto ContourStartSeconds = .24;
    constexpr auto OrbitStartSeconds = 1.64;
    constexpr auto SymbolsStartSeconds = .84;
    constexpr auto SymbolsEndSeconds = 1.5;
    constexpr auto OrbitEnterEndSeconds = 2.05;
    constexpr auto OrbitExitStartSeconds = 2.78;
    constexpr auto OrbitExitEndSeconds = 3.28;
    constexpr auto OrbitDrawEndSeconds = 2.82;
    constexpr auto OrbitTurnDegrees = 190;
    constexpr auto OrbitTurnEndSeconds = 3.3;
    constexpr auto OrbitScaleGain = .1;
    constexpr auto InitialOrbitScale = .9;
    constexpr auto OrbitScaleEndSeconds = 2.8;
    constexpr auto TitleEnterOffsetPixels = 18;
    constexpr auto TitleExitOffsetPixels = 8;
    constexpr auto TitleClipPercent = 100;
    constexpr auto SubtitleEnterStartSeconds = 3.18;
    constexpr auto SubtitleEnterEndSeconds = 3.8;
    constexpr auto SubtitleExitStartSeconds = 5.56;
    constexpr auto SubtitleExitEndSeconds = 6.08;

    const double t = std::isfinite(time) ? std::max(0.0, time) : 0;
    const double in = settle(unit(t, TitleEnterStartSeconds, TitleEnterEndSeconds)),
                 out = openingSmooth(unit(t, TitleExitStartSeconds, TitleExitEndSeconds));
    OpeningFrame f;
    f.phase = t < LogoExitStartSeconds    ? "logo"_L1
            : t < SloganPhaseStartSeconds ? "orbit"_L1
            : t < ArrayPhaseStartSeconds  ? "slogan"_L1
                                          : "array"_L1;
    f.background = 1 - openingSmooth(unit(t, TitleExitEndSeconds, BackgroundExitEndSeconds));
    f.mark = openingSmooth(unit(t, MarkEnterStartSeconds, MarkEnterEndSeconds))
            * (1 - openingSmooth(unit(t, LogoExitStartSeconds, MarkExitEndSeconds)));
    f.contour = settle(unit(t, ContourStartSeconds, OrbitStartSeconds));
    f.symbols = settle(unit(t, SymbolsStartSeconds, SymbolsEndSeconds));
    f.orbit = openingSmooth(unit(t, OrbitStartSeconds, OrbitEnterEndSeconds))
            * (1 - openingSmooth(unit(t, OrbitExitStartSeconds, OrbitExitEndSeconds)));
    f.orbitDraw = settle(unit(t, OrbitStartSeconds, OrbitDrawEndSeconds));
    f.orbitTurn = OrbitTurnDegrees * openingSmooth(unit(t, OrbitStartSeconds, OrbitTurnEndSeconds));
    f.orbitScale = InitialOrbitScale
            + OrbitScaleGain * settle(unit(t, OrbitStartSeconds, OrbitScaleEndSeconds));
    f.title = in * (1 - out);
    f.titleY = TitleEnterOffsetPixels * (1 - in) - TitleExitOffsetPixels * out;
    f.titleClip = TitleClipPercent * (1 - in);
    f.subtitle = openingSmooth(unit(t, SubtitleEnterStartSeconds, SubtitleEnterEndSeconds))
            * (1 - openingSmooth(unit(t, SubtitleExitStartSeconds, SubtitleExitEndSeconds)));
    return f;
}
QVariantMap OpeningFrame::values() const
{
    return { { u"background"_s, background },
             { u"mark"_s, mark },
             { u"contour"_s, contour },
             { u"symbols"_s, symbols },
             { u"orbit"_s, orbit },
             { u"orbitDraw"_s, orbitDraw },
             { u"orbitTurn"_s, orbitTurn },
             { u"orbitScale"_s, orbitScale },
             { u"title"_s, title },
             { u"titleY"_s, titleY },
             { u"titleClip"_s, titleClip },
             { u"subtitle"_s, subtitle },
             { u"phase"_s, phase } };
}
double openingSceneTime(double time)
{
    // Map the independent two-dimensional opening clock into the source three-dimensional timeline.
    constexpr auto SceneTimelineOriginSeconds = 21.92;
    constexpr auto ArrayPhaseStartSeconds = 6.4;

    return SceneTimelineOriginSeconds + std::max(0.0, time - ArrayPhaseStartSeconds);
}
CameraPose openingCamera(double sceneTime, double width, double height, SceneVector selected)
{
    // Blend source orbit, settling and preview poses in world coordinates before applying aspect framing.
    constexpr auto TerminalSceneSeconds = 27.12;
    constexpr auto OrbitStartSeconds = 22.6;
    constexpr auto OrbitDurationSeconds = 1.6;
    constexpr auto SettleStartSeconds = 24.25;
    constexpr auto SettleDurationSeconds = 2.25;
    constexpr auto PreviewStartSeconds = 25.3;
    constexpr auto PreviewDurationSeconds = 1.42;
    constexpr auto DegreesPerHalfTurn = 180;
    constexpr auto InitialYawDegrees = 89;
    constexpr auto OrbitYawDegrees = 22;
    constexpr auto SettleYawDegrees = 8;
    constexpr auto OrbitElevationDegrees = 8;
    constexpr auto InitialElevationDegrees = 3;
    constexpr auto InitialElevationGainDegrees = 40;
    constexpr auto ElevationStartSeconds = 21.96;
    constexpr auto ElevationDurationSeconds = .22;
    constexpr auto SettleElevationDegrees = 16;
    constexpr auto PreviewElevationDegrees = 6;
    constexpr auto InitialSpan = 10.8;
    constexpr auto OrbitSpan = 10.3;
    constexpr auto SettledSpan = 7.33;
    constexpr auto PortraitAspectBoundary = 1.05;
    constexpr auto PanDurationSeconds = 1.05;
    constexpr auto PortraitSpan = 8.4;
    constexpr auto StandardAspectWidth = 16.0;
    constexpr auto AspectHeightUnits = 9;
    constexpr auto InitialCameraDistance = 28;
    constexpr auto OrbitDistanceGain = 7;
    constexpr auto SettledCameraDistance = 140;
    constexpr auto ArrayAimXMagnitude = 1.091;
    constexpr auto InitialArrayAimDepth = 2.55;
    constexpr auto OrbitAimHeightGain = .4;
    constexpr auto PortraitPreviewLift = .4;
    constexpr auto SettledArrayAimDepth = .045;
    constexpr auto InitialArrayAimZ = 2.48;
    constexpr auto SettledArrayAimZ = .481;
    constexpr auto LeadAimOffset = 2.05;
    constexpr auto PanStartSeconds = 25.4;
    constexpr auto LeadAimFadeSeconds = .95;
    constexpr auto LeadAimStartSeconds = 24.2;
    constexpr auto LeadAimEnterSeconds = .8;
    constexpr auto AnchorBlendStartSeconds = 25.05;
    constexpr auto AnchorBlendEndSeconds = 27.3;
    constexpr auto ReferenceHeightPixels = 1080;
    constexpr auto CaseWidth = 4.45;
    constexpr auto InitialAnchorXPixels = 840;
    constexpr auto FinalAnchorXPixels = 518;
    constexpr auto ReferenceCenterXPixels = 960;
    constexpr auto ReferenceCenterYPixels = 540;
    constexpr auto InitialAnchorYPixels = 340;
    constexpr auto FinalAnchorYPixels = 288;
    constexpr auto AnchorBlendSeconds = .35;
    constexpr auto PortraitPreviewY = .36;
    constexpr auto UltrawideAspectWidth = 21.0;
    constexpr auto WideAimRightRatio = .035;
    constexpr auto BrowseAimUpRatio = .08;
    constexpr auto UltrawideAimUpGain = .02;

    const double shot = std::min(sceneTime, TerminalSceneSeconds), aspect = width / height;
    const double orbit = smooth((shot - OrbitStartSeconds) / OrbitDurationSeconds),
                 settle = smooth((shot - SettleStartSeconds) / SettleDurationSeconds);
    const double intro = smooth((shot - PreviewStartSeconds) / PreviewDurationSeconds);
    const double radians = std::numbers::pi / DegreesPerHalfTurn;
    const double yaw =
            (InitialYawDegrees - OrbitYawDegrees * orbit - SettleYawDegrees * settle) * radians;
    const double elevation =
            (InitialElevationDegrees
             + InitialElevationGainDegrees
                     * smooth((shot - ElevationStartSeconds) / ElevationDurationSeconds)
             - OrbitElevationDegrees * orbit - SettleElevationDegrees * settle
             + PreviewElevationDegrees * intro)
            * radians;
    const SceneVector direction{ -std::sin(yaw) * std::cos(elevation), std::sin(elevation),
                                 std::cos(yaw) * std::cos(elevation) };
    const auto right = cross({ 0, 1, 0 }, direction).normalized(),
               up = cross(direction, right).normalized();
    const double sourceSpan =
            (InitialSpan + (OrbitSpan - InitialSpan) * orbit) * (1 - settle) + SettledSpan * settle;
    const double span = aspect < PortraitAspectBoundary
            ? std::max(sourceSpan, PortraitSpan / aspect)
            : std::max(sourceSpan, sourceSpan * (StandardAspectWidth / AspectHeightUnits) / aspect);
    const double distance = (InitialCameraDistance + OrbitDistanceGain * orbit) * (1 - settle)
            + SettledCameraDistance * settle;
    SceneVector array{ -ArrayAimXMagnitude,
                       (-InitialArrayAimDepth + OrbitAimHeightGain * orbit) * (1 - settle)
                               - SettledArrayAimDepth * settle,
                       InitialArrayAimZ + (SettledArrayAimZ - InitialArrayAimZ) * settle };
    auto aim = array
            + right
                    * (-LeadAimOffset * (1 - smooth((shot - PanStartSeconds) / LeadAimFadeSeconds))
                       * smooth((shot - LeadAimStartSeconds) / LeadAimEnterSeconds));
    if (shot >= AnchorBlendStartSeconds && shot <= AnchorBlendEndSeconds) {
        const double pan = smooth((shot - PanStartSeconds) / PanDurationSeconds),
                     scale = ReferenceHeightPixels / sourceSpan;
        auto anchor = selected
                + SceneVector{ -CaseWidth / 2,
                               sceneGeometry::HingeHeight + sceneGeometry::CaseHeight / 2, 0 };
        anchor = anchor
                + right
                        * (-(InitialAnchorXPixels
                             + (FinalAnchorXPixels - InitialAnchorXPixels) * pan
                             - ReferenceCenterXPixels)
                           / scale)
                + up
                        * (-(ReferenceCenterYPixels
                             - (InitialAnchorYPixels
                                + (FinalAnchorYPixels - InitialAnchorYPixels) * pan))
                           / scale);
        const double blend = smooth((shot - AnchorBlendStartSeconds) / AnchorBlendSeconds);
        aim = aim * (1 - blend) + anchor * blend;
    }
    auto preview = array;
    if (aspect < PortraitAspectBoundary)
        preview = SceneVector{ 0,
                               -sceneGeometry::FloorDepth
                                       + settlingWave(0, sceneGeometry::SettledSceneSeconds)
                                       + PortraitPreviewLift + sceneGeometry::HingeHeight,
                               -sceneGeometry::ArchiveDepth }
                + up * ((PortraitPreviewY - .5) * span);
    else {
        const double wide = smooth((aspect - StandardAspectWidth / AspectHeightUnits)
                                   / (UltrawideAspectWidth / AspectHeightUnits
                                      - StandardAspectWidth / AspectHeightUnits));
        preview = preview + right * (WideAimRightRatio * wide * span * aspect)
                + up * ((BrowseAimUpRatio + UltrawideAimUpGain * wide) * span);
    }
    aim = aim * (1 - intro) + preview * intro;
    return { aim + direction * distance, aim, span };
}
double openingReveal(double elapsed, double delay)
{
    // Invert the authored Bezier x coordinate by bounded bisection, then evaluate cubic y.
    constexpr auto RevealEndSeconds = .72;
    constexpr auto BezierSearchSteps = 40;
    constexpr auto FirstBezierX = .22;
    constexpr auto SecondBezierX = .36;

    const double x = unit(elapsed, delay, RevealEndSeconds);
    // Invert CSS cubic-bezier(.22,1,.36,1), retaining the authored stagger endpoint.
    double lo = 0, hi = 1;
    for (int i = 0; i < BezierSearchSteps; ++i) {
        const double t = (lo + hi) * .5, u = 1 - t;
        const double bx = 3 * u * u * t * FirstBezierX + 3 * u * t * t * SecondBezierX + t * t * t;
        if (bx < x)
            lo = t;
        else
            hi = t;
    }
    return x == 0 ? 0 : x == 1 ? 1 : 1 - std::pow(1 - (lo + hi) * .5, 3);
}
}
