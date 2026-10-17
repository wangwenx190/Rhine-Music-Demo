#include "FrameClock.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
void FrameClock::requestVisualFrame()
{
    if (m_workPending)
        return;
    m_workPending = true;
    Q_EMIT workPendingChanged();
}
void FrameClock::setApplicationVisible(bool visible)
{
    if (m_applicationVisible == visible)
        return;
    m_applicationVisible = visible;
    Q_EMIT applicationVisibleChanged();
}
void FrameClock::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    m_discardNext = true; // The first resumed QML interval may include the whole suspension.
    Q_EMIT activeChanged();
}
void FrameClock::setSpeed(double speed)
{
    if (!std::isfinite(speed))
        return;
    speed = std::clamp(speed, .25, 3.0);
    if (speed == m_speed)
        return;
    m_speed = speed;
    Q_EMIT speedChanged();
}
void FrameClock::advance(double frameSeconds)
{
    if (!m_active || !std::isfinite(frameSeconds))
        return;
    // Consume before callbacks so invalidation raised during this frame survives sleep.
    m_workPending = false;
    double dt = std::clamp(frameSeconds, 0.0, .05);
    if (m_discardNext) {
        dt = 0;
        m_discardNext = false;
    }
    const double motionDt = dt * m_speed;
    m_visualTime += motionDt;
    if (m_openingRunning) {
        m_openingTime = std::min(11.6, m_openingTime + dt);
        if (11.6 - m_openingTime < 1e-9)
            finishOpening();
    }
    ++m_frameCount;
    Q_EMIT frame(dt, motionDt);
    Q_EMIT advanced();
    if (!m_workPending)
        Q_EMIT workPendingChanged();
}
void FrameClock::startOpening()
{
    m_openingTime = 0;
    m_openingRunning = true;
    Q_EMIT openingChanged();
    Q_EMIT advanced();
}
void FrameClock::finishOpening()
{
    if (!m_openingRunning)
        return;
    m_openingTime = 11.6;
    m_openingRunning = false;
    Q_EMIT openingChanged();
    Q_EMIT openingFinished();
    Q_EMIT advanced();
}
}
