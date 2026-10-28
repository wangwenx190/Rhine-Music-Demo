// Gates opening handoff on current resource and scene readiness. Revision checks reject
// readiness from a superseded startup or library request.

#pragma once
#include <QObject>
#include "MusicTypes.h"
#include <QJsonObject>

namespace yalahoo {
class LibrarySession;
class OpeningController;
class FrameClock;

// GUI owner of the single pending library snapshot. Rendering readiness remains event driven.
class StartupCoordinator final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StartupCoordinator)
    Q_PROPERTY(bool visualActive READ visualActive WRITE setVisualActive NOTIFY visualActiveChanged FINAL)
public:
    StartupCoordinator(LibrarySession *library, OpeningController *opening, FrameClock *clock,
                       bool awaitInitialSnapshot, QObject *parent = nullptr);
    void receive(Snapshot snapshot);
    bool visualActive() const;
    void setVisualActive(bool active);
    Q_INVOKABLE void scenePrepared();
    bool initialReady() const;
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
