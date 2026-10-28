#include "FrameClock.h"
#include "PanelController.h"
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
PanelController::PanelController(FrameClock *clock, QObject *parent) : QObject(parent)
{
    connect(clock, &FrameClock::frame, this, [this](double, double dt) {
        m_surface.advance(dt);
    });
}
void PanelController::open(const QString &panel)
{
    if (panel != "search"_L1 && panel != "library"_L1 && panel != "playback"_L1
        && panel != "settings"_L1)
        return;
    const auto revision = ++m_revision;
    m_pendingAlbum.clear();
    m_pendingTrack.clear();
    m_panel = panel;
    Q_EMIT changed();
    if (revision == m_revision)
        m_surface.show(m_reduced);
}
void PanelController::close()
{
    m_pendingAlbum.clear();
    m_pendingTrack.clear();
    beginClose();
}
void PanelController::selectResult(const QString &albumId, const QString &trackId)
{
    if (m_panel != "search"_L1 || !m_surface.shown() || albumId.isEmpty())
        return;
    m_pendingAlbum = albumId;
    m_pendingTrack = trackId;
    beginClose();
}
void PanelController::beginClose()
{
    const auto revision = ++m_revision;
    m_surface.hide(m_reduced, [this, revision] {
        // Signals may synchronously reopen another panel. Only the completed intent can navigate.
        if (revision != m_revision)
            return;
        const auto album = std::exchange(m_pendingAlbum, { });
        const auto track = std::exchange(m_pendingTrack, { });
        m_panel.clear();
        Q_EMIT changed();
        if (revision != m_revision)
            return;
        Q_EMIT closed();
        if (revision == m_revision && !album.isEmpty())
            Q_EMIT resultRequested(album, track);
    });
}
void PanelController::reset()
{
    const auto revision = m_revision + 1;
    close();
    if (revision == m_revision)
        m_surface.finish();
}
void PanelController::setReducedMotion(bool reduced)
{
    m_reduced = reduced;
    if (reduced)
        m_surface.finish();
}
}
