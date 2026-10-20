#include "MotionFunctions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace yalahoo {
namespace {
double bell(double x, double width)
{
    // The Gaussian exponent is -x^2/(2*sigma^2); width is sigma in scene rows.
    constexpr auto GaussianHalf = .5;

    return std::exp(-GaussianHalf * std::pow(x / width, 2));
}
double packet(double distance)
{
    // The authored packet combines a broad positive crest with a delayed negative lobe.
    constexpr auto LeadingAmplitude = 2.5;
    constexpr auto LeadingWidthRows = 3.8;
    constexpr auto TrailingAmplitude = .58;
    constexpr auto TrailingOffsetRows = 6;
    constexpr auto TrailingWidthRows = 3.5;

    return LeadingAmplitude * bell(distance, LeadingWidthRows)
            - TrailingAmplitude * bell(distance - TrailingOffsetRows, TrailingWidthRows);
}
}
// Quintic smoothstep 6t^5-15t^4+10t^3 has zero first and second endpoint derivatives.
double smooth(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (10 + t * (-15 + 6 * t));
}
double archiveWave(double row, double lane, double time)
{
    // Evaluate both authored travelling packets in scene time; lane phase staggers their crests.
    constexpr auto SceneStartSeconds = 22;
    constexpr auto LanePhaseRows = .65;
    constexpr auto LeadingFadeSeconds = .65;
    constexpr auto ArrivalFadeSeconds = .32;
    constexpr auto LeadingStartRow = 3;
    constexpr auto LeadingRowsPerSecond = 19;
    constexpr auto LeadingFadeStartSeconds = 2.15;
    constexpr auto ReturnStartRow = 32;
    constexpr auto ReturnStartSeconds = 2.3;
    constexpr auto ReturnRowsPerSecond = 24;
    constexpr auto ReturnFadeStartSeconds = 2.17;
    constexpr auto ReturnFadeOutSeconds = 3.5;
    constexpr auto ReturnFadeDurationSeconds = .85;

    const double t = time - SceneStartSeconds, phase = row + (lane - 2) * LanePhaseRows;
    return smooth(t / ArrivalFadeSeconds)
            * (packet(phase - (LeadingStartRow + t * LeadingRowsPerSecond))
                       * (1 - smooth((t - LeadingFadeStartSeconds) / LeadingFadeSeconds))
               + packet(phase - (ReturnStartRow - (t - ReturnStartSeconds) * ReturnRowsPerSecond))
                       * smooth((t - ReturnFadeStartSeconds) / ArrivalFadeSeconds)
                       * (1 - smooth((t - ReturnFadeOutSeconds) / ReturnFadeDurationSeconds)));
}
double extraction(double time)
{
    // Preview and inspection lifts occupy independent source timeline windows.
    constexpr auto PreviewLift = .4;
    constexpr auto PreviewStartSeconds = 25.58;
    constexpr auto PreviewDurationSeconds = .82;
    constexpr auto InspectionAdditionalLift = 2.95;
    constexpr auto InspectionStartSeconds = 27.55;
    constexpr auto InspectionDurationSeconds = 1.3;

    return PreviewLift * smooth((time - PreviewStartSeconds) / PreviewDurationSeconds)
            + InspectionAdditionalLift
            * smooth((time - InspectionStartSeconds) / InspectionDurationSeconds);
}
double settlingWave(double distance, double time)
{
    // Propagation delays depend on row distance; the damped ring rides the smooth static envelope.
    constexpr auto SettleStartSeconds = 25.05;
    constexpr auto DelaySecondsPerRow = .065;
    constexpr auto MinimumEnvelopeMagnitude = .42;
    constexpr auto CenterEnvelope = 2.15;
    constexpr auto EnvelopeFalloff = .17;
    constexpr auto RingRadiansPerSecond = 5.1;
    constexpr auto RingDecayPerSecond = 1.3;
    constexpr auto SettleDurationSeconds = .62;
    constexpr auto RingGain = .18;
    constexpr auto RingFadeSeconds = .16;

    const double age = time - SettleStartSeconds - std::abs(distance) * DelaySecondsPerRow;
    const double envelope =
            std::max(-MinimumEnvelopeMagnitude,
                     CenterEnvelope - EnvelopeFalloff * (std::sqrt(distance * distance + 1) - 1));
    const double ring = age > 0
            ? std::sin(age * RingRadiansPerSecond) * std::exp(-age * RingDecayPerSecond)
            : 0;
    return envelope
            * (smooth(age / SettleDurationSeconds)
               + RingGain * ring * smooth(age / RingFadeSeconds));
}
double baselineSelectionWave(double distance, double age)
{
    // A bounded travelling cosine packet decays in age and Gaussian row distance.
    constexpr auto LifetimeSeconds = 3.2;
    constexpr auto Amplitude = .8;
    constexpr auto FadeInSeconds = .2;
    constexpr auto DecayPerSecond = 1.15;
    constexpr auto RowsPerSecond = 8;
    constexpr auto RadiansPerRow = .58;
    constexpr auto WidthRows = 3.4;

    if (age < 0 || age > LifetimeSeconds)
        return 0;
    return Amplitude * smooth(age / FadeInSeconds) * std::exp(-age * DecayPerSecond)
            * std::cos((distance - age * RowsPerSecond) * RadiansPerRow)
            * bell(distance - age * RowsPerSecond, WidthRows);
}
double musicSelectionWave(double distance, double age)
{
    // The music route uses its authored lower amplitude and slower propagation.
    constexpr auto LifetimeSeconds = 3.2;
    constexpr auto Amplitude = .32;
    constexpr auto FadeInSeconds = .46;
    constexpr auto DecayPerSecond = 1.35;
    constexpr auto RowsPerSecond = 5.5;
    constexpr auto RadiansPerRow = .58;
    constexpr auto WidthRows = 3.4;

    if (age < 0 || age > LifetimeSeconds)
        return 0;
    return Amplitude * smooth(age / FadeInSeconds) * std::exp(-age * DecayPerSecond)
            * std::cos((distance - age * RowsPerSecond) * RadiansPerRow)
            * bell(distance - age * RowsPerSecond, WidthRows);
}
double selectionWave(double distance, double age)
{
    return std::max(0.0, baselineSelectionWave(distance, age));
}
double rippleEnvelope(double distance, double age)
{
    // Suppress the near field before applying the positive travelling cosine envelope.
    constexpr auto NearFieldRows = 2.5;
    constexpr auto RowsPerSecond = 8;
    constexpr auto RadiansPerRow = .58;

    return smooth(distance / NearFieldRows)
            * std::max(0.0, std::cos((distance - age * RowsPerSecond) * RadiansPerRow));
}
double columnStrength(double lane, double focus, double progress)
{
    // Blend from uniform lighting toward the Gaussian focus column without a discontinuity.
    constexpr auto OuterColumnGain = .25;
    constexpr auto FocusedAdditionalGain = .75;
    constexpr auto FocusWidthLanes = .55;

    return 1
            + (OuterColumnGain + FocusedAdditionalGain * bell(lane - focus, FocusWidthLanes) - 1)
            * smooth(progress);
}
double idleWave(double row, double lane, double time)
{
    // Two independent low-amplitude sinusoids avoid a rigidly repeating archive pose.
    constexpr auto PrimaryAmplitude = .075;
    constexpr auto PrimaryPeriodSeconds = 8;
    constexpr auto PrimaryRowPhase = .3;
    constexpr auto SecondaryLanePhase = .3;
    constexpr auto PrimaryLanePhase = .45;
    constexpr auto SecondaryAmplitude = .027;
    constexpr auto SecondaryPeriodSeconds = 13;
    constexpr auto SecondaryRowPhase = .17;

    return PrimaryAmplitude
            * std::sin(time * std::numbers::pi * 2 / PrimaryPeriodSeconds + row * PrimaryRowPhase
                       - lane * PrimaryLanePhase)
            + SecondaryAmplitude
            * std::sin(time * std::numbers::pi * 2 / SecondaryPeriodSeconds
                       - row * SecondaryRowPhase + lane * SecondaryLanePhase);
}
double cinematicField(double row, double lane, double time, double center, double focus)
{
    // Crossfade archive motion into settling while gradually revealing column focus.
    constexpr auto HandoffStartSeconds = 24.95;
    constexpr auto HandoffDurationSeconds = .45;
    constexpr auto SelectionStartSeconds = 25.4;
    constexpr auto SelectionDurationSeconds = .95;
    constexpr auto SettleTimeAdvanceSeconds = .3;

    const double handoff = smooth((time - HandoffStartSeconds) / HandoffDurationSeconds),
                 selection = smooth((time - SelectionStartSeconds) / SelectionDurationSeconds);
    return archiveWave(row, lane, time) * (1 - handoff)
            + settlingWave(row - center,
                           time + SettleTimeAdvanceSeconds * handoff * (1 - selection))
            * columnStrength(lane, focus,
                             (time - SelectionStartSeconds) / SelectionDurationSeconds);
}
double returnStep(double angle, double dt, bool reduced)
{
    // Exponential return is frame-rate independent; snap only inside the alignment tolerance.
    constexpr auto ReducedReturnRate = 35;
    constexpr auto ReturnRate = 7;

    const double next = angle * std::exp(-dt * (reduced ? ReducedReturnRate : ReturnRate));
    return std::abs(next) <= AlignmentEpsilon ? 0 : next;
}
}
