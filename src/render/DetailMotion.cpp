#include "DetailMotion.h"
#include <algorithm>
#include <cmath>

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
    m_placement.retarget(0, 1.45 / 1.05);
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
    m_placement.retarget(holdsDetail() ? 1 : 0, 1.45 / 1.05);
    m_placement.update(dt, reduced);
}
void DetailMotion::updateReadiness(double dt, bool cameraSettled, bool liftSettled, bool reduced)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    m_settledFor = cameraSettled && liftSettled ? m_settledFor + dt : 0;
    if (!cameraSettled || !liftSettled || (!reduced && m_settledFor < .08))
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
