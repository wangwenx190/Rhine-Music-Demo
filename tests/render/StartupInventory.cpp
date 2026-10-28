// Reports startup counters without making product readiness depend on a JSON schema.
#include "StartupCoordinator.h"
using namespace Qt::StringLiterals;
namespace yalahoo {
QJsonObject StartupCoordinator::statistics() const
{
    return { { u"receivedSnapshots"_s, qint64(m_received) },
             { u"appliedSnapshots"_s, qint64(m_applied) },
             { u"coalescedSnapshots"_s, qint64(m_coalesced) },
             { u"pendingSnapshots"_s, int(bool(m_pending)) },
             { u"initialReady"_s, m_initialReady },
             { u"visualActive"_s, m_visualActive },
             { u"snapshotFrozen"_s, m_frozen },
             { u"resumeSegments"_s, qint64(m_segments) } };
}
}
