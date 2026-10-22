// Defines the source-derived scalar motion fields in seconds and scene units. Pure functions
// keep wave envelopes and handoff timing independent of rendering.

#pragma once
namespace yalahoo {
namespace motionField {
// The default cinematic focus uses the source archive coordinate origin.
inline constexpr auto DefaultCenterRow = 12;
inline constexpr auto DefaultFocusLane = 2;
}

double smooth(double t);
double archiveWave(double row, double lane, double time);
double extraction(double time);
double settlingWave(double distance, double time);
// A pulse's time envelope is shared by all spatial samples in one scene state.
struct SelectionWave final
{
    double gain = 0, travel = 0;
    double sample(double distance) const;
};
SelectionWave prepareMusicSelectionWave(double age);
double baselineSelectionWave(double distance, double age);
double musicSelectionWave(double distance, double age);
double selectionWave(double distance, double age);
double rippleEnvelope(double distance, double age);
double columnStrength(double lane, double focus, double progress = 1);
double idleWave(double row, double lane, double time);
double cinematicField(double row, double lane, double time,
                      double center = motionField::DefaultCenterRow,
                      double focus = motionField::DefaultFocusLane);
inline constexpr double InspectionLift = 4.05;
inline constexpr double AlignmentEpsilon = .001;
double returnStep(double angle, double dt, bool reduced = false);
}
