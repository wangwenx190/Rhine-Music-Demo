#include "OrdinalMotion.h"
#include "ArchivePool.h"
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
void OrdinalTrack::retarget(double to, bool instant)
{
    if (to == target && !instant)
        return;
    target = to;
    elapsed = instant ? .460 : 0;
    if (instant) {
        value = to;
        velocity = 0;
        points.fill(to);
        return;
    }
    const double delta = value - target;
    const double cap = std::max(std::abs(delta), 1.0) * 12 / .460;
    const double initialVelocity = std::clamp(velocity, -cap, cap) * .460;
    for (int i = 0; i < 49; ++i) {
        const double t = i / 48.0;
        points[i] = i == 48
                ? target
                : target + (delta + (initialVelocity + 10 * delta) * t) * std::exp(-10 * t);
    }
    // WAAPI exposes the first sampled segment immediately after replacing its track.
    advance(0);
}
void OrdinalTrack::advance(double dt)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    elapsed = std::min(.460, elapsed + dt);
    if (elapsed >= std::nextafter(.460, 0.0)) {
        elapsed = .460;
        value = target;
        velocity = 0;
        return;
    }
    const double position = elapsed / .460 * 48;
    const int i = std::min(47, int(std::floor(position)));
    value = points[i] + (points[i + 1] - points[i]) * (position - i);
    velocity = (points[i + 1] - points[i]) * 48 / .460;
}
OrdinalMotion::OrdinalMotion(QObject *parent) : QObject(parent)
{
    retarget(true);
}
void OrdinalMotion::classBegin()
{
}
void OrdinalMotion::componentComplete()
{
    m_initialized = true;
}
FrameClock *OrdinalMotion::clock() const
{
    return m_clock;
}
void OrdinalMotion::setClock(FrameClock *clock)
{
    if (m_clock == clock)
        return;
    disconnect(m_connection);
    m_clock = clock;
    if (clock)
        m_connection = connect(clock, &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
    Q_EMIT clockChanged();
}
int OrdinalMotion::value() const
{
    return m_value;
}
void OrdinalMotion::setValue(int value)
{
    value = std::max(0, value);
    if (m_value == value)
        return;
    m_previous = m_value;
    m_value = value;
    retarget(!m_initialized || m_reduced || !m_active);
    m_initialized = true;
    Q_EMIT valueChanged();
}
int OrdinalMotion::digits() const
{
    return m_digits;
}
void OrdinalMotion::setDigits(int digits)
{
    digits = std::clamp(digits, 1, 10);
    if (m_digits == digits)
        return;
    m_digits = digits;
    retarget(true);
    Q_EMIT digitsChanged();
}
int OrdinalMotion::direction() const
{
    return m_direction;
}
void OrdinalMotion::setDirection(int direction)
{
    direction = (direction > 0) - (direction < 0);
    if (m_direction == direction)
        return;
    m_direction = direction;
    Q_EMIT directionChanged();
}
bool OrdinalMotion::reducedMotion() const
{
    return m_reduced;
}
void OrdinalMotion::setReducedMotion(bool reduced)
{
    if (m_reduced == reduced)
        return;
    m_reduced = reduced;
    if (reduced)
        retarget(true);
    Q_EMIT reducedMotionChanged();
}
bool OrdinalMotion::active() const
{
    return m_active;
}
void OrdinalMotion::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    Q_EMIT activeChanged();
}
void OrdinalMotion::retarget(bool instant)
{
    const QString text = QString::number(m_value).rightJustified(m_digits, '0'_L1);
    m_count = text.size();
    const int direction = m_direction ? m_direction : (m_value > m_previous ? 1 : -1);
    // Tracks use right-aligned decimal identity; only two visible faces are published per digit.
    for (int i = 0; i < m_count; ++i) {
        auto &track = m_tracks[i];
        const int digit = text[m_count - i - 1].digitValue();
        const int previousDigit = ArchivePool::wrap(std::round(track.target), 10);
        if (!instant && previousDigit == digit)
            continue;
        double target = std::floor(track.value / 10) * 10 + digit;
        if (!instant && direction > 0 && target < track.value - .001)
            target += 10;
        else if (!instant && direction < 0 && target > track.value + .001)
            target -= 10;
        track.retarget(instant ? digit : target, instant);
    }
    publish();
}
QVariantList OrdinalMotion::faces() const
{
    return m_faces;
}
void OrdinalMotion::publish()
{
    QVariantList faces;
    faces.reserve(m_count);
    for (int i = m_count - 1; i >= 0; --i) {
        const double position = m_tracks[i].value;
        const double floor = std::floor(position);
        faces.append(QVariantMap{ { u"upper"_s, ArchivePool::wrap(floor, 10) },
                                  { u"lower"_s, ArchivePool::wrap(floor + 1, 10) },
                                  { u"fraction"_s, position - floor } });
    }
    m_faces = std::move(faces);
    Q_EMIT changed();
}
void OrdinalMotion::advance(double dt)
{
    if (!m_active || !std::isfinite(dt) || dt < 0)
        return;
    bool moving = false;
    for (int i = 0; i < m_count; ++i)
        if (m_tracks[i].elapsed < .460) {
            m_tracks[i].advance(dt);
            if (m_tracks[i].elapsed >= .460)
                m_tracks[i].retarget(ArchivePool::wrap(m_tracks[i].target, 10), true);
            moving = true;
        }
    if (moving)
        publish();
}
}
