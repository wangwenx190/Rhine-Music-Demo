// Converts runtime configuration numbers without accepting switches, text or failed conversions.
// Range policy stays with each controller; missing and nonfinite values never become zero.

#pragma once
#include <QVariant>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace yalahoo {
inline std::optional<double> finiteNumber(const QVariant &value)
{
    // QVariant also converts byte strings and custom types; only arithmetic storage is input.
    static constexpr std::array numericTypes{
        QMetaType::Char,   QMetaType::SChar,    QMetaType::UChar,     QMetaType::Short,
        QMetaType::UShort, QMetaType::Int,      QMetaType::UInt,      QMetaType::Long,
        QMetaType::ULong,  QMetaType::LongLong, QMetaType::ULongLong, QMetaType::Float,
        QMetaType::Double
    };
    if (std::ranges::find(numericTypes, value.metaType().id()) == numericTypes.end())
        return std::nullopt;
    bool converted = false;
    const double number = value.toDouble(&converted);
    if (!converted || !std::isfinite(number))
        return std::nullopt;
    return number;
}
}
