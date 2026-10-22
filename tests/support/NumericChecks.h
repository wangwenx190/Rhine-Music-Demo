// Reject nonfinite sample differences before a maximum-error reduction can hide them.
// Only called inside QtTest slots or standalone tools with exception boundaries.
#pragma once
#include "Require.h"
#include <cmath>

namespace yalahoo::test {
inline double errorMagnitude(double difference)
{
    require(std::isfinite(difference), "Nonfinite numerical comparison");
    return std::abs(difference);
}
}
