#include "FrameClock.h"
#include "TransportMotion.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
double bezier(double x, bool ease)
{
    const double x1 = ease ? .25 : .4, y1 = ease ? .1 : 0;
    const double x2 = ease ? .25 : .2;
    const auto cubic = [](double t, double a, double b) {
        return 3 * (1 - t) * (1 - t) * t * a + 3 * (1 - t) * t * t * b + t * t * t;
    };
    double low = 0, high = 1;
    for (int i = 0; i < 48; ++i) {
        const double t = (low + high) / 2;
        if (cubic(t, x1, x2) < x)
            low = t;
        else
            high = t;
    }
    return cubic((low + high) / 2, y1, 1);
}
}
double TransportTrack::value() const
{
    if (elapsed >= duration)
        return target;
    return start + (target - start) * bezier(std::clamp(elapsed / duration, 0.0, 1.0), ease);
}
void TransportTrack::move(double to, double seconds, bool instant)
{
    if (target == to && !instant)
        return;
    const double current = value();
    const double previousTarget = target;
    if (!instant && elapsed < duration && to == reversingStart) {
        const double progress = bezier(std::clamp(elapsed / duration, 0.0, 1.0), ease);
        shortening = std::clamp(std::abs(progress * shortening + 1 - shortening), 0.0, 1.0);
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
    m_offset.move(18, .640, true);
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
    if (m_clock == clock)
        return;
    disconnect(m_connection);
    m_clock = clock;
    if (clock)
        m_connection = connect(clock, &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
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
    const bool instant = !m_complete || m_reduced || !m_active;
    m_width.move(m_shown ? m_targetWidth : 0, .640, instant);
    m_alpha.move(m_shown ? 1 : 0, .400, instant);
    m_offset.move(m_shown ? 0 : 18, .640, instant);
    m_icon.move(m_playing ? 1 : 0, .180, instant);
    Q_EMIT inputChanged();
    Q_EMIT changed();
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
    m_width.move(m_shown ? m_targetWidth : 0, .640, true);
    m_alpha.move(m_shown ? 1 : 0, .400, true);
    m_offset.move(m_shown ? 0 : 18, .640, true);
    m_icon.move(m_playing ? 1 : 0, .180, true);
    Q_EMIT changed();
}
}
