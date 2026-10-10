#include "QuinticTrack.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
double archiveTravelDuration(double lanes, double rows)
{
    return 1.4 + std::clamp((std::hypot(lanes, rows / 8) - 1) / 7, 0.0, 1.0);
}
void QuinticTrack::observe(Kinematics state)
{
    if (!m_active) m_state = state;
}
bool QuinticTrack::retarget(double target, double duration)
{
    if (m_hasTarget && target == m_target) return false;
    m_hasTarget = true;
    m_target = target;
    m_duration = std::max(.001, duration);
    m_elapsed = 0;
    m_active = true;
    const double t = m_duration, t2 = t * t;
    const auto [value, velocity, acceleration] = m_state;
    const double distance = target - value - velocity * t - acceleration * t2 / 2;
    const double remainingVelocity = -velocity - acceleration * t;
    const double remainingAcceleration = -acceleration;
    m_coefficients = {value, velocity, acceleration / 2,
        (10 * distance - 4 * remainingVelocity * t + remainingAcceleration * t2 / 2) / std::pow(t, 3),
        (-15 * distance + 7 * remainingVelocity * t - remainingAcceleration * t2) / std::pow(t, 4),
        (6 * distance - 3 * remainingVelocity * t + remainingAcceleration * t2 / 2) / std::pow(t, 5)};
    return true;
}
void QuinticTrack::update(double dt, bool reduced)
{
    if (!m_active) return;
    m_elapsed = reduced ? m_duration : std::min(m_duration, m_elapsed + std::max(0.0, dt));
    if (m_elapsed >= m_duration) { finish(); return; }
    const auto [a, b, c, d, e, f] = m_coefficients;
    const double t = m_elapsed;
    m_state.value = a + b*t + c*std::pow(t,2) + d*std::pow(t,3) + e*std::pow(t,4) + f*std::pow(t,5);
    m_state.velocity = b + 2*c*t + 3*d*std::pow(t,2) + 4*e*std::pow(t,3) + 5*f*std::pow(t,4);
    m_state.acceleration = 2*c + 6*d*t + 12*e*std::pow(t,2) + 20*f*std::pow(t,3);
}
void QuinticTrack::translate(double offset)
{
    m_state.value += offset;
    m_coefficients[0] += offset;
    if (m_hasTarget) m_target += offset;
}
void QuinticTrack::finish()
{
    m_state = {m_target, 0, 0};
    m_elapsed = m_duration;
    m_active = false;
}
void QuinticTrack::reset()
{
    m_active = false;
    m_hasTarget = false;
    m_duration = m_elapsed = 0;
}
bool ColumnScalar::settled() const
{
    return !guided() && std::abs(m_spring.value - m_target) < .008 && std::abs(m_spring.velocity) < .025;
}
bool ColumnScalar::retarget(double target, bool guided, double duration, double rate)
{
    if (target == m_target) return false;
    m_target = target;
    m_rate = rate;
    // Reset only the polynomial, leaving the displayed derivatives intact.
    m_track.reset();
    if (guided) {
        m_track.observe(m_spring);
        m_track.retarget(target, duration);
    } else {
        m_spring.observe(target, rate);
    }
    return true;
}
void ColumnScalar::update(double dt, bool reduced)
{
    if (reduced) { finish(); return; }
    if (guided()) {
        m_track.update(dt);
        static_cast<Kinematics &>(m_spring) = m_track.state();
    } else {
        m_spring.update(m_target, m_rate, dt);
        if (std::abs(m_spring.value - m_target) < 1e-8 && std::abs(m_spring.velocity) < 1e-8) finish();
    }
}
void ColumnScalar::translate(double offset)
{
    m_spring.translate(offset);
    m_target += offset;
    m_track.translate(offset);
}
void ColumnScalar::finish()
{
    m_spring.finish(m_target);
    m_track.reset();
}
}
