// Bounds external Review capture inputs before integer conversion or GPU allocation.
#pragma once
#include <QJsonObject>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <optional>

namespace yalahoo::review {
inline constexpr int DefaultMaterialWidth = 960;
inline constexpr int DefaultMaterialHeight = 720;
// Review captures permit up to a 4K square (64 MiB RGBA8 readback).
inline constexpr int MaximumMaterialDimension = 4096;
inline std::optional<QSize> materialReviewSize(const QJsonObject &state)
{
    const auto dimension = [&](QLatin1StringView key, int fallback) -> std::optional<int> {
        const auto value = state.value(key);
        if (value.isUndefined())
            return fallback;
        if (!value.isDouble())
            return std::nullopt;
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::trunc(number) != number || number < 1
            || number > MaximumMaterialDimension)
            return std::nullopt;
        return int(number);
    };
    using namespace Qt::StringLiterals;
    const auto width = dimension("width"_L1, DefaultMaterialWidth);
    const auto height = dimension("height"_L1, DefaultMaterialHeight);
    if (!width || !height)
        return std::nullopt;
    return QSize(*width, *height);
}
inline bool materialReviewTargetSupported(QSize logicalSize, double dpr, int deviceLimit)
{
    if (logicalSize.isEmpty() || !std::isfinite(dpr) || dpr <= 0 || deviceLimit <= 0)
        return false;
    const double limit = std::min(deviceLimit, MaximumMaterialDimension);
    // Check in floating point before QSize scaling can narrow or overflow.
    const double width = logicalSize.width() * dpr, height = logicalSize.height() * dpr;
    return std::isfinite(width) && std::isfinite(height) && std::round(width) >= 1
            && std::round(height) >= 1 && std::round(width) <= limit && std::round(height) <= limit;
}
}
