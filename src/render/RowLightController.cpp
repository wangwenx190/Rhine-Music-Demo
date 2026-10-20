#include "RowLightController.h"
#include "MotionFunctions.h"
#include <algorithm>
#include <numbers>
namespace {
// The damping factor maps the authored duration to the critical spring response.
constexpr auto CriticalDampingDurationFactor = 4.743864518390578;
constexpr auto IntroReturnStartSeconds = 24.15;
constexpr auto IntroStartRows = 9;
constexpr auto IntroTravelRows = 27;
constexpr auto RibbonLaneWidth = 27;
constexpr auto IntroStartSeconds = 22.2;
constexpr auto IntroTravelSeconds = 1.95;
constexpr auto IntroReturnRows = 18;
constexpr auto IntroReturnSeconds = 1.6;
constexpr auto ArchiveRowSpacing = .62;
constexpr auto ArchiveColumnSpacing = 5.2;
constexpr auto LaneEnvelopePower = 6;
}

namespace yalahoo {
// Project velocity onto the remaining travel direction and clamp the analytic spring result.
// This prevents a retarget from overshooting or briefly moving away from the selected row.
double RowLightController::follow(ScalarSpring &s, double target, double dt, double duration)
{
    const double previous = s.value, distance = target - previous;
    if (distance == 0) {
        s.velocity = 0;
        return previous;
    }
    const double rate = CriticalDampingDurationFactor / duration, direction = distance > 0 ? 1 : -1;
    const double velocity =
            std::min(std::max(0.0, s.velocity * direction), rate * std::abs(distance)) * direction;
    const double elapsed = std::max(0.0, dt), impulse = velocity - rate * distance,
                 decay = std::exp(-rate * elapsed);
    const double next = target + (-distance + impulse * elapsed) * decay;
    s.value = std::clamp(next, std::min(previous, target), std::max(previous, target));
    s.velocity = s.value == target || s.value != next
            ? 0
            : (velocity - rate * impulse * elapsed) * decay;
    return s.value;
}
double RowLightController::introOffset(double t)
{
    return t < IntroReturnStartSeconds
            ? -IntroStartRows
                    + IntroTravelRows * smooth((t - IntroStartSeconds) / IntroTravelSeconds)
            : IntroReturnRows * (1 - smooth((t - IntroReturnStartSeconds) / IntroReturnSeconds));
}
void RowLightController::reset()
{
    initialized = false;
    opening = false;
    basis.reset();
    basisLane = 0;
    independent = false;
    row.velocity = lane.velocity = 0;
}
void RowLightController::update(const RowLightingInput &i, double dt)
{
    if (!i.visible) {
        reset();
        return;
    }
    const double nextBasis = i.origin.z - i.row * ArchiveRowSpacing;
    const bool intro = !i.reduced && i.introTime.has_value();
    if (!initialized || i.reduced || i.cinematic || opening) {
        row.finish(i.row);
        lane.finish(i.lane);
    } else {
        if (basis
            && (i.independentColumns != independent
                || (i.independentColumns && i.lane != basisLane)))
            row.value += (*basis - nextBasis) / ArchiveRowSpacing;
        follow(row, i.row, dt, settings.transitionDuration);
        follow(lane, i.lane, dt, settings.transitionDuration);
    }
    if (intro)
        row.value = i.row + introOffset(*i.introTime);
    opening = intro;
    initialized = true;
    basis = nextBasis;
    basisLane = i.lane;
    independent = i.independentColumns;
    column = i.origin;
    column.z += (row.value - i.row) * ArchiveRowSpacing;
    column.x += (lane.value - i.lane) * ArchiveColumnSpacing;
}
double RowLightController::sample(SceneVector origin) const
{
    const double r = (origin.z - column.z - settings.offset * ArchiveRowSpacing)
            / (.5 * settings.width * ArchiveRowSpacing),
                 l = (origin.x - column.x) / RibbonLaneWidth;
    return std::exp(-std::numbers::ln2 * r * r) * std::exp(-std::pow(l, LaneEnvelopePower));
}
}
