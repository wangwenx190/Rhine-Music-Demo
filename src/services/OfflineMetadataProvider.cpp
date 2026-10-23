#include "OfflineMetadataProvider.h"
#include <QList>
#include <QHash>
#include "metadata/MetadataMerge.h"
#include <QCoreApplication>
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
struct OfflineMetadataProvider::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(OfflineMetadataProvider *owner)
        : q(owner), m_cache(std::make_shared<const LibrarySnapshot>()), m_dispatch(owner)
    {
    }
    OfflineMetadataProvider *const q;

    struct Pending final
    {
        AlbumQuery query;
        RequestToken token;
        MetadataKind kind;
        Snapshot cache;
        qsizetype albumIndex = -1;
    };
    void dispatch();
    Snapshot m_cache;
    QHash<QString, qsizetype> m_albumIndices;
    QList<Pending> m_pending;
    QTimer m_dispatch;
};
OfflineMetadataProvider::OfflineMetadataProvider(QObject *parent)
    : IMetadataProvider(parent), d(std::make_unique<Private>(this))
{
    d->m_dispatch.setSingleShot(true);
    connect(&d->m_dispatch, &QTimer::timeout, this, [this] {
        d->dispatch();
    });
}
void OfflineMetadataProvider::setCache(Snapshot cache)
{
    d->m_cache = cache ? std::move(cache) : std::make_shared<const LibrarySnapshot>();
    d->m_albumIndices.clear();
    d->m_albumIndices.reserve(d->m_cache->albums.size());
    for (qsizetype index = 0; index < d->m_cache->albums.size(); ++index)
        d->m_albumIndices.tryEmplace(d->m_cache->albums[index].id, index);
}
void OfflineMetadataProvider::cancel(RequestToken token)
{
    d->m_pending.removeIf([&token](const Private::Pending &pending) {
        return pending.token == token;
    });
    if (d->m_pending.isEmpty())
        d->m_dispatch.stop();
}
bool OfflineMetadataProvider::request(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    for (const auto &pending : std::as_const(d->m_pending))
        if (pending.token == token)
            return true;
    if (d->m_pending.size() >= yalahoo::metadataLimits::MaximumPendingRequests)
        return false;
    const auto albumIndex = d->m_albumIndices.value(token.albumId, -1);
    d->m_pending.append({ std::move(query), std::move(token), kind, d->m_cache, albumIndex });
    if (!d->m_dispatch.isActive())
        d->m_dispatch.start(0);
    return true;
}
void OfflineMetadataProvider::Private::dispatch()
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
    result.setRequest(token, kind);
    if (query.albumId != token.albumId) {
        result.status = ProviderStatus::Error;
        result.error = u"Album query/token mismatch"_s;
        Q_EMIT q->resultReady(result);
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
    Q_EMIT q->resultReady(result);
}
ProviderCapabilities OfflineMetadataProvider::capabilities() const
{
    return { };
}
OfflineMetadataProvider::~OfflineMetadataProvider() = default;

}
