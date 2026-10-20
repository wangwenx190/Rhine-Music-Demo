// Coordinates extraction, inspection and return of the selected case. Interrupted transitions
// preserve displayed motion and return copies own their remaining trajectory.

#pragma once
#include "QuinticTrack.h"

namespace yalahoo {
// Frozen music-camera.ts placement and presentation gate. The caller supplies
// actual camera/rail/lift readiness after advancing the same shared frame.
class DetailMotion final
{
public:
    enum class Phase {
        Archive,
        Placing,
        Presented,
        ReturningCenter,
        ReturningArray
    };
    DetailMotion();
    void request(bool detail);
    void selectionChanged();
    void advance(double dt, bool reduced, bool aligned);
    void updateReadiness(double dt, bool cameraSettled, bool liftSettled, bool reduced);
    Phase phase() const
    {
        return m_phase;
    }
    bool placed() const;
    bool holdsDetail() const;
    bool settled() const
    {
        return !m_placement.active();
    }
    double value() const;
    const Kinematics &state() const
    {
        return m_placement.state();
    }
    void reset();

private:
    QuinticTrack m_placement;
    Phase m_phase = Phase::Archive;
    double m_settledFor = 0;
};
}
