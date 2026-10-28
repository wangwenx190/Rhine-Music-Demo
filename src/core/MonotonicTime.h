// Provides one steady-clock epoch for input, submission and diagnostic timestamps.
// These timestamps do not imply GPU completion or physical presentation.

#pragma once
#include "Units.h"
#include <QtTypes>
#include <chrono>

namespace yalahoo {
inline qint64 monotonicNanoseconds()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}
inline double monotonicMilliseconds()
{
    return double(monotonicNanoseconds()) / units::NanosecondsPerMillisecond;
}
}
