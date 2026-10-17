#pragma once
#include "LibrarySession.h"
#include <QJsonObject>
#include "opening/OpeningController.h"

namespace yalahoo {
// GUI owner of the single pending library snapshot. Rendering readiness remains event driven.
class StartupCoordinator final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visualActive READ visualActive WRITE setVisualActive NOTIFY visualActiveChanged)
public:
    StartupCoordinator(LibrarySession *library, OpeningController *opening, FrameClock *clock,
                       bool awaitInitialSnapshot, QObject *parent = nullptr);
    void receive(Snapshot snapshot);
    bool visualActive() const;
    void setVisualActive(bool active);
    Q_INVOKABLE void scenePrepared();
    QJsonObject statistics() const;
Q_SIGNALS:
    void visualActiveChanged();
    void preparationInvalidated();

private:
    void schedule();
    bool applyPending();
    LibrarySession *m_library;
    OpeningController *m_opening;
    FrameClock *m_clock;
    Snapshot m_pending;
    bool m_visualActive = true, m_initialReady, m_queued = false, m_frozen = false;
    quint64 m_received = 0, m_applied = 0, m_coalesced = 0, m_segments = 0;
};
}
