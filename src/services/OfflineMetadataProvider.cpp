#include "IMetadataProvider.h"
#include <QCoreApplication>
#include "OfflineMetadataProvider.h"
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
QString kindName(MetadataKind kind)
{
    switch (kind) {
    case MetadataKind::Credits:
        return u"credits"_s;
    case MetadataKind::Enrichment:
        return u"enrichment"_s;
    default:
        return u"introduction"_s;
    }
}
QString statusName(ProviderStatus status)
{
    switch (status) {
    case ProviderStatus::Matched:
        return u"Matched"_s;
    case ProviderStatus::Partial:
        return u"Partial"_s;
    case ProviderStatus::NotFound:
        return u"NotFound"_s;
    case ProviderStatus::Uncertain:
        return u"Uncertain"_s;
    case ProviderStatus::Error:
        return u"Error"_s;
    case ProviderStatus::Cancelled:
        return u"Cancelled"_s;
    default:
        return u"Deferred"_s;
    }
}
namespace {
ProviderStatus cachedStatus(const QJsonObject &object)
{
    const QString status = object.value(u"status"_s).toString();
    if (status == "matched"_L1)
        return ProviderStatus::Matched;
    if (status == "partial"_L1)
        return ProviderStatus::Partial;
    if (status == "not-found"_L1)
        return ProviderStatus::NotFound;
    if (status == "uncertain"_L1)
        return ProviderStatus::Uncertain;
    if (status == "error"_L1)
        return ProviderStatus::Error;
    return ProviderStatus::Deferred;
}
}
OfflineMetadataProvider::OfflineMetadataProvider(QObject *parent)
    : IMetadataProvider(parent),
      m_cache(std::make_shared<const LibrarySnapshot>()),
      m_dispatch(this)
{
    m_dispatch.setSingleShot(true);
    connect(&m_dispatch, &QTimer::timeout, this, &OfflineMetadataProvider::dispatch);
}
void OfflineMetadataProvider::setCache(Snapshot cache)
{
    m_cache = cache ? std::move(cache) : std::make_shared<const LibrarySnapshot>();
    m_albumIndices.clear();
    m_albumIndices.reserve(m_cache->albums.size());
    for (qsizetype index = 0; index < m_cache->albums.size(); ++index)
        m_albumIndices.tryEmplace(m_cache->albums[index].id, index);
}
void OfflineMetadataProvider::cancel(RequestToken token)
{
    m_pending.removeIf([&token](const Pending &pending) {
        return pending.token == token;
    });
    if (m_pending.isEmpty())
        m_dispatch.stop();
}
bool OfflineMetadataProvider::request(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    for (const auto &pending : std::as_const(m_pending))
        if (pending.token == token)
            return true;
    if (m_pending.size() >= yalahoo::metadataLimits::MaximumPendingRequests)
        return false;
    const auto albumIndex = m_albumIndices.value(token.albumId, -1);
    m_pending.append({ std::move(query), std::move(token), kind, m_cache, albumIndex });
    if (!m_dispatch.isActive())
        m_dispatch.start(0);
    return true;
}
void OfflineMetadataProvider::dispatch()
{
    if (m_pending.isEmpty())
        return;
    const auto pending = m_pending.takeFirst();
    if (!m_pending.isEmpty())
        m_dispatch.start(0);
    const auto &query = pending.query;
    const auto &token = pending.token;
    const auto kind = pending.kind;
    const auto &cache = pending.cache;
    const auto albumIndex = pending.albumIndex;
    ProviderResult result;
    result.requestId = token.requestId;
    result.generation = token.generation;
    result.albumId = token.albumId;
    result.kind = kind;
    if (query.albumId != token.albumId) {
        result.status = ProviderStatus::Error;
        result.error = u"Album query/token mismatch"_s;
        Q_EMIT resultReady(result);
        return;
    }
    if (albumIndex >= 0) {
        const auto *album = &cache->albums[albumIndex];
        QJsonObject state;
        switch (kind) {
        case MetadataKind::Introduction:
            state = album->introduction;
            result.payload = { { u"description"_s, album->description },
                               { u"introduction"_s, state } };
            result.source = album->descriptionSource;
            result.status = cachedStatus(state);
            if (result.status == ProviderStatus::Deferred && !album->description.isEmpty())
                result.status = ProviderStatus::Matched;
            break;
        case MetadataKind::Credits:
            state = album->creditsLookup;
            result.payload = { { u"producers"_s, producerData(album->producers) },
                               { u"creditsLookup"_s, state } };
            result.source = { { u"name"_s, state.value(u"source"_s) } };
            result.status = cachedStatus(state);
            if (result.status == ProviderStatus::Deferred && !album->producers.isEmpty())
                result.status = ProviderStatus::Partial;
            break;
        case MetadataKind::Enrichment:
            state = album->online;
            result.payload = { { u"online"_s, state } };
            result.source = { { u"name"_s, u"MusicBrainz"_s },
                              { u"url"_s, state.value(u"sourceUrl"_s) } };
            result.status = cachedStatus(state);
            break;
        }
        if (result.status != ProviderStatus::Deferred) {
            result.checkedAt = state.value(u"checkedAt"_s)
                                       .toString(result.source.value(u"checkedAt"_s).toString());
            result.retryAt = state.value(u"retryAt"_s).toString();
            result.error = state.value(u"error"_s).toString();
        }
    }
    if (result.status == ProviderStatus::Deferred) {
        result.payload = { };
        result.source = { };
        result.checkedAt.clear();
        result.retryAt.clear();
        result.error = QCoreApplication::translate("Yalahoo", "Online metadata is not enabled yet");
    }
    Q_EMIT resultReady(result);
}
}
