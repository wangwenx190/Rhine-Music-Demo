// Owns bounded selection ripples and idle-wave envelopes. The selection coordinator supplies
// column state and coordinates; sampling has no side effects or QObject dependencies.

#pragma once
#include "ArchivePool.h"
#include "MotionFunctions.h"
#include "SceneGeometry.h"

namespace yalahoo {
class ArchiveField final
{
public:
    struct Column final
    {
        double lane = 0, center = 0, strength = 1, activity = 0, intro = 0;
        bool reduced = false;
    };
    struct Context final
    {
        bool realistic = false;
        std::optional<double> openingTime;
        ArchiveCell selected, origin;
    };
    void interact();
    void pulse(ArchiveCell cell);
    void clearPulses();
    void beginOpening();
    void rebase(ArchiveCell shift);
    void advance(double dt, bool reduced, bool idleEligible, bool muted);
    double sample(double row, const Column &column, const Context &context) const;
    double pulseGain() const
    {
        return m_pulseGain;
    }

private:
    struct Pulse final
    {
        ArchiveCell cell;
        double time = 0;
        SelectionWave wave;
    };
    double realisticField(double row, const Column &column) const;
    QVector<Pulse> m_pulses;
    double m_time = 0;
    double m_lastInteraction = 0, m_idleGain = 0, m_pulseGain = 1;
};
}
