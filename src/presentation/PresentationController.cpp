#include "PresentationController.h"
#include <tuple>
#include <cmath>
#include <QPointF>

using namespace Qt::StringLiterals;

namespace yalahoo {
PresentationController::PresentationController(FrameClock *clock, PresentationPorts ports,
                                               QObject *parent)
    : QObject(parent), m_ports(std::move(ports))
{
    m_browse.reset(true);
    if (clock) {
        connect(clock, &FrameClock::frame, this, [this](double, double dt) {
            transact([&] {
                m_browse.advance(dt);
                m_detail.advance(dt);
                advanceTab(dt);
                update();
            });
        });
    }
}
void PresentationController::setTabIndicator(double x, double width, bool animate)
{
    if (!std::isfinite(x) || !std::isfinite(width) || width < 0)
        return;
    m_tabFromX = m_tabX;
    m_tabFromWidth = m_tabWidth;
    m_tabTargetX = x;
    m_tabTargetWidth = width;
    m_tabElapsed = 0;
    if (!animate || m_reduced)
        advanceTab(.18);
}
void PresentationController::revealTabContent()
{
    m_tabFadeFrom = m_tabFadeElapsed < .15 ? m_tabOpacity : .35;
    m_tabFadeElapsed = 0;
    m_tabOpacity = m_reduced ? 1 : m_tabFadeFrom;
    if (m_reduced)
        m_tabFadeElapsed = .15;
    Q_EMIT tabIndicatorChanged();
}
void PresentationController::advanceTab(double dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    static const QEasingCurve ease = [] {
        QEasingCurve curve(QEasingCurve::BezierSpline);
        curve.addCubicBezierSegment({ .22, 1 }, { .36, 1 }, { 1, 1 });
        return curve;
    }();
    bool changed = false;
    if (m_tabElapsed < .18) {
        m_tabElapsed = std::min(.18, m_tabElapsed + dt);
        const double t = ease.valueForProgress(m_tabElapsed / .18);
        const double x = m_tabFromX + (m_tabTargetX - m_tabFromX) * t;
        const double width = m_tabFromWidth + (m_tabTargetWidth - m_tabFromWidth) * t;
        changed = x != m_tabX || width != m_tabWidth;
        m_tabX = x;
        m_tabWidth = width;
    }
    if (m_tabFadeElapsed < .15) {
        m_tabFadeElapsed = std::min(.15, m_tabFadeElapsed + dt);
        m_tabOpacity =
                m_tabFadeFrom + (1 - m_tabFadeFrom) * ease.valueForProgress(m_tabFadeElapsed / .15);
        changed = true;
    }
    if (changed)
        Q_EMIT tabIndicatorChanged();
}
void PresentationController::transact(const std::function<void()> &operation)
{
    const auto before = std::tuple(m_phase, m_wantsDetail, m_pending);
    ++m_depth;
    operation();
    --m_depth;
    if (!m_depth && before != std::tuple(m_phase, m_wantsDetail, m_pending))
        Q_EMIT changed();
}
void PresentationController::open()
{
    transact([&] {
        m_wantsDetail = true;
        if (m_phase == "archive"_L1) {
            if (m_ports.archiveReady()) {
                beginOpen();
            } else {
                m_phase = "selecting"_L1;
                m_browse.hide(m_reduced);
            }
        }
    });
}
void PresentationController::back()
{
    transact([&] {
        m_wantsDetail = false;
        m_pending.reset();
        if (m_phase != "archive"_L1 && m_phase != "selecting"_L1)
            beginExit();
    });
}
void PresentationController::select(const QString &albumId, bool openAfter, bool archiveRoute,
                                    const QString &trackId)
{
    if (albumId.isEmpty())
        return;
    transact([&] {
        m_pending = PresentationSelection{ albumId, trackId, archiveRoute };
        m_wantsDetail = openAfter;
        const bool switchInDetail = openAfter && !archiveRoute;
        if (m_phase == "opening"_L1 && !m_cameraEntered)
            return;
        if (m_phase == "archive"_L1) {
            const bool committed = commitSelection(false);
            if (openAfter || !committed) {
                m_phase = "selecting"_L1;
                m_browse.hide(m_reduced);
            }
        } else if (switchInDetail && (m_phase == "detail"_L1 || m_phase == "opening"_L1)) {
            beginDetailSwitch();
        } else if (switchInDetail && (m_phase == "switch-hiding"_L1 || m_phase == "switching"_L1)) {
            return;
        } else if (m_phase != "selecting"_L1) {
            beginExit();
        }
    });
}
bool PresentationController::navigationEnabled(int direction) const
{
    return m_ports.navigationEnabled
            ? m_ports.navigationEnabled(m_pending ? m_pending->albumId : QString(), direction)
            : canStepAlbum(direction);
}
bool PresentationController::canStepAlbum(int direction) const
{
    return (direction == 1 || direction == -1) && m_ports.adjacentAlbum
            && !m_ports.adjacentAlbum(m_pending ? m_pending->albumId : QString(), direction)
                        .isEmpty();
}
void PresentationController::stepAlbum(int direction)
{
    if (!canStepAlbum(direction))
        return;
    select(m_ports.adjacentAlbum(m_pending ? m_pending->albumId : QString(), direction), true);
}
void PresentationController::layoutReady(quint64 revision)
{
    if (revision == m_layoutRevision)
        m_layoutReady = true;
}
void PresentationController::update()
{
    transact([&] {
        if (m_phase == "switching"_L1) {
            if (m_pending) {
                if (commitSelection(true))
                    prepareMenu();
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
            m_phase = "archive"_L1;
            m_browse.show(m_reduced);
            Q_EMIT focusRequested(false);
            return;
        }
        if (!m_ports.archiveReady())
            return;
        if (m_pending) {
            commitSelection(false);
            m_phase = "selecting"_L1;
            return;
        }
        if (m_wantsDetail) {
            beginOpen();
        } else {
            m_phase = "archive"_L1;
            m_browse.show(m_reduced);
            Q_EMIT focusRequested(false);
        }
    });
}
void PresentationController::beginOpen()
{
    const auto revision = ++m_revision;
    m_phase = "opening"_L1;
    m_browseHidden = m_cameraEntered = false;
    prepareMenu();
    m_browse.hide(m_reduced, [this, revision] {
        transact([&] {
            if (revision != m_revision)
                return;
            m_browseHidden = true;
            if (m_pending) {
                m_phase = "selecting"_L1;
                commitSelection(false);
                return;
            }
            m_cameraEntered = true;
            m_ports.enterCamera();
        });
    });
}
void PresentationController::beginExit()
{
    if (m_phase == "hiding"_L1 || m_phase == "returning"_L1 || m_phase == "switch-hiding"_L1)
        return;
    const auto revision = ++m_revision;
    m_phase = "hiding"_L1;
    m_detail.hide(m_reduced, [this, revision] {
        transact([&] {
            if (revision == m_revision)
                returnToArchive();
        });
    });
}
void PresentationController::beginDetailSwitch()
{
    const auto revision = ++m_revision;
    m_phase = "switch-hiding"_L1;
    if (!m_browseHidden) {
        m_browse.hide(m_reduced, [this, revision] {
            if (revision == m_revision)
                m_browseHidden = true;
        });
    }
    m_detail.hide(m_reduced, [this, revision] {
        transact([&] {
            if (revision != m_revision)
                return;
            if (!m_wantsDetail || (m_pending && m_pending->archiveRoute)) {
                returnToArchive();
                return;
            }
            m_phase = "switching"_L1;
            if (m_pending) {
                if (commitSelection(true))
                    prepareMenu();
            }
        });
    });
}
bool PresentationController::commitSelection(bool detail)
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
void PresentationController::returnToArchive()
{
    m_phase = "returning"_L1;
    m_ports.returnCamera();
}
void PresentationController::prepareMenu()
{
    m_detail.reset(false);
    m_tabOpacity = 1;
    m_tabFadeElapsed = .15;
    Q_EMIT tabIndicatorChanged();
    m_layoutReady = false;
    ++m_layoutRevision;
    Q_EMIT prepared();
}
void PresentationController::showMenu()
{
    m_phase = "detail"_L1;
    m_detail.show(m_reduced);
    Q_EMIT focusRequested(true);
}
void PresentationController::reset()
{
    transact([&] {
        ++m_revision;
        ++m_layoutRevision;
        m_pending.reset();
        m_wantsDetail = m_browseHidden = m_cameraEntered = m_layoutReady = false;
        m_phase = "archive"_L1;
        m_detail.reset(false);
        m_browse.reset(true);
        Q_EMIT prepared();
    });
}
void PresentationController::setReducedMotion(bool reduced)
{
    if (m_reduced == reduced)
        return;
    transact([&] {
        m_reduced = reduced;
        if (reduced) {
            m_browse.finish();
            m_detail.finish();
            advanceTab(.18);
        }
    });
}
}
