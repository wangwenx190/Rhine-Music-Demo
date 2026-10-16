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
    ++m_revision;
    m_time = m_reveal = 0;
    m_skipPending = m_motion->reducedMotion();
    m_phase = Phase::Preparing;
    m_selection->cancelDrag();
    m_scene->setPointer(0, 0);
    // Warm the actual settled production pipeline before starting the fixed film clock.
    m_selection->setOpeningTime(27.12);
    m_scene->setOpeningTime(27.12);
    emit changed();
    emit started();
}
void OpeningController::scenePrepared()
{
    if (!preparing())
        return;
    m_phase = Phase::Film;
    m_motion->clock()->startOpening();
    sample(0);
    if (m_skipPending)
        finishFilm(true);
    else
        emit changed();
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
    const double shot = std::min(27.12, openingSceneTime(m_time));
    m_selection->setOpeningTime(shot);
    m_scene->setOpeningTime(shot, dt);
}
void OpeningController::advance(double dt)
{
    if (film()) {
        m_time = m_motion->clock()->openingTime();
        sample(dt);
        if (m_time == 11.6)
            finishFilm(false);
        else
            emit changed();
    } else if (m_phase == Phase::Reveal) {
        m_reveal = std::min(.72, m_reveal + dt);
        if (.72 - m_reveal < 1e-9)
            complete();
        else
            emit changed();
    }
}
void OpeningController::finishFilm(bool skipped)
{
    if (!film())
        return;
    m_time = 11.6;
    if (skipped)
        sample(0);
    m_motion->clock()->finishOpening();
    m_selection->setOpeningTime(std::nullopt);
    m_scene->setOpeningTime(std::nullopt);
    m_phase = Phase::Reveal;
    m_reveal = 0;
    emit handedOff();
    if (m_motion->reducedMotion())
        complete();
    else
        emit changed();
}
void OpeningController::complete()
{
    m_phase = Phase::Done;
    m_reveal = .72;
    emit changed();
    emit completed();
}
}
