#include "FrameConnection.h"
#include "EasingCurve.h"
#include "TransportMotion.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
// Invert CSS Bezier x before evaluating y; geometry, opacity and icon tracks retain independent timing.
constexpr auto EaseControlX = .25;
constexpr auto TransformFirstX = .4;
constexpr auto EaseFirstY = .1;
constexpr auto TransformSecondX = .2;
constexpr auto BezierSearchSteps = 48;
constexpr auto HiddenOffsetPixels = 18;
constexpr auto GeometryDurationSeconds = .640;
constexpr auto OpacityDurationSeconds = .400;
constexpr auto IconDurationSeconds = .180;

double bezier(double x, bool ease)
{
    const double x1 = ease ? EaseControlX : TransformFirstX, y1 = ease ? EaseFirstY : 0;
    const double x2 = ease ? EaseControlX : TransformSecondX;
    return sampleCubicBezier(x, { x1, y1 }, { x2, 1 }, BezierSearchSteps);
}
}
double TransportTrack::progress() const
{
    if (elapsed != m_cachedElapsed || duration != m_cachedDuration || ease != m_cachedEase) {
        // QML can read one track several times after a notification. Reuse the exact bounded
        // Bezier inversion until one of its inputs changes, including during reversal.
        m_cachedProgress = bezier(std::clamp(elapsed / duration, 0.0, 1.0), ease);
        m_cachedElapsed = elapsed;
        m_cachedDuration = duration;
        m_cachedEase = ease;
    }
    return m_cachedProgress;
}
double TransportTrack::value() const
{
    if (elapsed >= duration)
        return target;
    return start + (target - start) * progress();
}
void TransportTrack::move(double to, double seconds, bool instant)
{
    if (target == to && !instant)
        return;
    const double current = value();
    const double previousTarget = target;
    if (!instant && elapsed < duration && to == reversingStart) {
        shortening = std::clamp(std::abs(progress() * shortening + 1 - shortening), 0.0, 1.0);
        reversingStart = previousTarget;
    } else {
        shortening = 1;
        reversingStart = current;
    }
    start = current;
    target = to;
    duration = seconds * shortening;
    elapsed = instant || start == target ? duration : 0;
}
void TransportTrack::advance(double dt)
{
    elapsed = std::min(duration, elapsed + dt);
}
TransportMotion::TransportMotion(QObject *parent) : QObject(parent)
{
    m_alpha.ease = true;
    m_icon.ease = true;
    m_offset.move(HiddenOffsetPixels, GeometryDurationSeconds, true);
}
void TransportMotion::classBegin()
{
}
void TransportMotion::componentComplete()
{
    retarget();
    m_complete = true;
}
FrameClock *TransportMotion::clock() const
{
    return m_clock;
}
void TransportMotion::setClock(FrameClock *clock)
{
    if (bindFrameClock(this, m_clock, m_connection, clock))
        Q_EMIT inputChanged();
}
double TransportMotion::targetWidth() const
{
    return m_targetWidth;
}
void TransportMotion::setTargetWidth(double width)
{
    if (!std::isfinite(width) || width < 0 || m_targetWidth == width)
        return;
    m_targetWidth = width;
    retarget();
}
bool TransportMotion::shown() const
{
    return m_shown;
}
void TransportMotion::setShown(bool shown)
{
    if (m_shown == shown)
        return;
    m_shown = shown;
    retarget();
}
bool TransportMotion::playing() const
{
    return m_playing;
}
void TransportMotion::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    retarget();
}
bool TransportMotion::reducedMotion() const
{
    return m_reduced;
}
void TransportMotion::setReducedMotion(bool reduced)
{
    if (m_reduced == reduced)
        return;
    m_reduced = reduced;
    if (reduced)
        finish();
    Q_EMIT inputChanged();
}
bool TransportMotion::active() const
{
    return m_active;
}
void TransportMotion::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    Q_EMIT inputChanged();
}
double TransportMotion::width() const
{
    return m_width.value();
}
double TransportMotion::opacity() const
{
    return m_alpha.value();
}
double TransportMotion::offset() const
{
    return m_offset.value();
}
double TransportMotion::pauseOpacity() const
{
    return m_icon.value();
}
bool TransportMotion::moving() const
{
    return m_width.elapsed < m_width.duration || m_alpha.elapsed < m_alpha.duration
            || m_offset.elapsed < m_offset.duration || m_icon.elapsed < m_icon.duration;
}
void TransportMotion::retarget()
{
    move(!m_complete || m_reduced || !m_active);
    Q_EMIT inputChanged();
    Q_EMIT changed();
}
void TransportMotion::move(bool instant)
{
    m_width.move(m_shown ? m_targetWidth : 0, GeometryDurationSeconds, instant);
    m_alpha.move(m_shown ? 1 : 0, OpacityDurationSeconds, instant);
    m_offset.move(m_shown ? 0 : HiddenOffsetPixels, GeometryDurationSeconds, instant);
    m_icon.move(m_playing ? 1 : 0, IconDurationSeconds, instant);
}
void TransportMotion::advance(double dt)
{
    if (!m_active || !moving() || !std::isfinite(dt) || dt < 0)
        return;
    m_width.advance(dt);
    m_alpha.advance(dt);
    m_offset.advance(dt);
    m_icon.advance(dt);
    Q_EMIT changed();
}
void TransportMotion::finish()
{
    move(true);
    Q_EMIT changed();
}
}
