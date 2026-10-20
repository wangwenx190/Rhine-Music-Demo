#include "FrameClock.h"
#include "OrdinalMotion.h"
#include "ArchivePool.h"
#include <algorithm>
#include <cmath>

namespace {
// Ordinal rolling samples the source decay curve; decimal wrap remains distinct from track time.
constexpr auto OrdinalTransitionSeconds = .460;
constexpr auto VelocityLimitGain = 12;
constexpr auto TrackSampleCount = 49;
constexpr auto SampleIntervals = 48.0;
constexpr auto LastSampleIndex = 48;
constexpr auto LastInterpolationIndex = 47;
constexpr auto DecimalRadix = 10;
constexpr auto TrackDecay = 10;
constexpr auto DigitAlignmentTolerance = .001;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
void OrdinalTrack::retarget(double to, bool instant)
{
    if (to == target && !instant)
        return;
    target = to;
    elapsed = instant ? OrdinalTransitionSeconds : 0;
    if (instant) {
        value = to;
        velocity = 0;
        points.fill(to);
        return;
    }
    const double delta = value - target;
    const double cap =
            std::max(std::abs(delta), 1.0) * VelocityLimitGain / OrdinalTransitionSeconds;
    const double initialVelocity = std::clamp(velocity, -cap, cap) * OrdinalTransitionSeconds;
    for (int i = 0; i < TrackSampleCount; ++i) {
        const double t = i / SampleIntervals;
        points[i] = i == LastSampleIndex ? target
                                         : target
                        + (delta + (initialVelocity + TrackDecay * delta) * t)
                                * std::exp(-TrackDecay * t);
    }
    // WAAPI exposes the first sampled segment immediately after replacing its track.
    advance(0);
}
void OrdinalTrack::advance(double dt)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    elapsed = std::min(OrdinalTransitionSeconds, elapsed + dt);
    if (elapsed >= std::nextafter(OrdinalTransitionSeconds, 0.0)) {
        elapsed = OrdinalTransitionSeconds;
        value = target;
        velocity = 0;
        return;
    }
    const double position = elapsed / OrdinalTransitionSeconds * LastSampleIndex;
    const int i = std::min(LastInterpolationIndex, int(std::floor(position)));
    value = points[i] + (points[i + 1] - points[i]) * (position - i);
    velocity = (points[i + 1] - points[i]) * LastSampleIndex / OrdinalTransitionSeconds;
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
    digits = std::clamp(digits, 1, ordinalMotion::MaximumDecimalDigits);
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
        const int previousDigit = ArchivePool::wrap(std::round(track.target), DecimalRadix);
        if (!instant && previousDigit == digit)
            continue;
        double target = std::floor(track.value / DecimalRadix) * DecimalRadix + digit;
        if (!instant && direction > 0 && target < track.value - DigitAlignmentTolerance)
            target += DecimalRadix;
        else if (!instant && direction < 0 && target > track.value + DigitAlignmentTolerance)
            target -= DecimalRadix;
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
        faces.append(QVariantMap{ { u"upper"_s, ArchivePool::wrap(floor, DecimalRadix) },
                                  { u"lower"_s, ArchivePool::wrap(floor + 1, DecimalRadix) },
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
        if (m_tracks[i].elapsed < OrdinalTransitionSeconds) {
            m_tracks[i].advance(dt);
            if (m_tracks[i].elapsed >= OrdinalTransitionSeconds)
                m_tracks[i].retarget(ArchivePool::wrap(m_tracks[i].target, DecimalRadix), true);
            moving = true;
        }
    if (moving)
        publish();
}
}
