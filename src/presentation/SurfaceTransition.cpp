#include "EasingCurve.h"
#include "Units.h"
#include "FrameClock.h"
#include "SurfaceTransition.h"
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace {
// Offsets are logical pixels; interrupted opacity determines the remaining authored duration.
constexpr auto RightEntryOffsetPixels = 24;
constexpr auto BottomEntryOffsetPixels = 10;
constexpr auto RightExitOffsetPixels = 32;
constexpr auto BottomExitOffsetPixels = 6;
constexpr auto MinimumInterruptedMilliseconds = 72.0;
constexpr auto ExitBezierFirstX = .4;
constexpr auto BrowseBezierFirstX = .45;
constexpr auto BrowseBezierSecondX = .25;
constexpr auto EnterBezierFirstX = .22;
constexpr auto EnterBezierSecondX = .36;
constexpr auto CompletionEpsilonSeconds = 1e-10;

const QEasingCurve &exitCurve()
{
    static const auto curve = yalahoo::cubicBezier(ExitBezierFirstX, 0, 1, 1);
    return curve;
}
const QEasingCurve &enterCurve(bool browse)
{
    if (browse) {
        static const auto curve =
                yalahoo::cubicBezier(BrowseBezierFirstX, 0, BrowseBezierSecondX, 1);
        return curve;
    }
    static const auto curve = yalahoo::cubicBezier(EnterBezierFirstX, 1, EnterBezierSecondX, 1);
    return curve;
}
}

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
    m_fromOffset = hidden ? (m_right ? RightEntryOffsetPixels : BottomEntryOffsetPixels) : m_offset;
    m_toOffset = show ? 0 : (m_right ? RightExitOffsetPixels : BottomExitOffsetPixels);
    const double base = show ? m_enterMs : m_exitMs;
    m_duration =
            (interrupted ? std::max(MinimumInterruptedMilliseconds,
                                    base * std::sqrt(std::abs((show ? 1.0 : 0.0) - m_fromOpacity)))
                         : base)
            / yalahoo::units::MillisecondsPerSecond;
    if (!show) {
        m_ease = exitCurve();
    } else if (m_enterCurve) {
        m_ease = *m_enterCurve;
    } else {
        m_ease = enterCurve(m_browse);
    }
    m_elapsed = 0;
    m_opacity = m_fromOpacity;
    m_offset = m_fromOffset;
    m_shown = true;
    m_running = true;
    if (reduced || (!show && hidden)) {
        finish();
    } else {
        Q_EMIT changed();
    }
}
void SurfaceTransition::advance(double seconds)
{
    if (!m_running || !std::isfinite(seconds) || seconds <= 0)
        return;
    m_elapsed = std::min(m_duration, m_elapsed + seconds);
    if (m_duration - m_elapsed < CompletionEpsilonSeconds) {
        finish();
        return;
    }
    const double t = m_ease.valueForProgress(m_elapsed / m_duration);
    m_opacity = m_fromOpacity + ((m_show ? 1 : 0) - m_fromOpacity) * t;
    m_offset = m_fromOffset + (m_toOffset - m_fromOffset) * t;
    Q_EMIT changed();
}
// Move the completion out before emitting: changed() can synchronously replace this transition.
// Only invoke the captured completion when its revision still owns the terminal state.
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
    Q_EMIT changed();
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
        Q_EMIT changed();
}
}
