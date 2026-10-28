// Reports cover ownership and actual retained payloads for regression consumers.
#include "SceneCoversState_p.h"
#include <QJsonArray>
using namespace Qt::StringLiterals;
namespace yalahoo::rhi {
QJsonObject SceneCovers::statistics() const
{
    int detailed = 0, owned = 0;
    qint64 ownedBytes = 0, tiledBytes = 0;
    bool selectedDetailed = false;
    QJsonArray owners;
    for (const auto &cover : d->m_ownedCovers) {
        if (!cover.pixels)
            continue;
        ++owned;
        owners.append(QJsonObject{
                { u"objectId"_s, qint64(cover.objectId) },
                { u"albumId"_s, cover.pixels->identity },
                { u"lease"_s, qint64(cover.lease) },
                { u"resolution"_s, cover.pixels->pixels ? cover.pixels->pixels->resolution : 0 } });
        if (cover.pixels->pixels) {
            ownedBytes += cover.pixels->pixels->byteSize();
            detailed += cover.pixels->pixels->resolution >= d->m_detailResolution;
            if (cover.objectId == d->m_activeObject)
                selectedDetailed = cover.pixels->pixels->resolution >= d->m_detailResolution;
        }
        for (const auto &upload : cover.uploads)
            tiledBytes += AtlasUploadQueue::payloadBytes(upload);
    }
    return { { u"owners"_s, owners },
             { u"cache"_s, d->m_covers.statistics() },
             { u"identities"_s, d->m_poolLeases.size() },
             { u"ready"_s, d->m_readyCovers.size() },
             { u"owned"_s, owned },
             { u"detailed"_s, detailed },
             { u"selectedDetailed"_s, selectedDetailed },
             { u"atlasRetainedBytes"_s, d->m_atlas.retainedBytes() },
             { u"ownedBytes"_s, ownedBytes },
             { u"tiledBytes"_s, tiledBytes },
             { u"atlasSize"_s, QJsonArray{ d->m_atlasSize.width(), d->m_atlasSize.height() } } };
}
}
