#include "metadata/MetadataMerge.h"
#include <QCoreApplication>
#include "OfflineMetadataProvider.h"
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
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
    if (albumIndex >= 0)
        result = cachedMetadata(cache->albums[albumIndex], kind, token);
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
