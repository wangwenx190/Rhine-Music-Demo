// Converts runtime configuration numbers without accepting switches, text or failed conversions.
// Range policy stays with each controller; missing and nonfinite values never become zero.

#pragma once
#include <QVariant>
#include <cmath>
#include <optional>

namespace yalahoo {
inline std::optional<double> finiteNumber(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QString || value.metaType().id() == QMetaType::Bool)
        return std::nullopt;
    bool converted = false;
    const double number = value.toDouble(&converted);
    if (!converted || !std::isfinite(number))
        return std::nullopt;
    return number;
}
}
