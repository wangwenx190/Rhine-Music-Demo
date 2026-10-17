#include "OpeningController.h"

namespace yalahoo {
OpeningController::OpeningController(MotionController *motion, SceneAdapter *scene,
                                     SelectionController *selection, QObject *parent)
    : QObject(parent), m_motion(motion), m_scene(scene), m_selection(selection)
{
    connect(motion->clock(), &FrameClock::frame, this, [this](double dt, double) {
        advance(dt);
    });
    connect(motion, &MotionController::reducedMotionChanged, this, [this] {
        if (!m_motion->reducedMotion())
            return;
        if (m_phase == Phase::Film)
            finishFilm(true);
        else if (m_phase == Phase::Preparing)
            m_skipPending = true;
        else if (m_phase == Phase::Reveal)
            complete();
    });
}
void OpeningController::replay()
{
    const auto revision = ++m_revision;
    m_time = m_reveal = 0;
    m_skipPending = m_motion->reducedMotion();
    m_phase = Phase::Preparing;
    m_selection->cancelDrag();
    m_scene->setPointer(0, 0);
    // Warm the actual settled production pipeline before starting the fixed film clock.
    m_selection->setOpeningTime(27.12);
    if (revision != m_revision)
        return;
    m_scene->setOpeningTime(27.12);
    if (revision != m_revision)
        return;
    Q_EMIT changed();
    if (revision == m_revision && preparing())
        Q_EMIT started();
}
void OpeningController::scenePrepared()
{
    if (!preparing())
        return;
    const auto revision = m_revision;
    m_phase = Phase::Film;
    m_motion->clock()->startOpening();
    if (revision != m_revision || !film())
        return;
    sample(0);
    if (revision != m_revision || !film())
        return;
    if (m_skipPending)
        finishFilm(true);
    else
        Q_EMIT changed();
}
void OpeningController::skip()
{
    if (preparing())
        m_skipPending = true;
    else if (film())
        finishFilm(true);
}
void OpeningController::sample(double dt)
{
    const auto revision = m_revision;
    const auto phase = m_phase;
    const double shot = std::min(27.12, openingSceneTime(m_time));
    m_selection->setOpeningTime(shot);
    if (revision == m_revision && phase == m_phase)
        m_scene->setOpeningTime(shot, dt);
}
void OpeningController::advance(double dt)
{
    if (film()) {
        const auto revision = m_revision;
        m_time = m_motion->clock()->openingTime();
        sample(dt);
        if (revision != m_revision || !film())
            return;
        if (m_time == 11.6)
            finishFilm(false);
        else
            Q_EMIT changed();
    } else if (m_phase == Phase::Reveal) {
        m_reveal = std::min(.72, m_reveal + dt);
        if (.72 - m_reveal < 1e-9)
            complete();
        else
            Q_EMIT changed();
    }
}
void OpeningController::finishFilm(bool skipped)
{
    if (!film())
        return;
    const auto revision = m_revision;
    m_time = 11.6;
    if (skipped)
        sample(0);
    if (revision != m_revision || !film())
        return;
    m_phase = Phase::Reveal;
    m_reveal = 0;
    m_motion->clock()->finishOpening();
    if (revision != m_revision || m_phase != Phase::Reveal)
        return;
    m_selection->setOpeningTime(std::nullopt);
    if (revision != m_revision || m_phase != Phase::Reveal)
        return;
    m_scene->setOpeningTime(std::nullopt);
    if (revision != m_revision || m_phase != Phase::Reveal)
        return;
    Q_EMIT handedOff();
    if (revision != m_revision || m_phase != Phase::Reveal)
        return;
    if (m_motion->reducedMotion())
        complete();
    else
        Q_EMIT changed();
}
void OpeningController::complete()
{
    const auto revision = m_revision;
    m_phase = Phase::Done;
    m_reveal = .72;
    Q_EMIT changed();
    if (revision == m_revision && m_phase == Phase::Done)
        Q_EMIT completed();
}
}
