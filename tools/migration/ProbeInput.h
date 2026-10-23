// Validates source-probe JSON before any rendering or network requests. Missing optional
// fields keep their documented defaults; supplied fields must retain their original types.
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
inline QJsonArray array(QJsonValue value)
{
    requireInput(value.isArray() && !value.toArray().isEmpty());
    return value.toArray();
}
inline void boolean(QJsonValue value)
{
    requireInput(value.isBool());
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
inline void validateRulerState(QJsonValue value)
{
    const auto state = object(value);
    for (const auto key : { "width"_L1, "scroll"_L1, "scrollVelocity"_L1, "selected"_L1 })
        number(state[key]);
    requireInput(state[u"width"_s].toDouble() >= 0);
    const auto slots = array(state[u"slots"_s]);
    requireInput(slots.size() == 16);
    for (const auto &value : slots) {
        const auto slot = object(value);
        for (const auto key : { "x"_L1, "ordinal"_L1, "height"_L1 })
            number(slot[key]);
        number(slot[u"alpha"_s], 0, 1);
        boolean(slot[u"enabled"_s]);
        boolean(slot[u"active"_s]);
    }
}
inline void validateRuler(const QJsonDocument &document)
{
    requireInput(document.isObject());
    const auto source = document.object();
    for (const auto &value : array(source[u"captures"_s])) {
        const auto capture = object(value);
        requireInput(number(capture[u"dpr"_s], 0) > 0);
        integer(capture[u"fps"_s], 1);
        boolean(capture[u"narrow"_s]);
        boolean(capture[u"night"_s]);
        bool configured = false;
        for (const auto &value : array(capture[u"states"_s])) {
            const auto state = object(value);
            const auto op = object(state[u"op"_s]);
            boolean(state[u"capture"_s]);
            validateRulerState(state[u"state"_s]);
            if (op.contains(u"frames"_s)) {
                requireInput(configured);
                const int frames = integer(op[u"frames"_s], 1);
                const auto traceValue = state[u"state"_s].toObject()[u"trace"_s];
                if (!traceValue.isUndefined()) {
                    requireInput(traceValue.isArray());
                    const auto trace = traceValue.toArray();
                    requireInput(trace.isEmpty() || trace.size() == frames);
                    for (const auto &frame : trace)
                        validateRulerState(frame);
                }
            } else {
                const int count = integer(op[u"count"_s]);
                const int row = integer(op[u"row"_s]);
                requireInput(count == 0 ? row == 0 : row < count);
                number(op[u"physical"_s]);
                boolean(op[u"loop"_s]);
                if (op.contains(u"group"_s))
                    boolean(op[u"group"_s]);
                configured = true;
            }
        }
    }
    for (const auto &value : array(source[u"ordinalSamples"_s])) {
        const auto sample = object(value);
        const auto op = object(sample[u"op"_s]);
        number(sample[u"value"_s]);
        number(sample[u"velocity"_s]);
        requireInput(op.contains(u"dt"_s) != op.contains(u"target"_s));
        if (op.contains(u"dt"_s))
            number(op[u"dt"_s], 0);
        else {
            number(op[u"target"_s]);
            if (op.contains(u"instant"_s))
                boolean(op[u"instant"_s]);
        }
    }
}
}
