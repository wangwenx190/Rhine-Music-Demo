// Validates explicit public metadata query tracks before network work is accepted.
// Missing optional fields retain defaults; supplied fields keep strict types and finite bounds.
#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace yalahoo::probe {
using namespace Qt::StringLiterals;
inline void requireInput(bool condition)
{
    if (!condition)
        throw std::runtime_error("Invalid probe input structure or numeric field");
}
inline double number(QJsonValue value, double minimum = -std::numeric_limits<double>::max(),
                     double maximum = std::numeric_limits<double>::max())
{
    requireInput(value.isDouble());
    const double result = value.toDouble();
    requireInput(std::isfinite(result) && result >= minimum && result <= maximum);
    return result;
}
inline int integer(QJsonValue value, int minimum = 0)
{
    const double result = number(value, minimum, std::numeric_limits<int>::max());
    requireInput(std::trunc(result) == result);
    return int(result);
}
inline QJsonObject object(QJsonValue value)
{
    requireInput(value.isObject());
    return value.toObject();
}
inline void validateTracks(const QJsonDocument &document)
{
    constexpr int MaximumQueryTracks = 2000;
    requireInput(document.isArray() && document.array().size() <= MaximumQueryTracks);
    for (const auto &value : document.array()) {
        const auto track = object(value);
        requireInput(track[u"title"_s].isString() && !track[u"title"_s].toString().isEmpty());
        integer(track[u"number"_s], 1);
        if (track.contains(u"disc"_s))
            integer(track[u"disc"_s], 1);
        if (track.contains(u"duration"_s))
            number(track[u"duration"_s], 0);
    }
}
}
