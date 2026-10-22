#include "ScalarSpring.h"

namespace yalahoo {
void ScalarSpring::observe(double target, double rate)
{
    acceleration = rate * rate * (target - value) - 2 * rate * velocity;
}
// For critical damping, displacement is (delta + impulse*t)*exp(-rate*t).
// Advance position and velocity from the same initial state, then recompute acceleration.
void ScalarSpring::update(double target, double rate, double dt)
{
    const auto next = criticalSpringStep(value, velocity, target, rate, dt);
    value = next.value;
    velocity = next.velocity;
    observe(target, rate);
}
}
