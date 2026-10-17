#include "StartupCoordinator.h"
#include <QJsonObject>
#include <QThread>
#include <utility>

using namespace Qt::StringLiterals;

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
    Q_EMIT visualActiveChanged();
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
                    Q_EMIT preparationInvalidated();
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
        Q_EMIT preparationInvalidated();
        return;
    }
    m_frozen = true;
    m_opening->scenePrepared();
}
QJsonObject StartupCoordinator::statistics() const
{
    return { { u"receivedSnapshots"_s, qint64(m_received) },
             { u"appliedSnapshots"_s, qint64(m_applied) },
             { u"coalescedSnapshots"_s, qint64(m_coalesced) },
             { u"pendingSnapshots"_s, int(bool(m_pending)) },
             { u"initialReady"_s, m_initialReady },
             { u"visualActive"_s, m_visualActive },
             { u"snapshotFrozen"_s, m_frozen },
             { u"resumeSegments"_s, qint64(m_segments) } };
}
}
