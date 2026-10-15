#include "SurfaceTransition.h"
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace yalahoo {
SurfaceTransition::SurfaceTransition(double enterMs, double exitMs, bool right, bool browse,
                                     QObject *parent)
    : QObject(parent), m_enterMs(enterMs), m_exitMs(exitMs), m_right(right), m_browse(browse)
{
}
void SurfaceTransition::show(bool reduced)
{
    run(true, reduced, { });
}
void SurfaceTransition::hide(bool reduced, std::function<void()> finished)
{
    run(false, reduced, std::move(finished));
}
void SurfaceTransition::run(bool show, bool reduced, std::function<void()> finished)
{
    ++m_revision;
    const bool hidden = !m_shown;
    const bool interrupted = m_running;
    m_finished = std::move(finished); // Drops the obsolete completion before replacing its state.
    m_show = show;
    m_fromOpacity = hidden ? 0 : m_opacity;
    m_fromOffset = hidden ? (m_right ? 24 : 10) : m_offset;
    m_toOffset = show ? 0 : (m_right ? 32 : 6);
    const double base = show ? m_enterMs : m_exitMs;
    m_duration =
            (interrupted ? std::max(72.0,
                                    base * std::sqrt(std::abs((show ? 1.0 : 0.0) - m_fromOpacity)))
                         : base)
            / 1000;
    m_ease = QEasingCurve(QEasingCurve::BezierSpline);
    if (!show) {
        m_ease.addCubicBezierSegment({ .4, 0 }, { 1, 1 }, { 1, 1 });
    } else if (m_enterCurve) {
        m_ease = *m_enterCurve;
    } else if (m_browse) {
        m_ease.addCubicBezierSegment({ .45, 0 }, { .25, 1 }, { 1, 1 });
    } else {
        m_ease.addCubicBezierSegment({ .22, 1 }, { .36, 1 }, { 1, 1 });
    }
    m_elapsed = 0;
    m_opacity = m_fromOpacity;
    m_offset = m_fromOffset;
    m_shown = true;
    m_running = true;
    if (reduced || (!show && hidden)) {
        finish();
    } else {
        emit changed();
    }
}
void SurfaceTransition::advance(double seconds)
{
    if (!m_running || !std::isfinite(seconds) || seconds <= 0)
        return;
    m_elapsed = std::min(m_duration, m_elapsed + seconds);
    if (m_duration - m_elapsed < 1e-10) {
        finish();
        return;
    }
    const double t = m_ease.valueForProgress(m_elapsed / m_duration);
    m_opacity = m_fromOpacity + ((m_show ? 1 : 0) - m_fromOpacity) * t;
    m_offset = m_fromOffset + (m_toOffset - m_fromOffset) * t;
    emit changed();
}
void SurfaceTransition::finish()
{
    if (!m_running)
        return;
    m_opacity = m_show ? 1 : 0;
    m_offset = m_toOffset;
    m_shown = m_show;
    m_running = false;
    const auto revision = m_revision;
    auto callback = std::move(m_finished);
    m_finished = { };
    emit changed();
    if (callback && revision == m_revision)
        callback();
}
void SurfaceTransition::reset(bool shown)
{
    ++m_revision;
    m_finished = { };
    const bool changedState =
            m_running || m_shown != shown || m_opacity != (shown ? 1 : 0) || m_offset != 0;
    m_running = false;
    m_show = m_shown = shown;
    m_opacity = shown ? 1 : 0;
    m_offset = 0;
    if (changedState)
        emit changed();
}
}
