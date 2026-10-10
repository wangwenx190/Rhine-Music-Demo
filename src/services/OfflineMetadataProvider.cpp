#include "OfflineMetadataProvider.h"
#include <QTimer>

namespace yalahoo {
QString kindName(MetadataKind kind)
{
    switch (kind) {
    case MetadataKind::Credits: return QStringLiteral("credits");
    case MetadataKind::Enrichment: return QStringLiteral("enrichment");
    default: return QStringLiteral("introduction");
    }
}
QString statusName(ProviderStatus status)
{
    switch (status) {
    case ProviderStatus::Matched: return QStringLiteral("Matched");
    case ProviderStatus::Partial: return QStringLiteral("Partial");
    case ProviderStatus::NotFound: return QStringLiteral("NotFound");
    case ProviderStatus::Uncertain: return QStringLiteral("Uncertain");
    case ProviderStatus::Error: return QStringLiteral("Error");
    case ProviderStatus::Cancelled: return QStringLiteral("Cancelled");
    default: return QStringLiteral("Deferred");
    }
}
namespace {
ProviderStatus cachedStatus(const QJsonObject &object)
{
    const QString status = object.value("status").toString();
    if (status == QLatin1String("matched")) return ProviderStatus::Matched;
    if (status == QLatin1String("partial")) return ProviderStatus::Partial;
    if (status == QLatin1String("not-found")) return ProviderStatus::NotFound;
    if (status == QLatin1String("uncertain")) return ProviderStatus::Uncertain;
    if (status == QLatin1String("error")) return ProviderStatus::Error;
    return ProviderStatus::Deferred;
}
}
OfflineMetadataProvider::OfflineMetadataProvider(QObject *parent) : IMetadataProvider(parent), m_cache(std::make_shared<const LibrarySnapshot>()) {}
void OfflineMetadataProvider::setCache(Snapshot cache)
{ m_cache = cache ? std::move(cache) : std::make_shared<const LibrarySnapshot>(); }
void OfflineMetadataProvider::requestIntroduction(AlbumQuery query, RequestToken token)
{ request(std::move(query), std::move(token), MetadataKind::Introduction); }
void OfflineMetadataProvider::requestCredits(AlbumQuery query, RequestToken token)
{ request(std::move(query), std::move(token), MetadataKind::Credits); }
void OfflineMetadataProvider::requestEnrichment(AlbumQuery query, RequestToken token)
{ request(std::move(query), std::move(token), MetadataKind::Enrichment); }
void OfflineMetadataProvider::cancel(RequestToken token)
{ if (m_pending.contains(token)) m_pending[token] = true; }
void OfflineMetadataProvider::request(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    if (m_pending.contains(token)) return;
    m_pending.insert(token, false);
    const Snapshot cache = m_cache;
    // Context-bound event-loop delivery: never emit inline, never outlive provider.
    QTimer::singleShot(0, this, [this, query = std::move(query), token, kind, cache] {
        ProviderResult result;
        result.requestId = token.requestId; result.generation = token.generation;
        result.albumId = token.albumId; result.kind = kind;
        const bool cancelled = m_pending.take(token);
        if (cancelled) { result.status = ProviderStatus::Cancelled; emit resultReady(result); return; }
        if (query.albumId != token.albumId) {
            result.status = ProviderStatus::Error; result.error = QStringLiteral("Album query/token mismatch");
            emit resultReady(result); return;
        }
        if (const auto *album = cache->album(token.albumId)) {
            QJsonObject state;
            switch (kind) {
            case MetadataKind::Introduction:
                state = album->introduction;
                result.payload = {{"description", album->description}, {"introduction", state}};
                result.source = album->descriptionSource;
                result.status = cachedStatus(state);
                if (result.status == ProviderStatus::Deferred && !album->description.isEmpty()) result.status = ProviderStatus::Matched;
                break;
            case MetadataKind::Credits:
                state = album->creditsLookup;
                result.payload = {{"producers", toJson(*album).value("producers")}, {"creditsLookup", state}};
                result.source = {{"name", state.value("source")}};
                result.status = cachedStatus(state);
                if (result.status == ProviderStatus::Deferred && !album->producers.isEmpty()) result.status = ProviderStatus::Partial;
                break;
            case MetadataKind::Enrichment:
                state = album->online; result.payload = {{"online", state}};
                result.source = {{"name", "MusicBrainz"}, {"url", state.value("sourceUrl")}};
                result.status = cachedStatus(state);
                break;
            }
            if (result.status != ProviderStatus::Deferred) {
                result.checkedAt = state.value("checkedAt").toString(result.source.value("checkedAt").toString());
                result.retryAt = state.value("retryAt").toString(); result.error = state.value("error").toString();
            }
        }
        if (result.status == ProviderStatus::Deferred) {
            result.payload = {}; result.source = {}; result.checkedAt.clear(); result.retryAt.clear();
            result.error = QStringLiteral("联网资料功能暂未启用");
        }
        emit resultReady(result);
    });
}
}
