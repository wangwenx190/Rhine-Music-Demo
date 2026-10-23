#include "MetadataCoordinator.h"
#include "LibraryIndex.h"
#include "MetadataPresentation.h"
#include "MetadataMerge.h"
#include "OfflineMetadataProvider.h"
#include <QPointer>
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
namespace {
QVariantMap metadataValue(const ProviderResult &result)
{
    return { { u"requestId"_s, QVariant::fromValue(result.requestId) },
             { u"generation"_s, QVariant::fromValue(result.generation) },
             { u"albumId"_s, result.albumId },
             { u"kind"_s, kindName(result.kind) },
             { u"status"_s, statusName(result.status) },
             { u"payload"_s, result.payload.toVariantMap() },
             { u"source"_s, result.source.toVariantMap() },
             { u"checkedAt"_s, result.checkedAt },
             { u"retryAt"_s, result.retryAt },
             { u"error"_s, result.error } };
}

void retainIntroduction(QVariantMap &value, QVariantMap &payload, const Album &resolved)
{
    payload.insert(u"description"_s, resolved.description);
    if (!resolved.descriptionSource.isEmpty())
        value.insert(u"source"_s, resolved.descriptionSource.toVariantMap());
}
}

struct MetadataCoordinator::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private(MetadataCoordinator *owner, IMetadataProvider *provider, const LibraryIndex &library)
        : q(owner), m_provider(provider), m_library(library)
    {
    }
    MetadataCoordinator *const q;
    struct Pending final
    {
        RequestToken token;
        MetadataKind kind;
    };
    bool invalidate();
    quint64 request(const QString &albumId, const QString &name, bool force,
                    const QString &releaseId, bool batch);
    void receive(const ProviderResult &result);
    void advanceBatch();
    void updateOfflineCache();
    QPointer<IMetadataProvider> m_provider;
    const LibraryIndex &m_library;
    QHash<quint64, Pending> m_pending;
    quint64 m_generation = 0, m_requestId = 0, m_metadataRevision = 0;
    QHash<QString, ProviderResult> m_metadata;
    QStringList m_batchAlbums;
    QString m_batchKind, m_batchStatus;
    quint64 m_batchRequest = 0;
    int m_batchNext = 0, m_batchCompleted = 0, m_batchErrors = 0;
    bool m_batchActive = false, m_closing = false;
};
MetadataCoordinator::MetadataCoordinator(IMetadataProvider *provider, const LibraryIndex &library,
                                         QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, provider, library))
{
    qRegisterMetaType<ProviderResult>();
    if (provider) {
        connect(provider, &IMetadataProvider::capabilitiesChanged, this,
                &MetadataCoordinator::capabilitiesChanged);
        connect(
                provider, &IMetadataProvider::resultReady, this,
                [this](const ProviderResult &result) {
                    d->receive(result);
                },
                Qt::QueuedConnection);
        connect(provider, &QObject::destroyed, this, [this] {
            d->invalidate();
            ++d->m_metadataRevision;
            Q_EMIT changed();
            Q_EMIT progressChanged();
            Q_EMIT capabilitiesChanged();
        });
    }
}
MetadataCoordinator::~MetadataCoordinator()
{
    shutdown();
}
bool MetadataCoordinator::invalidate()
{
    return d->invalidate();
}
void MetadataCoordinator::shutdown()
{
    d->m_closing = true;
    d->invalidate();
}
void MetadataCoordinator::Private::updateOfflineCache()
{
    if (auto *offline = qobject_cast<OfflineMetadataProvider *>(m_provider.data()))
        offline->setCache(m_library.snapshot());
}
void MetadataCoordinator::libraryReplaced()
{
    d->updateOfflineCache();
    d->m_metadata.clear();
    ++d->m_metadataRevision;
    Q_EMIT changed();
    Q_EMIT progressChanged();
}
void MetadataCoordinator::configurationChanged()
{
    ++d->m_metadataRevision;
    Q_EMIT changed();
    Q_EMIT progressChanged();
}
void MetadataCoordinator::metadataCommitted()
{
    for (auto it = d->m_metadata.begin(); it != d->m_metadata.end();) {
        const auto &result = it.value();
        const auto *entry = d->m_library.album(result.albumId);
        const auto field = metadataStateKey(result.kind);
        const auto state = entry ? metadataState(*entry, result.kind) : QJsonObject();
        if (!state.isEmpty() && state == result.payload.value(field).toObject())
            it = d->m_metadata.erase(it);
        else
            ++it;
    }
    d->updateOfflineCache();
    ++d->m_metadataRevision;
    Q_EMIT changed();
}

QVariantMap MetadataCoordinator::metadata() const
{
    QVariantMap result;
    for (auto it = d->m_metadata.cbegin(); it != d->m_metadata.cend(); ++it)
        result.insert(it.key(), metadataValue(it.value()));
    return result;
}

QVariantMap MetadataCoordinator::capabilities() const
{
    const auto capabilities = d->m_provider
            ? d->m_provider->capabilities()
            : ProviderCapabilities{ false, false, false, false, false };
    return { { u"networkEnabled"_s, capabilities.networkEnabled },
             { u"introductions"_s, capabilities.introductions },
             { u"credits"_s, capabilities.credits },
             { u"enrichment"_s, capabilities.enrichment },
             { u"batch"_s, capabilities.batch } };
}

quint64 MetadataCoordinator::request(const QString &albumId, const QString &name, bool force,
                                     const QString &releaseId)
{
    return d->request(albumId, name, force, releaseId, false);
}
quint64 MetadataCoordinator::Private::request(const QString &albumId, const QString &name,
                                              bool force, const QString &releaseId, bool batch)
{
    const auto snapshot = m_library.snapshot();
    const auto generation = m_generation;
    const auto provider = m_provider;
    const auto *album = m_library.album(albumId);
    if (m_closing || !album || !m_provider)
        return 0;
    const auto parsedKind = metadataKind(name);
    if (!parsedKind || !supportsMetadata(m_provider->capabilities(), *parsedKind))
        return 0;
    const auto kind = *parsedKind;
    const auto requests = m_pending;
    for (const auto &request : requests)
        if (request.token.albumId == albumId && request.kind == kind)
            q->cancel(request.token.requestId);
    if (m_generation != generation || m_library.snapshot() != snapshot || m_provider != provider
        || !provider)
        return 0;
    if (m_pending.size() >= yalahoo::metadataLimits::MaximumPendingRequests)
        return 0;
    const RequestToken token{ ++m_requestId, m_generation, albumId };
    m_pending.insert(token.requestId, { token, kind });
    // Establish batch ownership before provider calls or synchronous notifications can cancel it.
    if (batch)
        m_batchRequest = token.requestId;
    AlbumQuery query;
    query.configuration = m_library.snapshot()->fields.value(u"metadataConfiguration"_s).toObject();
    query.contact = query.configuration.value(u"musicBrainzContact"_s).toString();
    query.enabled = m_provider->capabilities().networkEnabled;
    query.albumId = albumId;
    query.album = q->resolvedAlbum(albumId);
    query.force = force;
    query.releaseId = releaseId;
    query.albumIds = { albumId };
    for (const auto &track : album->tracks)
        query.trackIds.append(track.id);
    bool accepted = false;
    switch (kind) {
    case MetadataKind::Introduction:
        accepted = provider->requestIntroduction(query, token);
        break;
    case MetadataKind::Credits:
        accepted = provider->requestCredits(query, token);
        break;
    case MetadataKind::Enrichment:
        accepted = provider->requestEnrichment(query, token);
        break;
    }
    if (!accepted || !m_pending.contains(token.requestId)) {
        m_pending.remove(token.requestId);
        return 0;
    }
    ++m_metadataRevision;
    Q_EMIT q->changed();
    Q_EMIT q->progressChanged();
    return m_pending.contains(token.requestId) ? token.requestId : 0;
}

void MetadataCoordinator::cancel(quint64 requestId)
{
    const auto it = d->m_pending.find(requestId);
    if (it == d->m_pending.end())
        return;
    const auto token = it->token;
    d->m_pending.erase(it);
    if (d->m_batchRequest == requestId) {
        d->m_batchRequest = 0;
        d->m_batchActive = false;
        d->m_batchStatus = u"Cancelled"_s;
        Q_EMIT progressChanged();
    }
    if (d->m_provider)
        d->m_provider->cancel(token);
    ++d->m_metadataRevision;
    Q_EMIT changed();
    Q_EMIT progressChanged();
}

QVariantMap MetadataCoordinator::configuration() const
{
    return d->m_library.snapshot()
            ->fields.value(u"metadataConfiguration"_s)
            .toObject()
            .toVariantMap();
}

QVariantMap MetadataCoordinator::progress() const
{
    return { { u"active"_s, d->m_batchActive },
             { u"kind"_s, d->m_batchKind },
             { u"total"_s, d->m_batchAlbums.size() },
             { u"completed"_s, d->m_batchCompleted },
             { u"errors"_s, d->m_batchErrors },
             { u"status"_s, d->m_batchStatus },
             { u"statusText"_s, metadataStatusText(d->m_batchStatus) } };
}

void MetadataCoordinator::cancelAll()
{
    if (!d->invalidate())
        return;
    ++d->m_metadataRevision;
    Q_EMIT changed();
    Q_EMIT progressChanged();
}

bool MetadataCoordinator::requestBatch(const QString &kind, bool missingOnly)
{
    const auto parsedKind = metadataKind(kind);
    if (d->m_closing || !d->m_provider || !parsedKind
        || !supportsMetadata(d->m_provider->capabilities(), *parsedKind))
        return false;
    const auto snapshot = d->m_library.snapshot();
    const auto provider = d->m_provider;
    if (!d->invalidate() || d->m_library.snapshot() != snapshot || d->m_provider != provider)
        return false;
    d->m_batchAlbums.clear();
    for (const auto &entry : d->m_library.snapshot()->albums) {
        const auto state = metadataState(entry, *parsedKind);
        if (!missingOnly || state.isEmpty())
            d->m_batchAlbums.append(entry.id);
    }
    d->m_batchNext = d->m_batchCompleted = d->m_batchErrors = 0;
    d->m_batchKind = kind;
    d->m_batchStatus = u"Pending"_s;
    d->m_batchActive = true;
    d->advanceBatch();
    return true;
}

QVariantMap MetadataCoordinator::forAlbum(const QString &albumId, const QString &kind,
                                          quint64 revision) const
{
    if (revision && revision != d->m_metadataRevision)
        return { };
    const auto key = albumId + '/'_L1 + kind;
    const auto *entry = d->m_library.album(albumId);
    if (!entry)
        return { };
    const auto present = [&](QVariantMap value) {
        for (const auto &pending : d->m_pending)
            if (pending.token.albumId == albumId && kindName(pending.kind) == kind) {
                value.insert(u"status"_s, u"Pending"_s);
                value.insert(u"error"_s, QString());
                break;
            }
        return metadataPresentation(std::move(value), entry->title);
    };
    if (const auto cached = d->m_metadata.constFind(key); cached != d->m_metadata.cend()) {
        auto value = metadataValue(*cached);
        auto payload = value.value(u"payload"_s).toMap();
        const auto resolved = resolvedAlbum(albumId);
        if (kind == "introduction"_L1 && payload.value(u"description"_s).toString().isEmpty()
            && !resolved.description.isEmpty()) {
            retainIntroduction(value, payload, resolved);
        } else if (kind == "credits"_L1 && payload.value(u"producers"_s).toList().isEmpty()
                   && !resolved.producers.isEmpty()) {
            payload.insert(u"producers"_s, producerData(resolved.producers).toVariantList());
        }
        value.insert(u"payload"_s, payload);
        return present(std::move(value));
    }
    const auto parsedKind = metadataKind(kind);
    if (!parsedKind)
        return { };
    return present(
            metadataValue(cachedMetadata(*entry, *parsedKind, { 0, d->m_generation, albumId })));
}

Album MetadataCoordinator::resolvedAlbum(const QString &albumId) const
{
    const auto *entry = d->m_library.album(albumId);
    if (!entry)
        return { };
    auto resolved = *entry;
    for (const auto kind :
         { MetadataKind::Introduction, MetadataKind::Credits, MetadataKind::Enrichment }) {
        const auto cached = d->m_metadata.constFind(albumId + '/'_L1 + kindName(kind));
        if (cached != d->m_metadata.cend())
            mergeMetadata(resolved, *cached);
    }
    return resolved;
}

bool MetadataCoordinator::busy() const
{
    return !d->m_pending.isEmpty();
}

quint64 MetadataCoordinator::revision() const
{
    return d->m_metadataRevision;
}

bool MetadataCoordinator::Private::invalidate()
{
    const auto pending = std::exchange(m_pending, { });
    if (m_batchActive)
        m_batchStatus = u"Cancelled"_s;
    m_batchActive = false;
    m_batchRequest = 0;
    const auto generation = ++m_generation;
    for (const auto &request : pending) {
        if (!m_provider)
            break;
        m_provider->cancel(request.token);
    }
    return generation == m_generation;
}

void MetadataCoordinator::Private::receive(const ProviderResult &result)
{
    const auto it = m_pending.find(result.requestId);
    if (it == m_pending.end() || result.generation != m_generation
        || result.generation != it->token.generation || result.albumId != it->token.albumId
        || result.kind != it->kind || !m_library.album(result.albumId))
        return;
    const auto generation = m_generation;
    m_pending.erase(it);
    m_metadata.insert(result.albumId + '/'_L1 + kindName(result.kind), result);
    ++m_metadataRevision;
    Q_EMIT q->changed();
    if (generation != m_generation)
        return;
    Q_EMIT q->albumChanged(result.albumId);
    if (generation != m_generation)
        return;
    Q_EMIT q->delivered(result);
    Q_EMIT q->progressChanged();
    if (generation != m_generation)
        return;
    if (m_batchActive && m_batchRequest == result.requestId) {
        m_batchRequest = 0;
        ++m_batchCompleted;
        if (result.status == ProviderStatus::Error)
            ++m_batchErrors;
        m_batchStatus = statusName(result.status);
        Q_EMIT q->progressChanged();
        QTimer::singleShot(0, q, [this] {
            advanceBatch();
        });
    }
}

void MetadataCoordinator::Private::advanceBatch()
{
    if (!m_batchActive || m_batchRequest)
        return;
    if (m_batchNext >= m_batchAlbums.size()) {
        m_batchActive = false;
        Q_EMIT q->progressChanged();
        return;
    }
    const auto generation = m_generation;
    const auto albumId = m_batchAlbums[m_batchNext++];
    const auto kind = m_batchKind;
    const auto requestId = request(albumId, kind, false, { }, true);
    // The request publishes signals. A newer batch or cancellation owns all subsequent state.
    if (generation != m_generation || !m_batchActive)
        return;
    if (!requestId) {
        m_batchRequest = 0;
        m_batchActive = false;
        m_batchStatus = u"Error"_s;
    }
    Q_EMIT q->progressChanged();
}

}
