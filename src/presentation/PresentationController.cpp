#include "PresentationController.h"
#include <tuple>

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
                update();
            });
        });
    }
}
void PresentationController::transact(const std::function<void()> &operation)
{
    const auto before = std::tuple(m_phase, m_wantsDetail, m_pending);
    ++m_depth;
    operation();
    --m_depth;
    if (!m_depth && before != std::tuple(m_phase, m_wantsDetail, m_pending))
        emit changed();
}
void PresentationController::open()
{
    transact([&] {
        m_wantsDetail = true;
        if (m_phase == "archive") {
            if (m_ports.archiveReady()) {
                beginOpen();
            } else {
                m_phase = "selecting";
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
        if (m_phase != "archive" && m_phase != "selecting")
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
        if (m_phase == "opening" && !m_cameraEntered)
            return;
        if (m_phase == "archive") {
            commitSelection(false);
            if (openAfter) {
                m_phase = "selecting";
                m_browse.hide(m_reduced);
            }
        } else if (switchInDetail && (m_phase == "detail" || m_phase == "opening")) {
            beginDetailSwitch();
        } else if (switchInDetail && (m_phase == "switch-hiding" || m_phase == "switching")) {
            return;
        } else if (m_phase != "selecting") {
            beginExit();
        }
    });
}
void PresentationController::layoutReady(quint64 revision)
{
    if (revision == m_layoutRevision)
        m_layoutReady = true;
}
void PresentationController::update()
{
    transact([&] {
        if (m_phase == "switching") {
            if (m_pending) {
                commitSelection(true);
                prepareMenu();
                return;
            }
            if (m_browseHidden && m_ports.presentationReady() && m_layoutReady)
                showMenu();
            return;
        }
        if (m_phase == "opening") {
            if (m_browseHidden && m_cameraEntered && m_ports.presentationReady() && m_layoutReady)
                showMenu();
            return;
        }
        if (m_phase != "returning" && m_phase != "selecting")
            return;
        if (m_phase == "returning" && !m_wantsDetail && m_ports.archiveInteractive
            && m_ports.archiveInteractive()) {
            m_phase = "archive";
            if (m_pending)
                commitSelection(false);
            m_browse.show(m_reduced);
            emit focusRequested(false);
            return;
        }
        if (!m_ports.archiveReady())
            return;
        if (m_pending) {
            commitSelection(false);
            m_phase = "selecting";
            return;
        }
        if (m_wantsDetail) {
            beginOpen();
        } else {
            m_phase = "archive";
            m_browse.show(m_reduced);
            emit focusRequested(false);
        }
    });
}
void PresentationController::beginOpen()
{
    const auto revision = ++m_revision;
    m_phase = "opening";
    m_browseHidden = m_cameraEntered = false;
    prepareMenu();
    m_browse.hide(m_reduced, [this, revision] {
        transact([&] {
            if (revision != m_revision)
                return;
            m_browseHidden = true;
            if (m_pending) {
                m_phase = "selecting";
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
    if (m_phase == "hiding" || m_phase == "returning" || m_phase == "switch-hiding")
        return;
    const auto revision = ++m_revision;
    m_phase = "hiding";
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
    m_phase = "switch-hiding";
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
            m_phase = "switching";
            if (m_pending) {
                commitSelection(true);
                prepareMenu();
            }
        });
    });
}
void PresentationController::commitSelection(bool detail)
{
    const auto request = *m_pending;
    m_pending.reset();
    if (detail)
        m_ports.switchDetail(request);
    else
        m_ports.select(request);
}
void PresentationController::returnToArchive()
{
    m_phase = "returning";
    m_ports.returnCamera();
}
void PresentationController::prepareMenu()
{
    m_detail.reset(false);
    m_layoutReady = false;
    ++m_layoutRevision;
    emit prepared();
}
void PresentationController::showMenu()
{
    m_phase = "detail";
    m_detail.show(m_reduced);
    emit focusRequested(true);
}
void PresentationController::reset()
{
    transact([&] {
        ++m_revision;
        ++m_layoutRevision;
        m_pending.reset();
        m_wantsDetail = m_browseHidden = m_cameraEntered = m_layoutReady = false;
        m_phase = "archive";
        m_detail.reset(false);
        m_browse.reset(true);
        emit prepared();
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
        }
    });
}
}
