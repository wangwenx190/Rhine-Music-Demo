// Models finite library columns and their scalar navigation state. Stable album identity
// survives layout changes without introducing a second frame source.

#pragma once
#include "ArchivePool.h"
#include "QuinticTrack.h"

namespace yalahoo {
namespace realColumnGeometry {
// Canonical source row coordinates begin at the first album offset.
inline constexpr auto FirstAlbumRow = 12;
}

// Frozen MusicRealColumns semantics, sampled lazily at the shared motion time.
class RealColumns final
{
    Q_DISABLE_COPY_MOVE(RealColumns)
public:
    RealColumns() = default;
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
    struct Column final
    {
        int count = 0;
        double midpoint = realColumnGeometry::FirstAlbumRow;
        ColumnScalar row, activity;
        double sampledAt = 0;
        quint64 reducedEpoch = 0;
    };
    Column *sample(int lane) const;
    mutable QList<Column> m_columns;
    ArchiveCell m_selected{ -1, realColumnGeometry::FirstAlbumRow };
    double m_time = 0;
    quint64 m_reducedEpoch = 0;
};
}
