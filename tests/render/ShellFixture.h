// Checks the complete source-state inventory and projection evidence before GPU work starts.
#pragma once
#include "Require.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace yalahoo::review {
inline void validateShellFixture(const QJsonObject &source)
{
    using namespace Qt::StringLiterals;
    const auto cases = source[u"cases"_s].toArray();
    require(cases.size() == 61, "Incomplete source shell states");
    QSet<int> seen;
    for (const auto &value : cases) {
        require(value.isObject(), "Invalid shell state");
        const auto state = value.toObject();
        const auto identity = state[u"sourceIndex"_s];
        const double index = identity.toDouble(-1);
        require(identity.isDouble() && std::isfinite(index) && std::trunc(index) == index
                        && (index == 2 || (index >= 4 && index <= 57)
                            || (index >= 60 && index <= 65)),
                "Unexpected source shell identity");
        require(!seen.contains(int(index)), "Duplicate source shell identity");
        seen.insert(int(index));
        const auto projections = state[u"projections"_s].toArray();
        require(projections.size() == 5, "Incomplete source shell projection evidence");
        for (const auto &value : projections) {
            require(value.isObject(), "Invalid source shell projection sample");
            const auto sample = value.toObject();
            for (const auto key : { "world"_L1, "pixel"_L1 }) {
                const auto coordinates = sample[key].toArray();
                require(coordinates.size() == (key == "world"_L1 ? 3 : 2),
                        "Invalid source shell projection coordinates");
                for (const auto &coordinate : coordinates)
                    require(coordinate.isDouble() && std::isfinite(coordinate.toDouble()),
                            "Nonfinite source shell projection coordinate");
            }
        }
    }
}
}
