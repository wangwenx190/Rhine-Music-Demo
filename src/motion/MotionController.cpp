#include "MotionController.h"

namespace yalahoo {
std::array<Kinematics, 4> ArchiveMotion::state() const
{
    std::array<Kinematics, 4> result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = m_tracks[i].state();
    return result;
}
void ArchiveMotion::observe(const ArchiveSprings &springs, const ArchiveValues &targets, const ArchiveValues &rates)
{
    if (m_active) return;
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        auto spring = springs[i];
        spring.observe(targets[i], rates[i]);
        m_tracks[i].observe(spring);
    }
}
bool ArchiveMotion::retarget(const ArchiveSprings &springs, const ArchiveValues &targets)
{
    if (m_hasTarget && m_targets == targets) return false;
    const double duration = archiveTravelDuration((targets[1]-springs[1].value)/5.2,
        (targets[0]-springs[0].value)/.62);
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        auto state = m_tracks[i].state();
        state.value = springs[i].value;
        state.velocity = springs[i].velocity;
        m_tracks[i].reset();
        m_tracks[i].observe(state);
        m_tracks[i].retarget(targets[i], duration);
    }
    m_targets = targets;
    m_hasTarget = m_active = true;
    return true;
}
void ArchiveMotion::update(ArchiveSprings &springs, double dt, bool reduced)
{
    if (!m_active) return;
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        m_tracks[i].update(dt, reduced);
        static_cast<Kinematics &>(springs[i]) = m_tracks[i].state();
    }
    m_active = m_tracks[0].active();
}
void ArchiveMotion::translate(const ArchiveValues &offset)
{
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        m_tracks[i].translate(offset[i]);
        if (m_hasTarget) m_targets[i] += offset[i];
    }
}
void ArchiveMotion::finish(ArchiveSprings &springs) { update(springs, 0, true); }
void ArchiveMotion::reset()
{
    for (auto &track : m_tracks) track.reset();
    m_hasTarget = m_active = false;
}
MotionController::MotionController(QObject *parent) : QObject(parent)
{
    connect(&m_clock, &FrameClock::frame, this, [this](double, double motionDt) { step(motionDt); });
}
void MotionController::observe(const ArchiveSprings &springs, const ArchiveValues &targets, const ArchiveValues &rates)
{
    if (moving()) return;
    m_springs = springs;
    m_archive.observe(springs, targets, rates);
    emit stateChanged();
}
bool MotionController::retarget(const ArchiveValues &targets)
{
    if (!m_archive.retarget(m_springs, targets)) return false;
    ++m_revision;
    emit stateChanged();
    if (m_reduced) finish();
    return true;
}
void MotionController::travel(double rail, double column, double shoulder, double lane) { retarget({rail,column,shoulder,lane}); }
void MotionController::translate(const ArchiveValues &offset)
{
    m_archive.translate(offset);
    for (size_t i = 0; i < m_springs.size(); ++i) m_springs[i].translate(offset[i]);
    emit stateChanged();
}
void MotionController::setReducedMotion(bool reduced)
{
    if (m_reduced == reduced) return;
    m_reduced = reduced;
    emit reducedMotionChanged();
    if (reduced) { finish(); m_clock.finishOpening(); }
}
void MotionController::step(double dt)
{
    if (!moving()) return;
    m_archive.update(m_springs, dt, m_reduced);
    const bool finished = !moving();
    const auto revision = m_revision;
    emit stateChanged();
    if (finished) emit completed(revision);
}
void MotionController::finish()
{
    if (!moving()) return;
    m_archive.finish(m_springs);
    const auto revision = m_revision;
    emit stateChanged();
    emit completed(revision);
}
}
