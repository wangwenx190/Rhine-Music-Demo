#include "DetailMotion.h"
#include <algorithm>
#include <cmath>

namespace {
// Readiness requires camera and lift to remain settled across the authored hold interval.
constexpr auto PlacementSourceSeconds = 1.45;
constexpr auto PlacementSpeed = 1.05;
constexpr auto ReadinessHoldSeconds = .08;
}

namespace yalahoo {
DetailMotion::DetailMotion()
{
    reset();
}
void DetailMotion::reset()
{
    m_phase = Phase::Archive;
    m_settledFor = 0;
    m_placement.reset();
    m_placement.retarget(0, PlacementSourceSeconds / PlacementSpeed);
    m_placement.finish();
}
bool DetailMotion::placed() const
{
    return m_phase == Phase::Placing || m_phase == Phase::Presented;
}
bool DetailMotion::holdsDetail() const
{
    return placed() || m_phase == Phase::ReturningCenter;
}
double DetailMotion::value() const
{
    return std::clamp(m_placement.state().value, 0.0, 1.0);
}
void DetailMotion::request(bool detail)
{
    const auto previous = m_phase;
    if (detail) {
        if (!placed())
            m_phase = Phase::Placing;
    } else if (m_phase != Phase::Archive && m_phase != Phase::ReturningArray) {
        m_phase = Phase::ReturningCenter;
    }
    if (previous != m_phase)
        m_settledFor = 0;
}
void DetailMotion::selectionChanged()
{
    if (placed())
        m_phase = Phase::Placing;
    else if (m_phase == Phase::Archive || m_phase == Phase::ReturningArray)
        m_phase = Phase::ReturningArray;
    m_settledFor = 0;
}
void DetailMotion::advance(double dt, bool reduced, bool aligned)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    if (m_phase == Phase::ReturningCenter && aligned) {
        m_phase = Phase::ReturningArray;
        m_settledFor = 0;
    }
    m_placement.retarget(holdsDetail() ? 1 : 0, PlacementSourceSeconds / PlacementSpeed);
    m_placement.update(dt, reduced);
}
// Do not expose content merely because placement elapsed: camera and lift must both settle.
// Reset the hold whenever either becomes active, including an interrupted selection.
void DetailMotion::updateReadiness(double dt, bool cameraSettled, bool liftSettled, bool reduced)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    m_settledFor = cameraSettled && liftSettled ? m_settledFor + dt : 0;
    if (!cameraSettled || !liftSettled || (!reduced && m_settledFor < ReadinessHoldSeconds))
        return;
    const auto previous = m_phase;
    if (m_phase == Phase::Placing)
        m_phase = Phase::Presented;
    else if (m_phase == Phase::ReturningArray)
        m_phase = Phase::Archive;
    if (previous != m_phase)
        m_settledFor = 0;
}
}
