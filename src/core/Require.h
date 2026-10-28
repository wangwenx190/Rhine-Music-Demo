// Stops fallible resource and verification operations at a catchable exception boundary.
// Framework callbacks must handle failures before returning into Qt.

#pragma once
#include <stdexcept>

namespace yalahoo {
inline void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}
