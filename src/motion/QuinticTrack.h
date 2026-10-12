#pragma once
#include "ScalarSpring.h"
#include <array>

namespace yalahoo {
double archiveTravelDuration(double lanes, double rows);

// Rest-to-rest endpoint, preserving the displayed x/v/a on interruption.
class QuinticTrack
{
public:
    const Kinematics &state() const
    {
        return m_state;
    }
    double target() const
    {
        return m_target;
    }
    double elapsed() const
    {
        return m_elapsed;
    }
    double duration() const
    {
        return m_duration;
    }
    double progress() const
    {
        return m_duration ? m_elapsed / m_duration : 1;
    }
    bool active() const
    {
        return m_active;
    }
    void observe(Kinematics state);
    bool retarget(double target, double duration);
    void update(double dt, bool reduced = false);
    void translate(double offset);
    void finish();
    void reset();

private:
    Kinematics m_state;
    std::array<double, 6> m_coefficients{ };
    double m_target = 0;
    double m_elapsed = 0;
    double m_duration = 0;
    bool m_active = false;
    bool m_hasTarget = false;
};

// Scalar portion of music-real-columns.ts. Column identity/layout remains R13.
class ColumnScalar
{
public:
    explicit ColumnScalar(double value = 0) : m_target(value)
    {
        m_spring.value = value;
    }
    const Kinematics &state() const
    {
        return m_spring;
    }
    double target() const
    {
        return m_target;
    }
    bool guided() const
    {
        return m_track.active();
    }
    bool settled() const;
    bool retarget(double target, bool guided = false, double duration = 1.4, double rate = 3.7);
    void update(double dt, bool reduced = false);
    void translate(double offset);
    void finish();

private:
    ScalarSpring m_spring;
    QuinticTrack m_track;
    double m_target;
    double m_rate = 3.7;
};
}
