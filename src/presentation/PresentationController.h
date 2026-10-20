// Coordinates browse, detail and modal presentation around stable album identity. Revisions
// prevent interrupted readiness work from revealing stale content.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "SurfaceTransition.h"
#include <optional>

namespace yalahoo {
namespace presentationMotion {
// Surface and tab tracks use independent source-authored time windows.
inline constexpr auto BrowseEnterMilliseconds = 720;
inline constexpr auto BrowseExitMilliseconds = 140;
inline constexpr auto DetailEnterMilliseconds = 360;
inline constexpr auto DetailExitMilliseconds = 240;
inline constexpr auto TabSlideSeconds = .18;
inline constexpr auto TabInitialOpacity = .35;
inline constexpr auto TabFadeSeconds = .15;
}

struct PresentationSelection final
{
    QString albumId, trackId;
    bool archiveRoute = false;
    bool operator==(const PresentationSelection &) const = default;
};
struct PresentationPorts final
{
    std::function<bool()> presentationReady, archiveReady, archiveInteractive;
    std::function<void()> enterCamera, returnCamera;
    std::function<void(const PresentationSelection &)> select, switchDetail;
    std::function<bool(const PresentationSelection &)> canSelect;
    std::function<QString(const QString &, int)> adjacentAlbum;
    std::function<bool(const QString &, int)> navigationEnabled;
};
// GUI-thread owner. Readiness is supplied by the scene; elapsed time never substitutes for it.
class PresentationController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresentationController)
    Q_PROPERTY(double tabOpacity READ tabOpacity NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(double tabX READ tabX NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(double tabWidth READ tabWidth NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(QString phase READ phase NOTIFY changed FINAL)
    Q_PROPERTY(bool wantsDetail READ wantsDetail NOTIFY changed FINAL)
    Q_PROPERTY(bool archiveEnabled READ archiveEnabled NOTIFY changed FINAL)
    Q_PROPERTY(bool detailEnabled READ detailEnabled NOTIFY changed FINAL)
    Q_PROPERTY(bool canMovePrevious READ canMovePrevious NOTIFY changed FINAL)
    Q_PROPERTY(bool canMoveNext READ canMoveNext NOTIFY changed FINAL)
    Q_PROPERTY(quint64 layoutRevision READ layoutRevision NOTIFY prepared FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *browseSurface READ browseSurface CONSTANT FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *detailSurface READ detailSurface CONSTANT FINAL)
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
        return m_phase == "archive"_L1;
    }
    bool detailEnabled() const
    {
        return m_phase == "detail"_L1;
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
    bool canMovePrevious() const
    {
        return navigationEnabled(-1);
    }
    bool canMoveNext() const
    {
        return navigationEnabled(1);
    }
    Q_INVOKABLE void stepAlbum(int direction);
    Q_INVOKABLE bool canStepAlbum(int direction) const;
    Q_INVOKABLE void layoutReady(quint64 revision);
    Q_INVOKABLE void reset();
    void update();
    double tabOpacity() const
    {
        return m_tabOpacity;
    }
    Q_INVOKABLE void revealTabContent();
    double tabX() const
    {
        return m_tabX;
    }
    double tabWidth() const
    {
        return m_tabWidth;
    }
    Q_INVOKABLE void setTabIndicator(double x, double width, bool animate);

    void setReducedMotion(bool reduced);
Q_SIGNALS:
    void changed();
    void tabIndicatorChanged();
    void prepared();
    void focusRequested(bool detail);

private:
    void advanceTab(double dt);
    bool navigationEnabled(int direction) const;
    void transact(const std::function<void()> &operation);
    void beginOpen();
    void beginExit();
    void beginDetailSwitch();
    void returnToArchive();
    bool commitSelection(bool detail);
    void prepareMenu();
    void showMenu();
    PresentationPorts m_ports;
    SurfaceTransition m_browse{ presentationMotion::BrowseEnterMilliseconds,
                                presentationMotion::BrowseExitMilliseconds, false, true, this };
    SurfaceTransition m_detail{ presentationMotion::DetailEnterMilliseconds,
                                presentationMotion::DetailExitMilliseconds, true, false, this };
    QString m_phase = u"archive"_s;
    std::optional<PresentationSelection> m_pending;
    quint64 m_revision = 0, m_layoutRevision = 0;
    bool m_wantsDetail = false, m_browseHidden = false, m_cameraEntered = false;
    bool m_layoutReady = false, m_reduced = false;
    double m_tabX = 0, m_tabWidth = 0, m_tabFromX = 0, m_tabFromWidth = 0;
    double m_tabTargetX = 0, m_tabTargetWidth = 0,
           m_tabElapsed = presentationMotion::TabSlideSeconds;
    double m_tabOpacity = 1, m_tabFadeFrom = presentationMotion::TabInitialOpacity,
           m_tabFadeElapsed = presentationMotion::TabFadeSeconds;
    int m_depth = 0;
};
}
