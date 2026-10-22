#include "ArchiveField.h"
#include "OpeningTimeline.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
constexpr auto IdleReleaseRate = 4;
constexpr auto MaximumSelectionPulses = 6;
constexpr auto MaximumRippleDisplacement = .24;
constexpr auto RippleLaneDistanceScale = 2.2;
constexpr auto PulseGainFollowRate = 8;
constexpr auto ScanBlendDecayRate = 3;
constexpr auto PulseLifetimeSeconds = 3.2;
constexpr auto IdleDelaySeconds = 2.5;
constexpr auto IdleAttackRate = .8;
}
void ArchiveField::interact()
{
    m_lastInteraction = m_time;
}
void ArchiveField::pulse(ArchiveCell cell)
{
    m_pulses.append({ cell, m_time, { } });
    if (m_pulses.size() > MaximumSelectionPulses)
        m_pulses.removeFirst();
}
void ArchiveField::clearPulses()
{
    m_pulses.clear();
}
void ArchiveField::beginOpening()
{
    m_scanBlend = m_idleGain = 0;
    clearPulses();
    interact();
}
void ArchiveField::rebase(ArchiveCell shift)
{
    for (auto &pulse : m_pulses) {
        pulse.cell.lane -= shift.lane;
        pulse.cell.row -= shift.row;
    }
}
void ArchiveField::advance(double dt, bool reduced, bool idleEligible, bool muted)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    m_time += dt;
    m_scanTime += dt;
    m_scanBlend *= std::exp(-dt * ScanBlendDecayRate);
    m_pulses.removeIf([&](const Pulse &p) {
        return m_time - p.time >= PulseLifetimeSeconds;
    });
    for (auto &pulse : m_pulses)
        pulse.wave = prepareMusicSelectionWave(m_time - pulse.time);
    const bool idle = !reduced && idleEligible && m_time - m_lastInteraction > IdleDelaySeconds;
    m_idleGain += ((idle ? 1 : 0) - m_idleGain)
            * (1 - std::exp(-dt * (idle ? IdleAttackRate : IdleReleaseRate)));
    m_pulseGain += ((muted ? 0 : 1) - m_pulseGain) * (1 - std::exp(-dt * PulseGainFollowRate));
    if (reduced)
        m_idleGain = 0;
}
double ArchiveField::realisticField(double row, const Column &column) const
{
    const double lane = column.lane, activity = column.activity;
    double height =
            column.reduced ? 0 : idleWave(row, lane, m_time) * (1 + (m_idleGain - 1) * activity);
    if (!column.reduced && activity > 0 && m_pulseGain != 0) {
        double ripple = 0;
        for (const auto &pulse : m_pulses)
            if (pulse.cell.lane == lane)
                ripple += pulse.wave.sample(std::abs(row - pulse.cell.row));
        height += std::clamp(ripple, -MaximumRippleDisplacement, MaximumRippleDisplacement)
                * m_pulseGain * activity;
    }
    return height
            + settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds) * activity;
}
double ArchiveField::sample(double row, const Column &column, const Context &context) const
{
    const double lane = column.lane;
    if (context.openingTime) {
        const double intro = column.intro;
        const double opening = intro == 1
                ? 0
                : cinematicField(row - context.selected.row + sceneGeometry::FirstAlbumRow,
                                 lane - context.selected.lane + 2, *context.openingTime);
        if (intro == 0)
            return opening;
        const double resting = settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds)
                * (context.realistic ? column.activity : column.strength);
        return opening * (1 - intro) + resting * intro;
    }
    if (context.realistic)
        return realisticField(row, column);
    double height = 0;
    if (m_scanBlend != 0)
        height += archiveWave(row + context.origin.row, lane + context.origin.lane, m_scanTime)
                * m_scanBlend;
    if (m_idleGain != 0)
        height +=
                idleWave(row + context.origin.row, lane + context.origin.lane, m_time) * m_idleGain;
    if (!column.reduced && m_pulseGain != 0) {
        double ripple = 0;
        for (const auto &pulse : m_pulses)
            ripple += pulse.wave.sample(std::hypot(
                    row - pulse.cell.row, (lane - pulse.cell.lane) * RippleLaneDistanceScale));
        height += std::clamp(ripple, -MaximumRippleDisplacement, MaximumRippleDisplacement)
                * m_pulseGain;
    }
    return height
            + settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds)
            * column.strength;
}
}
