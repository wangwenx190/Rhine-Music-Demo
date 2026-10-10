#pragma once
namespace yalahoo {
double smooth(double t);
double archiveWave(double row, double lane, double time);
double extraction(double time);
double settlingWave(double distance, double time);
double baselineSelectionWave(double distance, double age);
double musicSelectionWave(double distance, double age);
double selectionWave(double distance, double age);
double rippleEnvelope(double distance, double age);
double columnStrength(double lane, double focus, double progress = 1);
double idleWave(double row, double lane, double time);
double cinematicField(double row, double lane, double time, double center = 12, double focus = 2);
inline constexpr double InspectionLift = 4.05;
inline constexpr double AlignmentEpsilon = .001;
double returnStep(double angle, double dt, bool reduced = false);
}
