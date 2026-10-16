#include "StartupCoordinator.h"
#include <QJsonObject>
#include <QThread>
#include <utility>

namespace yalahoo {
StartupCoordinator::StartupCoordinator(LibrarySession *library, OpeningController *opening,
                                       FrameClock *clock, bool awaitInitialSnapshot,
                                       QObject *parent)
    : QObject(parent),
      m_library(library),
      m_opening(opening),
      m_clock(clock),
      m_initialReady(!awaitInitialSnapshot)
{
    connect(opening, &OpeningController::started, this, [this] {
        m_frozen = false;
        schedule();
    });
    connect(opening, &OpeningController::handedOff, this, [this] {
        m_frozen = false;
        schedule();
    });
}
bool StartupCoordinator::visualActive() const
{
    return m_visualActive;
}
void StartupCoordinator::setVisualActive(bool active)
{
    if (m_visualActive == active)
        return;
    m_visualActive = active;
    if (!active)
        m_clock->setActive(false);
    else {
        ++m_segments;
        schedule();
    }
    emit visualActiveChanged();
}
void StartupCoordinator::receive(Snapshot snapshot)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!snapshot)
        return;
    ++m_received;
    m_coalesced += bool(m_pending);
    m_pending = std::move(snapshot);
    schedule();
}
void StartupCoordinator::schedule()
{
    if (m_queued || !m_visualActive || m_frozen || !m_pending)
        return;
    m_queued = true;
    QMetaObject::invokeMethod(
            this,
            [this] {
                m_queued = false;
                if (m_visualActive && !m_frozen && applyPending() && m_opening->preparing())
                    emit preparationInvalidated();
            },
            Qt::QueuedConnection);
}
bool StartupCoordinator::applyPending()
{
    if (!m_pending)
        return false;
    const auto snapshot = std::exchange(m_pending, { });
    m_initialReady = true;
    m_library->setSnapshot(*snapshot);
    ++m_applied;
    return true;
}
void StartupCoordinator::scenePrepared()
{
    if (!m_visualActive || !m_initialReady || !m_opening->preparing())
        return;
    // A scan delivered after the last polish must get its own real cover/render acknowledgement.
    if (applyPending()) {
        emit preparationInvalidated();
        return;
    }
    m_frozen = true;
    m_opening->scenePrepared();
}
QJsonObject StartupCoordinator::statistics() const
{
    return { { "receivedSnapshots", qint64(m_received) },
             { "appliedSnapshots", qint64(m_applied) },
             { "coalescedSnapshots", qint64(m_coalesced) },
             { "pendingSnapshots", int(bool(m_pending)) },
             { "initialReady", m_initialReady },
             { "visualActive", m_visualActive },
             { "snapshotFrozen", m_frozen },
             { "resumeSegments", qint64(m_segments) } };
}
}
