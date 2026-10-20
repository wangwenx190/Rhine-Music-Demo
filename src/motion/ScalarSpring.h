// Evaluates the exact critically damped spring solution. Position and velocity advance
// analytically, avoiding frame-rate-dependent Euler integration.

#pragma once

namespace yalahoo {
struct Kinematics
{
    double value = 0;
    double velocity = 0;
    double acceleration = 0;
};

// Exact solution of the critically damped spring in frozen motion.ts.
struct ScalarSpring final : Kinematics
{
    void observe(double target, double rate);
    void update(double target, double rate, double dt);
    void translate(double offset)
    {
        value += offset;
    }
    void finish(double target)
    {
        value = target;
        velocity = acceleration = 0;
    }
};
}
