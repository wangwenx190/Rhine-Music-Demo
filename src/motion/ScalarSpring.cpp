#include "ScalarSpring.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
void ScalarSpring::observe(double target, double rate)
{
    acceleration = rate * rate * (target - value) - 2 * rate * velocity;
}
void ScalarSpring::update(double target, double rate, double dt)
{
    dt = std::max(0.0, dt);
    const double delta = value - target;
    const double impulse = velocity + rate * delta;
    const double decay = std::exp(-rate * dt);
    value = target + (delta + impulse * dt) * decay;
    velocity = (velocity - rate * impulse * dt) * decay;
    observe(target, rate);
}
}
