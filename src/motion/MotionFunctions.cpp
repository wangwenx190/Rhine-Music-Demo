#include "MotionFunctions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace yalahoo {
namespace {
double bell(double x, double width) { return std::exp(-.5 * std::pow(x / width, 2)); }
double packet(double distance) { return 2.5 * bell(distance, 3.8) - .58 * bell(distance - 6, 3.5); }
}
double smooth(double t) { t = std::clamp(t, 0.0, 1.0); return t*t*t*(10+t*(-15+6*t)); }
double archiveWave(double row, double lane, double time)
{
    const double t = time - 22, phase = row + (lane-2)*.65;
    return smooth(t/.32) * (packet(phase-(3+t*19))*(1-smooth((t-2.15)/.65))
        + packet(phase-(32-(t-2.3)*24))*smooth((t-2.17)/.32)*(1-smooth((t-3.5)/.85)));
}
double extraction(double time) { return .4*smooth((time-25.58)/.82)+2.95*smooth((time-27.55)/1.3); }
double settlingWave(double distance, double time)
{
    const double age = time-25.05-std::abs(distance)*.065;
    const double envelope = std::max(-.42, 2.15-.17*(std::sqrt(distance*distance+1)-1));
    const double ring = age > 0 ? std::sin(age*5.1)*std::exp(-age*1.3) : 0;
    return envelope*(smooth(age/.62)+.18*ring*smooth(age/.16));
}
double baselineSelectionWave(double distance, double age)
{
    if (age < 0 || age > 3.2) return 0;
    return .8*smooth(age/.2)*std::exp(-age*1.15)*std::cos((distance-age*8)*.58)*bell(distance-age*8,3.4);
}
double musicSelectionWave(double distance, double age)
{
    if (age < 0 || age > 3.2) return 0;
    return .32*smooth(age/.46)*std::exp(-age*1.35)*std::cos((distance-age*5.5)*.58)*bell(distance-age*5.5,3.4);
}
double selectionWave(double distance, double age) { return std::max(0.0,baselineSelectionWave(distance,age)); }
double rippleEnvelope(double distance, double age) { return smooth(distance/2.5)*std::max(0.0,std::cos((distance-age*8)*.58)); }
double columnStrength(double lane, double focus, double progress) { return 1+(.25+.75*bell(lane-focus,.55)-1)*smooth(progress); }
double idleWave(double row, double lane, double time)
{
    return .075*std::sin(time*std::numbers::pi*2/8+row*.3-lane*.45)
        +.027*std::sin(time*std::numbers::pi*2/13-row*.17+lane*.3);
}
double cinematicField(double row, double lane, double time, double center, double focus)
{
    const double handoff = smooth((time-24.95)/.45), selection = smooth((time-25.4)/.95);
    return archiveWave(row,lane,time)*(1-handoff)
        +settlingWave(row-center,time+.3*handoff*(1-selection))*columnStrength(lane,focus,(time-25.4)/.95);
}
double returnStep(double angle, double dt, bool reduced)
{
    const double next = angle*std::exp(-dt*(reduced ? 35 : 7));
    return std::abs(next) <= AlignmentEpsilon ? 0 : next;
}
}
