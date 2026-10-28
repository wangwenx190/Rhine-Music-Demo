// Bounds external Review capture inputs before integer conversion or GPU allocation.
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <array>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <optional>

namespace yalahoo::review {
inline constexpr qint64 MaximumMaterialInputBytes = 4 * 1024 * 1024;
inline constexpr int SourceMaterialCaseCount = 90;
inline constexpr int MaximumLightingCases = 256;
inline constexpr double MaximumMaterialPixels = 512.0 * 1024 * 1024;
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
inline bool materialNumber(QJsonValue value, double minimum, double maximum)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= minimum
            && value.toDouble() <= maximum;
}
inline bool materialVector(QJsonValue value, int count, double limit)
{
    if (!value.isArray() || value.toArray().size() != count)
        return false;
    for (const auto &component : value.toArray())
        if (!materialNumber(component, -limit, limit))
            return false;
    return true;
}
inline int materialSurface(QJsonValue value)
{
    using namespace Qt::StringLiterals;
    const std::array names{ "Frosted_Polymer"_L1, "Ivory_Edges"_L1, "Optical_Diffuser"_L1 };
    for (int i = 0; i < int(names.size()); ++i)
        if (value.isString() && value.toString() == names[i])
            return i;
    return -1;
}
inline bool validMaterialReview(const QJsonObject &source, double dpr)
{
    using namespace Qt::StringLiterals;
    const auto lighting = source.value(u"lighting"_s);
    if (!lighting.isUndefined() && !lighting.isBool())
        return false;
    const auto casesValue = source.value(u"cases"_s);
    if (!casesValue.isArray())
        return false;
    const auto cases = casesValue.toArray();
    if (cases.isEmpty() || cases.size() > MaximumLightingCases
        || (!lighting.toBool() && cases.size() != SourceMaterialCaseCount))
        return false;
    double pixels = 0;
    for (const auto &value : cases) {
        if (!value.isObject())
            return false;
        const auto state = value.toObject();
        const auto size = materialReviewSize(state);
        if (!size || !materialReviewTargetSupported(*size, dpr, MaximumMaterialDimension))
            return false;
        pixels += std::round(size->width() * dpr) * std::round(size->height() * dpr);
        if (pixels > MaximumMaterialPixels || !materialVector(state[u"position"_s], 3, 1e6)
            || !state[u"day"_s].isBool() || !materialNumber(state[u"clarity"_s], 0, 1))
            return false;
        if (!state[u"origin"_s].isUndefined() && !materialVector(state[u"origin"_s], 3, 1e6))
            return false;
        for (const auto key : { "detail"_L1, "paperLight"_L1 })
            if (!state[key].isUndefined() && !materialNumber(state[key], 0, 2))
                return false;
        for (const auto key : { "materialBaseline"_L1, "paperProbe"_L1 })
            if (!state[key].isUndefined() && !state[key].isBool())
                return false;
        if (!QStringList{ u"square"_s, u"portrait"_s, u"landscape"_s }.contains(
                    state[u"cover"_s].toString())
            || !QStringList{ u"all"_s, u"paper"_s, u"Frosted_Polymer"_s, u"Ivory_Edges"_s,
                             u"Optical_Diffuser"_s }
                        .contains(state[u"layer"_s].toString()))
            return false;
        if (lighting.toBool()
            && !QStringList{ u"baseline"_s, u"hybrid"_s, u"area"_s, u"guided"_s }.contains(
                    state[u"mode"_s].toString()))
            return false;
        const auto parameters = state[u"parameters"_s];
        if (!parameters.isArray() || parameters.toArray().size() != 3)
            return false;
        std::array<bool, 3> seen{ };
        for (const auto &entry : parameters.toArray()) {
            if (!entry.isObject())
                return false;
            const auto parameter = entry.toObject();
            const int surface = materialSurface(parameter[u"surface"_s]);
            if (surface < 0 || seen[surface])
                return false;
            seen[surface] = true;
            for (const auto key : { "transmission"_L1, "roughness"_L1 })
                if (!materialNumber(parameter[key], 0, 1))
                    return false;
            for (const auto key : { "thickness"_L1, "attenuationDistance"_L1 })
                if (!materialNumber(parameter[key], 0, 1e6))
                    return false;
        }
        const auto projections = state[u"projections"_s];
        if (!projections.isArray() || projections.toArray().size() != 5)
            return false;
        for (const auto &entry : projections.toArray()) {
            const auto point = entry.toObject();
            if (!entry.isObject() || !materialVector(point[u"world"_s], 3, 1e6)
                || !materialVector(point[u"pixel"_s], 2, 1e9))
                return false;
        }
    }
    return true;
}

}
