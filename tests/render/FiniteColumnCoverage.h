// Establishes nonempty finite-fixture identity and draw coverage before numerical reductions.
// The required IDs come from the anonymous 0/1/2/3/49/106-column fixture boundaries.
#pragma once
#include "Require.h"
#include "RhiItem.h"
#include <QSet>
#include <QJsonObject>

namespace yalahoo::review {
inline void requireFiniteColumnCoverage(const rhi::Input &input, const QJsonObject &metadata,
                                        const QString &selected)
{
    using namespace Qt::StringLiterals;
    require(!input.entities.isEmpty() && input.entities.size() == input.scene.instances.size()
                    && metadata[u"drawCount"_s].toInteger() == input.entities.size()
                    && metadata[u"visibleEntities"_s].toInt() > 0
                    && !metadata[u"submittedShellUniforms"_s].toArray().isEmpty(),
            "Finite fixture has missing entities or draw submissions");
    QSet<QString> remaining{ u"1:0"_s, u"2:0"_s, u"2:1"_s, u"3:0"_s, u"3:1"_s, u"3:2"_s, selected };
    for (const auto &instance : input.scene.instances)
        remaining.remove(instance.albumId);
    require(remaining.isEmpty(), "Finite fixture lost required boundary identities");
}
}
