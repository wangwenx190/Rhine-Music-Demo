// Coordinates archive travel across synchronized scalar tracks. Retargeting preserves displayed
// derivatives and one frame clock drives all updates.

#pragma once
#include "FrameClock.h"
#include "QuinticTrack.h"
#include <array>

namespace yalahoo {
// rail, column, shoulder, lane in frozen ARCHIVE_TRACK_KEYS order.
// The four synchronized tracks retain the source rail/column/shoulder/lane ABI.
enum ArchiveAxis {
    RailAxis,
    ColumnAxis,
    ShoulderAxis,
    LaneAxis,
    ArchiveAxisCount
};
using ArchiveValues = std::array<double, ArchiveAxisCount>;
using ArchiveSprings = std::array<ScalarSpring, ArchiveAxisCount>;
class ArchiveMotion final
{
public:
    bool active() const
    {
        return m_active;
    }
    double progress() const
    {
        return m_tracks[0].progress();
    }
    double duration() const
    {
        return m_tracks[0].duration();
    }
    std::array<Kinematics, ArchiveAxisCount> state() const;
    void observe(const ArchiveSprings &springs, const ArchiveValues &targets,
                 const ArchiveValues &rates);
    bool retarget(const ArchiveSprings &springs, const ArchiveValues &targets);
    void update(ArchiveSprings &springs, double dt, bool reduced = false);
    void translate(const ArchiveValues &offset);
    void finish(ArchiveSprings &springs);
    void reset();

private:
    std::array<QuinticTrack, ArchiveAxisCount> m_tracks;
    ArchiveValues m_targets{ };
    bool m_hasTarget = false;
    bool m_active = false;
};

class MotionController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MotionController)
    Q_PROPERTY(yalahoo::FrameClock* clock READ clock CONSTANT FINAL)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged FINAL)
    Q_PROPERTY(bool moving READ moving NOTIFY stateChanged FINAL)
    Q_PROPERTY(double rail READ rail NOTIFY stateChanged FINAL)
    Q_PROPERTY(double column READ column NOTIFY stateChanged FINAL)
    Q_PROPERTY(quint64 revision READ revision NOTIFY stateChanged FINAL)
public:
    explicit MotionController(QObject *parent = nullptr);
    FrameClock *clock()
    {
        return &m_clock;
    }
    bool reducedMotion() const
    {
        return m_reduced;
    }
    void setReducedMotion(bool reduced);
    bool moving() const
    {
        return m_archive.active();
    }
    double rail() const
    {
        return m_springs[RailAxis].value;
    }
    double column() const
    {
        return m_springs[ColumnAxis].value;
    }
    quint64 revision() const
    {
        return m_revision;
    }
    const ArchiveSprings &springs() const
    {
        return m_springs;
    }
    const ArchiveMotion &archive() const
    {
        return m_archive;
    }
    void observe(const ArchiveSprings &springs, const ArchiveValues &targets,
                 const ArchiveValues &rates);
    bool retarget(const ArchiveValues &targets);
    void translate(const ArchiveValues &offset);
    void cancelTravel();
    Q_INVOKABLE void travel(double rail, double column, double shoulder, double lane);
    Q_INVOKABLE void finish();
Q_SIGNALS:
    void stateChanged();
    void reducedMotionChanged();
    void completed(quint64 revision);

private:
    void step(double dt);
    FrameClock m_clock;
    ArchiveMotion m_archive;
    ArchiveSprings m_springs{ };
    bool m_reduced = false;
    quint64 m_revision = 0;
};
}
