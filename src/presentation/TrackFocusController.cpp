#include "FrameClock.h"
#include "PresentationController.h"
#include "LibrarySession.h"
#include <QCoreApplication>
#include "TrackFocusController.h"
#include <algorithm>
#include <QPointF>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
TrackFocusController::TrackFocusController(LibrarySession *library,
                                           PresentationController *presentation, FrameClock *clock,
                                           QObject *parent)
    : QObject(parent), m_library(library), m_presentation(presentation)
{
    connect(clock, &FrameClock::frame, this, [this](double dt, double) {
        advance(dt);
    });
    connect(library, &LibrarySession::snapshotChanged, this, &TrackFocusController::cancel);
    connect(presentation, &PresentationController::changed, this, [this] {
        if (!m_presentation->wantsDetail())
            cancel();
        else
            ready();
    });
    connect(library, &LibrarySession::navigationChanged, this, [this] {
        if (m_requested && m_library->navigationAlbumId() != m_albumId)
            cancel();
    });
}
bool TrackFocusController::request(const QString &albumId, const QString &trackId)
{
    cancel();
    if (!m_library->contains(albumId, trackId)) {
        Q_EMIT rejected(QCoreApplication::translate(
                "Yalahoo",
                "This album or track is no longer in the library. Rescan and try again."));
        return false;
    }
    Q_EMIT navigationRequested();
    m_albumId = albumId;
    m_trackId = trackId;
    m_row = m_library->trackIndex(albumId, trackId);
    Q_EMIT changed();
    const auto phase = m_presentation->phase();
    const bool reuse = !trackId.isEmpty() && m_library->navigationAlbumId() == albumId
            && !m_presentation->pendingSelection()
            && (phase == "detail"_L1 || phase == "opening"_L1 || phase == "switching"_L1);
    if (!reuse)
        m_presentation->select(albumId, true, true, trackId);
    else
        m_library->navigate(albumId, trackId);
    ready();
    return true;
}
bool TrackFocusController::locatePlayback()
{
    return request(m_library->playbackAlbumId(), m_library->playbackTrackId());
}
void TrackFocusController::cancel()
{
    ++m_revision;
    m_albumId.clear();
    m_trackId.clear();
    m_row = -1;
    m_requested = m_scrolling = m_pulsing = false;
    m_highlight = 0;
    Q_EMIT changed();
}
void TrackFocusController::ready()
{
    if (!active() || m_requested || !m_presentation->detailEnabled()
        || m_library->navigationAlbumId() != m_albumId)
        return;
    if (m_trackId.isEmpty()) {
        cancel();
        return;
    }
    m_requested = true;
    Q_EMIT revealRequested(m_revision, m_row);
}
void TrackFocusController::beginReveal(quint64 revision, double start, double target)
{
    if (revision != m_revision || !active() || !m_requested || !std::isfinite(start)
        || !std::isfinite(target))
        return;
    m_start = m_scrollY = start;
    m_target = target;
    m_elapsed = 0;
    m_duration = std::min(.7, .32 + std::abs(target - start) * .00012);
    m_scrolling = true;
    Q_EMIT changed();
    if (revision != m_revision || !m_scrolling)
        return;
    if (m_reduced || std::abs(target - start) < 1)
        advance(m_duration);
}
void TrackFocusController::beginListReveal(quint64 revision, double startRelative, double origin,
                                           double low, double high, double rowTop, double rowHeight,
                                           double viewportHeight, double headerHeight)
{
    if (revision != m_revision || !active() || !m_requested || !std::isfinite(startRelative)
        || !std::isfinite(origin) || !std::isfinite(low) || !std::isfinite(high) || high < low
        || !std::isfinite(rowTop) || !std::isfinite(rowHeight) || !std::isfinite(viewportHeight)
        || !std::isfinite(headerHeight))
        return;
    m_listOrigin = origin;
    m_listLow = low;
    m_listHigh = high;
    const double offset =
            headerHeight + std::max(0., (viewportHeight - headerHeight - rowHeight) / 2);
    m_listTarget = std::clamp(rowTop - origin - offset, low, high);
    m_listOffset = 0;
    beginReveal(revision, startRelative + origin, m_listTarget + origin);
}
void TrackFocusController::refineListReveal(double origin, double rowTop, double rowHeight,
                                            double viewportHeight, double headerHeight, double high)
{
    if (!active() || !std::isfinite(origin) || !std::isfinite(rowTop) || !std::isfinite(rowHeight)
        || !std::isfinite(viewportHeight) || !std::isfinite(headerHeight))
        return;
    if (!std::isfinite(high))
        high = m_listHigh;
    high = std::max(m_listLow, high);
    const double offset =
            headerHeight + std::max(0., (viewportHeight - headerHeight - rowHeight) / 2);
    m_listOffset = std::clamp(rowTop - origin - offset, m_listLow, high) - m_listTarget;
}
double TrackFocusController::listScrollY(double origin) const
{
    return m_scrollY + origin - m_listOrigin + m_listOffset;
}
void TrackFocusController::startHighlight()
{
    m_scrolling = false;
    m_pulsing = true;
    m_elapsed = 0;
    m_highlight = m_reduced ? 1 : 0;
    const auto revision = m_revision;
    Q_EMIT changed();
    if (revision == m_revision && m_pulsing)
        Q_EMIT focusRequested(m_row);
}
void TrackFocusController::setReducedMotion(bool reduced)
{
    m_reduced = reduced;
    if (reduced && m_scrolling)
        advance(m_duration);
}
void TrackFocusController::advance(double dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    if (m_scrolling) {
        m_elapsed += dt;
        const double t = std::min(1., m_elapsed / m_duration);
        const double eased = t < .5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
        m_scrollY = m_start + (m_target - m_start) * eased;
        const auto revision = m_revision;
        Q_EMIT scrollChanged();
        if (revision == m_revision && m_scrolling && t == 1)
            startHighlight();
    } else if (m_pulsing) {
        m_elapsed += dt;
        if (m_elapsed >= (m_reduced ? 1.1 : 2.)) {
            cancel();
            return;
        }
        if (!m_reduced) {
            const double t = std::fmod(m_elapsed, 1.);
            static const QEasingCurve ease = [] {
                QEasingCurve curve(QEasingCurve::BezierSpline);
                curve.addCubicBezierSegment({ .42, 0 }, { .58, 1 }, { 1, 1 });
                return curve;
            }();
            m_highlight = t < .32 ? ease.valueForProgress(t / .32)
                    : t < .48     ? 1
                    : t < .88     ? 1 - ease.valueForProgress((t - .48) / .4)
                                  : 0;
            Q_EMIT changed();
        }
    }
}
}
