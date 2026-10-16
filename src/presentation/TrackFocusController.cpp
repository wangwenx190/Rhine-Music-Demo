#include "TrackFocusController.h"
#include <algorithm>
#include <QPointF>
#include <cmath>

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
        emit rejected(QStringLiteral("这张专辑或歌曲已不在当前音乐库中，请重新扫描后重试。"));
        return false;
    }
    emit navigationRequested();
    m_albumId = albumId;
    m_trackId = trackId;
    m_row = m_library->trackIndex(albumId, trackId);
    emit changed();
    const auto phase = m_presentation->phase();
    const bool reuse = !trackId.isEmpty() && m_library->navigationAlbumId() == albumId
            && !m_presentation->pendingSelection()
            && (phase == "detail" || phase == "opening" || phase == "switching");
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
    emit changed();
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
    emit revealRequested(m_revision, m_row);
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
    emit changed();
    if (m_reduced || std::abs(target - start) < 1)
        advance(m_duration);
}
void TrackFocusController::startHighlight()
{
    m_scrolling = false;
    m_pulsing = true;
    m_elapsed = 0;
    m_highlight = m_reduced ? 1 : 0;
    emit changed();
    emit focusRequested(m_row);
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
        emit scrollChanged();
        if (t == 1)
            startHighlight();
    } else if (m_pulsing) {
        m_elapsed += dt;
        if (m_elapsed >= (m_reduced ? 1.1 : 2.)) {
            cancel();
            return;
        }
        if (!m_reduced) {
            const double t = std::fmod(m_elapsed, 1.);
            QEasingCurve ease(QEasingCurve::BezierSpline);
            ease.addCubicBezierSegment({ .42, 0 }, { .58, 1 }, { 1, 1 });
            m_highlight = t < .32 ? ease.valueForProgress(t / .32)
                    : t < .48     ? 1
                    : t < .88     ? 1 - ease.valueForProgress((t - .48) / .4)
                                  : 0;
            emit changed();
        }
    }
}
}
