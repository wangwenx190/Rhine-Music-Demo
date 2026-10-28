// Evaluates the exact critically damped spring solution. Position and velocity advance
// analytically, avoiding frame-rate-dependent Euler integration.

#pragma once
#include <algorithm>
#include <cmath>

namespace yalahoo {
template <typename T>
struct CriticalSpringState final
{
    T value, velocity;
};
// Scalar and vector consumers share the exact solution; caller-specific limits stay outside it.
template <typename T>
CriticalSpringState<T> criticalSpringStep(T value, T velocity, T target, double rate, double dt)
{
    dt = std::max(0.0, dt);
    const auto delta = value - target;
    const auto impulse = velocity + delta * rate;
    const double decay = std::exp(-rate * dt);
    return { target + (delta + impulse * dt) * decay, (velocity - impulse * (rate * dt)) * decay };
}

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
