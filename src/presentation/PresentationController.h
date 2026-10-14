#pragma once
#include "SurfaceTransition.h"
#include <optional>

namespace yalahoo {
struct PresentationSelection
{
    QString albumId, trackId;
    bool archiveRoute = false;
    bool operator==(const PresentationSelection &) const = default;
};
struct PresentationPorts
{
    std::function<bool()> presentationReady, archiveReady, archiveInteractive;
    std::function<void()> enterCamera, returnCamera;
    std::function<void(const PresentationSelection &)> select, switchDetail;
};
// GUI-thread owner. Readiness is supplied by the scene; elapsed time never substitutes for it.
class PresentationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(bool wantsDetail READ wantsDetail NOTIFY changed)
    Q_PROPERTY(bool archiveEnabled READ archiveEnabled NOTIFY changed)
    Q_PROPERTY(bool detailEnabled READ detailEnabled NOTIFY changed)
    Q_PROPERTY(quint64 layoutRevision READ layoutRevision NOTIFY prepared)
    Q_PROPERTY(yalahoo::SurfaceTransition *browseSurface READ browseSurface CONSTANT)
    Q_PROPERTY(yalahoo::SurfaceTransition *detailSurface READ detailSurface CONSTANT)
public:
    explicit PresentationController(FrameClock *clock, PresentationPorts ports,
                                    QObject *parent = nullptr);
    QString phase() const
    {
        return m_phase;
    }
    bool wantsDetail() const
    {
        return m_wantsDetail;
    }
    bool archiveEnabled() const
    {
        return m_phase == "archive";
    }
    bool detailEnabled() const
    {
        return m_phase == "detail";
    }
    quint64 layoutRevision() const
    {
        return m_layoutRevision;
    }
    SurfaceTransition *browseSurface()
    {
        return &m_browse;
    }
    SurfaceTransition *detailSurface()
    {
        return &m_detail;
    }
    std::optional<PresentationSelection> pendingSelection() const
    {
        return m_pending;
    }
    Q_INVOKABLE void open();
    Q_INVOKABLE void back();
    Q_INVOKABLE void select(const QString &albumId, bool openAfter, bool archiveRoute = false,
                            const QString &trackId = { });
    Q_INVOKABLE void layoutReady(quint64 revision);
    Q_INVOKABLE void reset();
    void update();
    void setReducedMotion(bool reduced);
signals:
    void changed();
    void prepared();
    void focusRequested(bool detail);

private:
    void transact(const std::function<void()> &operation);
    void beginOpen();
    void beginExit();
    void beginDetailSwitch();
    void returnToArchive();
    void commitSelection(bool detail);
    void prepareMenu();
    void showMenu();
    PresentationPorts m_ports;
    SurfaceTransition m_browse{ 720, 140, false, true, this };
    SurfaceTransition m_detail{ 360, 240, true, false, this };
    QString m_phase = "archive";
    std::optional<PresentationSelection> m_pending;
    quint64 m_revision = 0, m_layoutRevision = 0;
    bool m_wantsDetail = false, m_browseHidden = false, m_cameraEntered = false;
    bool m_layoutReady = false, m_reduced = false;
    int m_depth = 0;
};
}
