#pragma once
#include "ArchivePool.h"
#include "QuinticTrack.h"

namespace yalahoo {
// Frozen MusicRealColumns semantics, sampled lazily at the shared motion time.
class RealColumns
{
public:
    void reset(const QList<int> &counts, ArchiveCell selected,
               std::optional<double> displayedRow = std::nullopt);
    void select(ArchiveCell cell, bool guided = false, bool detail = false);
    void update(double dt, bool reduced);
    double row(int lane, double overview = 0) const;
    double activity(int lane) const;
    Kinematics rowState(int lane) const;
    Kinematics activityState(int lane) const;
    bool settled(int lane) const;

private:
    struct Column
    {
        int count = 0;
        double midpoint = 12;
        ColumnScalar row, activity;
        double sampledAt = 0;
        quint64 reducedEpoch = 0;
    };
    Column *sample(int lane) const;
    mutable QList<Column> m_columns;
    ArchiveCell m_selected{ -1, 12 };
    double m_time = 0;
    quint64 m_reducedEpoch = 0;
};
}
