#include "PresentationController.h"
#include <utility>
#include "EasingCurve.h"
#include "FrameClock.h"
#include <cmath>
#include <QPointF>

namespace {
// Tab retargeting starts from displayed geometry; fade and slide have independent durations.
constexpr auto TabBezierFirstX = .22;
constexpr auto TabBezierSecondX = .36;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
struct PresentationController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(PresentationController *owner, PresentationPorts ports)
        : q(owner), m_ports(std::move(ports))
    {
    }
    PresentationController *const q;

    void selectPending(const QString &albumId, const QString &trackId, bool &archiveRoute,
                       bool &openAfter);

    void commitPendingMenu();
    void browseHidden(quint64 revision);
    void detailHidden(quint64 revision);
    void switchHidden(quint64 revision);
    void advanceFrame(double dt);
    void updatePresentation();

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
                                presentationMotion::BrowseExitMilliseconds, false, true, q };
    SurfaceTransition m_detail{ presentationMotion::DetailEnterMilliseconds,
                                presentationMotion::DetailExitMilliseconds, true, false, q };
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
void PresentationController::Private::advanceFrame(double dt)
{
    transact([&] {
        m_browse.advance(dt);
        m_detail.advance(dt);
        advanceTab(dt);
        q->update();
    });
}
PresentationController::PresentationController(FrameClock *clock, PresentationPorts ports,
                                               QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, std::move(ports)))
{
    d->m_browse.reset(true);
    if (clock) {
        connect(clock, &FrameClock::frame, this, [this](double, double dt) {
            return d->advanceFrame(dt);
        });
    }
}
void PresentationController::setTabIndicator(double x, double width, bool animate)
{
    if (!std::isfinite(x) || !std::isfinite(width) || width < 0)
        return;
    d->m_tabFromX = d->m_tabX;
    d->m_tabFromWidth = d->m_tabWidth;
    d->m_tabTargetX = x;
    d->m_tabTargetWidth = width;
    d->m_tabElapsed = 0;
    if (!animate || d->m_reduced)
        d->advanceTab(yalahoo::presentationMotion::TabSlideSeconds);
}
void PresentationController::revealTabContent()
{
    d->m_tabFadeFrom = d->m_tabFadeElapsed < yalahoo::presentationMotion::TabFadeSeconds
            ? d->m_tabOpacity
            : yalahoo::presentationMotion::TabInitialOpacity;
    d->m_tabFadeElapsed = 0;
    d->m_tabOpacity = d->m_reduced ? 1 : d->m_tabFadeFrom;
    if (d->m_reduced)
        d->m_tabFadeElapsed = yalahoo::presentationMotion::TabFadeSeconds;
    Q_EMIT tabIndicatorChanged();
}
void PresentationController::Private::advanceTab(double dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    static const QEasingCurve ease = [] {
        auto curve = cubicBezier(TabBezierFirstX, 1, TabBezierSecondX, 1);
        return curve;
    }();
    bool changed = false;
    if (m_tabElapsed < yalahoo::presentationMotion::TabSlideSeconds) {
        m_tabElapsed = std::min(yalahoo::presentationMotion::TabSlideSeconds, m_tabElapsed + dt);
        const double t =
                ease.valueForProgress(m_tabElapsed / yalahoo::presentationMotion::TabSlideSeconds);
        const double x = m_tabFromX + (m_tabTargetX - m_tabFromX) * t;
        const double width = m_tabFromWidth + (m_tabTargetWidth - m_tabFromWidth) * t;
        changed = x != m_tabX || width != m_tabWidth;
        m_tabX = x;
        m_tabWidth = width;
    }
    if (m_tabFadeElapsed < yalahoo::presentationMotion::TabFadeSeconds) {
        m_tabFadeElapsed =
                std::min(yalahoo::presentationMotion::TabFadeSeconds, m_tabFadeElapsed + dt);
        m_tabOpacity = m_tabFadeFrom
                + (1 - m_tabFadeFrom)
                        * ease.valueForProgress(m_tabFadeElapsed
                                                / yalahoo::presentationMotion::TabFadeSeconds);
        changed = true;
    }
    if (changed)
        Q_EMIT q->tabIndicatorChanged();
}
void PresentationController::Private::transact(const std::function<void()> &operation)
{
    struct PresentationState final
    {
        QString phase;
        bool wantsDetail;
        std::optional<PresentationSelection> pending;
        bool operator==(const PresentationState &) const = default;
    };
    const PresentationState before{ m_phase, m_wantsDetail, m_pending };
    ++m_depth;
    operation();
    --m_depth;
    if (!m_depth && before != PresentationState{ m_phase, m_wantsDetail, m_pending })
        Q_EMIT q->changed();
}
void PresentationController::open()
{
    d->transact([&] {
        d->m_wantsDetail = true;
        if (d->m_phase == "archive"_L1) {
            if (d->m_ports.archiveReady()) {
                d->beginOpen();
            } else {
                d->m_phase = u"selecting"_s;
                d->m_browse.hide(d->m_reduced);
            }
        }
    });
}
void PresentationController::back()
{
    d->transact([&] {
        d->m_wantsDetail = false;
        d->m_pending.reset();
        if (d->m_phase != "archive"_L1 && d->m_phase != "selecting"_L1)
            d->beginExit();
    });
}
void PresentationController::Private::selectPending(const QString &albumId, const QString &trackId,
                                                    bool &archiveRoute, bool &openAfter)
{
    m_pending = PresentationSelection{ albumId, trackId, archiveRoute };
    m_wantsDetail = openAfter;
    const bool switchInDetail = openAfter && !archiveRoute;
    if (m_phase == "opening"_L1 && !m_cameraEntered)
        return;
    if (m_phase == "archive"_L1) {
        const bool committed = commitSelection(false);
        if (openAfter || !committed) {
            m_phase = u"selecting"_s;
            m_browse.hide(m_reduced);
        }
        return;
    }
    if (switchInDetail && (m_phase == "detail"_L1 || m_phase == "opening"_L1)) {
        beginDetailSwitch();
        return;
    }
    if (switchInDetail && (m_phase == "switch-hiding"_L1 || m_phase == "switching"_L1))
        return;
    if (m_phase != "selecting"_L1)
        beginExit();
}
void PresentationController::select(const QString &albumId, bool openAfter, bool archiveRoute,
                                    const QString &trackId)
{
    if (albumId.isEmpty())
        return;
    d->transact([&] {
        return d->selectPending(albumId, trackId, archiveRoute, openAfter);
    });
}
bool PresentationController::Private::navigationEnabled(int direction) const
{
    return m_ports.navigationEnabled
            ? m_ports.navigationEnabled(m_pending ? m_pending->albumId : QString(), direction)
            : q->canStepAlbum(direction);
}
bool PresentationController::canStepAlbum(int direction) const
{
    return (direction == 1 || direction == -1) && d->m_ports.adjacentAlbum
            && !d->m_ports
                        .adjacentAlbum(d->m_pending ? d->m_pending->albumId : QString(), direction)
                        .isEmpty();
}
void PresentationController::stepAlbum(int direction)
{
    if (!canStepAlbum(direction))
        return;
    select(d->m_ports.adjacentAlbum(d->m_pending ? d->m_pending->albumId : QString(), direction),
           true);
}
void PresentationController::layoutReady(quint64 revision)
{
    if (revision == d->m_layoutRevision)
        d->m_layoutReady = true;
}
void PresentationController::Private::commitPendingMenu()
{
    if (commitSelection(true))
        prepareMenu();
}
void PresentationController::Private::updatePresentation()
{
    if (m_phase == "switching"_L1) {
        if (m_pending) {
            commitPendingMenu();
            return;
        }
        if (m_browseHidden && m_ports.presentationReady() && m_layoutReady)
            showMenu();
        return;
    }
    if (m_phase == "opening"_L1) {
        if (m_browseHidden && m_cameraEntered && m_ports.presentationReady() && m_layoutReady)
            showMenu();
        return;
    }
    if (m_phase != "returning"_L1 && m_phase != "selecting"_L1)
        return;
    if (m_phase == "returning"_L1 && !m_wantsDetail && m_ports.archiveInteractive
        && m_ports.archiveInteractive()) {
        if (m_pending && !commitSelection(false))
            return;
        m_phase = u"archive"_s;
        m_browse.show(m_reduced);
        Q_EMIT q->focusRequested(false);
        return;
    }
    if (!m_ports.archiveReady())
        return;
    if (m_pending) {
        commitSelection(false);
        m_phase = u"selecting"_s;
        return;
    }
    if (m_wantsDetail) {
        beginOpen();
    } else {
        m_phase = u"archive"_s;
        m_browse.show(m_reduced);
        Q_EMIT q->focusRequested(false);
    }
}
void PresentationController::update()
{
    d->transact([&] {
        return d->updatePresentation();
    });
}
void PresentationController::Private::browseHidden(quint64 revision)
{
    transact([&] {
        if (revision != m_revision)
            return;
        m_browseHidden = true;
        if (m_pending) {
            m_phase = u"selecting"_s;
            commitSelection(false);
            return;
        }
        m_cameraEntered = true;
        m_ports.enterCamera();
    });
}
void PresentationController::Private::detailHidden(quint64 revision)
{
    transact([&] {
        if (revision == m_revision)
            returnToArchive();
    });
}
void PresentationController::Private::switchHidden(quint64 revision)
{
    transact([&] {
        if (revision != m_revision)
            return;
        if (!m_wantsDetail || (m_pending && m_pending->archiveRoute)) {
            returnToArchive();
            return;
        }
        m_phase = u"switching"_s;
        if (m_pending && commitSelection(true))
            prepareMenu();
    });
}
void PresentationController::Private::beginOpen()
{
    const auto revision = ++m_revision;
    m_phase = u"opening"_s;
    m_browseHidden = m_cameraEntered = false;
    prepareMenu();
    m_browse.hide(m_reduced, [this, revision] {
        browseHidden(revision);
    });
}
void PresentationController::Private::beginExit()
{
    if (m_phase == "hiding"_L1 || m_phase == "returning"_L1 || m_phase == "switch-hiding"_L1)
        return;
    const auto revision = ++m_revision;
    m_phase = u"hiding"_s;
    m_detail.hide(m_reduced, [revision, this] {
        detailHidden(revision);
    });
}
void PresentationController::Private::beginDetailSwitch()
{
    const auto revision = ++m_revision;
    m_phase = u"switch-hiding"_s;
    if (!m_browseHidden) {
        m_browse.hide(m_reduced, [this, revision] {
            if (revision == m_revision)
                m_browseHidden = true;
        });
    }
    m_detail.hide(m_reduced, [this, revision] {
        switchHidden(revision);
    });
}
bool PresentationController::Private::commitSelection(bool detail)
{
    if (m_ports.canSelect && !m_ports.canSelect(*m_pending))
        return false;
    const auto request = *m_pending;
    m_pending.reset();
    if (detail)
        m_ports.switchDetail(request);
    else
        m_ports.select(request);
    return true;
}
void PresentationController::Private::returnToArchive()
{
    m_phase = u"returning"_s;
    m_ports.returnCamera();
}
void PresentationController::Private::prepareMenu()
{
    m_detail.reset(false);
    m_tabOpacity = 1;
    m_tabFadeElapsed = yalahoo::presentationMotion::TabFadeSeconds;
    Q_EMIT q->tabIndicatorChanged();
    m_layoutReady = false;
    ++m_layoutRevision;
    Q_EMIT q->prepared();
}
void PresentationController::Private::showMenu()
{
    m_phase = u"detail"_s;
    m_detail.show(m_reduced);
    Q_EMIT q->focusRequested(true);
}
void PresentationController::reset()
{
    d->transact([&] {
        ++d->m_revision;
        ++d->m_layoutRevision;
        d->m_pending.reset();
        d->m_wantsDetail = d->m_browseHidden = d->m_cameraEntered = d->m_layoutReady = false;
        d->m_phase = u"archive"_s;
        d->m_detail.reset(false);
        d->m_browse.reset(true);
        Q_EMIT prepared();
    });
}
void PresentationController::setReducedMotion(bool reduced)
{
    if (d->m_reduced == reduced)
        return;
    d->transact([&] {
        d->m_reduced = reduced;
        if (reduced) {
            d->m_browse.finish();
            d->m_detail.finish();
            d->advanceTab(yalahoo::presentationMotion::TabSlideSeconds);
        }
    });
}
QString PresentationController::phase() const
{
    return d->m_phase;
}
bool PresentationController::wantsDetail() const
{
    return d->m_wantsDetail;
}
bool PresentationController::archiveEnabled() const
{
    return d->m_phase == "archive"_L1;
}
bool PresentationController::detailEnabled() const
{
    return d->m_phase == "detail"_L1;
}
quint64 PresentationController::layoutRevision() const
{
    return d->m_layoutRevision;
}
SurfaceTransition *PresentationController::browseSurface()
{
    return &d->m_browse;
}
SurfaceTransition *PresentationController::detailSurface()
{
    return &d->m_detail;
}
std::optional<PresentationSelection> PresentationController::pendingSelection() const
{
    return d->m_pending;
}
bool PresentationController::canMovePrevious() const
{
    return d->navigationEnabled(-1);
}
bool PresentationController::canMoveNext() const
{
    return d->navigationEnabled(1);
}
double PresentationController::tabOpacity() const
{
    return d->m_tabOpacity;
}
double PresentationController::tabX() const
{
    return d->m_tabX;
}
double PresentationController::tabWidth() const
{
    return d->m_tabWidth;
}
PresentationController::~PresentationController() = default;

}
