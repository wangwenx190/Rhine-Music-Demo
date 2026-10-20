#include <QCoreApplication>
#include "LibrarySession.h"
#include "MetadataPresentation.h"
#include "metadata/MetadataMerge.h"
#include "OfflineMetadataProvider.h"
#include <utility>
#include <algorithm>
#include <QThread>
#include <QSet>
#include <QTimer>

namespace {
// Bound accepted metadata intents so slow network work cannot grow the GUI queue.
constexpr auto MaximumPendingMetadataRequests = 8;
}

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
}
QVariantMap LibrarySession::metadata() const
{
    QVariantMap result;
    for (auto it = m_metadata.cbegin(); it != m_metadata.cend(); ++it)
        result.insert(it.key(), metadataValue(it.value()));
    return result;
}
LibrarySession::LibrarySession(IMetadataProvider *provider, QObject *parent)
    : QObject(parent),
      m_provider(provider),
      m_albums(this),
      m_search(this),
      m_snapshot(std::make_shared<const LibrarySnapshot>())
{
    qRegisterMetaType<ProviderResult>();
    connect(&m_albums, &AlbumListModel::columnsChanged, this,
            &LibrarySession::rebuildBrowsePositions);
    if (provider) {
        connect(provider, &IMetadataProvider::capabilitiesChanged, this,
                &LibrarySession::providerCapabilitiesChanged);
        connect(provider, &IMetadataProvider::resultReady, this, &LibrarySession::receive,
                Qt::QueuedConnection);
        connect(provider, &QObject::destroyed, this, [this] {
            invalidate();
            Q_EMIT metadataProgressChanged();
            Q_EMIT providerCapabilitiesChanged();
        });
    }
    rebuildSummary();
}
QVariantList LibrarySession::genres() const
{
    return m_genres;
}
bool LibrarySession::hasGenre(const QString &id) const
{
    return std::any_of(m_genres.cbegin(), m_genres.cend(), [&id](const QVariant &value) {
        return value.toMap().value(u"id"_s).toString() == id;
    });
}
QVariantMap LibrarySession::summary() const
{
    return m_summary;
}
void LibrarySession::rebuildSummary()
{
    int tracks = 0;
    QStringList offline;
    for (const auto &entry : m_snapshot->albums)
        tracks += int(entry.tracks.size());
    for (const auto &root : m_snapshot->roots)
        if (root.status == "offline"_L1)
            offline.append(
                    root.path.toLocalFile()
                    + QCoreApplication::translate(
                            "Yalahoo", " Temporarily offline. The original index was retained."));
    m_summary = { { u"albums"_s, m_snapshot->albums.size() },
                  { u"tracks"_s, tracks },
                  { u"genres"_s, genres().size() },
                  { u"offline"_s, offline.join('\n'_L1) } };
}
int LibrarySession::trackIndex(const QString &albumId, const QString &trackId) const
{
    return int(m_trackIndices.value(albumId).value(trackId, -1));
}
bool LibrarySession::contains(const QString &albumId, const QString &trackId) const
{
    return album(albumId) && (trackId.isEmpty() || track(albumId, trackId));
}
LibrarySession::~LibrarySession()
{
    invalidate();
}
void LibrarySession::invalidate()
{
    const auto pending = std::exchange(m_pending, { });
    if (m_batchActive)
        m_batchStatus = "Cancelled"_L1;
    m_batchActive = false;
    m_batchRequest = 0;
    ++m_generation;
    for (const auto &request : pending) {
        if (!m_provider)
            break;
        m_provider->cancel(request.token);
    }
}
void LibrarySession::setSnapshot(LibrarySnapshot snapshot)
{
    Q_ASSERT(QThread::currentThread() == thread());
    invalidate();
    m_snapshot = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    m_genres.clear();
    QSet<QString> usedGenres;
    for (const auto &entry : m_snapshot->albums)
        usedGenres.insert(entry.genreId);
    for (const auto &genre : m_snapshot->genres)
        if (usedGenres.contains(genre.id))
            m_genres.append(
                    QVariantMap{ { u"id"_s, genre.id }, { u"name"_s, genreDisplayName(genre) } });
    rebuildSummary();
    m_albumIndices.clear();
    m_genreNames.clear();
    for (const auto &genre : m_snapshot->genres)
        m_genreNames.insert(genre.id, genreDisplayName(genre));
    m_trackIndices.clear();
    m_albumIndices.reserve(m_snapshot->albums.size());
    for (qsizetype index = 0; index < m_snapshot->albums.size(); ++index) {
        const auto &entry = m_snapshot->albums[index];
        if (m_albumIndices.contains(entry.id))
            continue;
        m_albumIndices.insert(entry.id, index);
        if (!entry.tracks.isEmpty()) {
            QHash<QString, qsizetype> tracks;
            tracks.reserve(entry.tracks.size());
            for (qsizetype row = 0; row < entry.tracks.size(); ++row)
                if (!tracks.contains(entry.tracks[row].id))
                    tracks.insert(entry.tracks[row].id, row);
            m_trackIndices.insert(entry.id, std::move(tracks));
        }
    }
    if (auto *offline = qobject_cast<OfflineMetadataProvider *>(m_provider.data()))
        offline->setCache(m_snapshot);
    m_albums.setSnapshot(m_snapshot);
    m_search.update(m_snapshot, m_query, m_genre);
    if (!m_navigationAlbumId.isEmpty()
        && (!album(m_navigationAlbumId)
            || (!m_navigationTrackId.isEmpty()
                && !track(m_navigationAlbumId, m_navigationTrackId))))
        navigate({ }, { });
    // Playback IDs survive a library replacement: playing media may outlive its index.
    m_metadata.clear();
    ++m_metadataRevision;
    Q_EMIT metadataChanged();
    Q_EMIT browseInfoChanged();
    Q_EMIT snapshotChanged();
    Q_EMIT metadataConfigurationChanged();
    Q_EMIT metadataProgressChanged();
}
void LibrarySession::search(const QString &query, const QString &genreId)
{
    m_query = query;
    m_genre = genreId;
    m_search.update(m_snapshot, query, genreId);
}
void LibrarySession::sort(const QString &mode)
{
    if (mode == "genre"_L1)
        m_albums.setSortMode(SortMode::Genre);
    else if (mode == "artist"_L1)
        m_albums.setSortMode(SortMode::Artist);
    else if (mode == "album"_L1)
        m_albums.setSortMode(SortMode::Album);
    Q_EMIT browseInfoChanged();
}
void LibrarySession::rebuildBrowsePositions()
{
    m_browsePositions.clear();
    const auto &order = m_albums.order();
    m_browsePositions.reserve(order.indices.size());
    for (int i = 0; i < order.indices.size(); ++i)
        m_browsePositions[m_snapshot->albums[order.indices[i]].id].code = i + 1;
    for (int lane = 0; lane < order.columns.size(); ++lane) {
        const auto &column = order.columns[lane];
        for (int row = 0; row < column.albumIds.size(); ++row) {
            auto &position = m_browsePositions[column.albumIds[row]];
            position.number = row + 1;
            position.total = column.albumIds.size();
            position.lane = lane + 1;
            position.label = column.label;
        }
    }
}
bool LibrarySession::navigate(const QString &albumId, const QString &trackId)
{
    if (albumId.isEmpty() && !trackId.isEmpty())
        return false;
    if (!albumId.isEmpty() && (!album(albumId) || (!trackId.isEmpty() && !track(albumId, trackId))))
        return false;
    if (m_navigationAlbumId == albumId && m_navigationTrackId == trackId)
        return true;
    m_navigationAlbumId = albumId;
    m_navigationTrackId = trackId;
    Q_EMIT navigationChanged();
    Q_EMIT browseInfoChanged();
    return true;
}
QVariantMap LibrarySession::browseInfo() const
{
    const auto *selected = album(m_navigationAlbumId);
    if (!selected)
        return { };
    const QString genre = m_genreNames.value(selected->genreId);
    const auto position = m_browsePositions.value(selected->id);
    return { { u"title"_s, selected->title },
             { u"artist"_s, artistDisplayName(selected->artist) },
             { u"genre"_s, genre },
             { u"year"_s, selected->year ? QString::number(*selected->year) : QString{ } },
             { u"tracks"_s, selected->tracks.size() },
             { u"albumId"_s, selected->id },
             { u"code"_s, position.code },
             { u"number"_s, position.number },
             { u"columnCount"_s, position.total },
             { u"columnNumber"_s, position.lane },
             { u"columnTotal"_s, m_albums.order().columns.size() },
             { u"columnLabel"_s, position.label } };
}
bool LibrarySession::setPlaybackIds(const QString &albumId, const QString &trackId)
{
    if (!(albumId.isEmpty() && trackId.isEmpty()) && !track(albumId, trackId))
        return false;
    adoptPlaybackIds(albumId, trackId);
    return true;
}
void LibrarySession::adoptPlaybackIds(const QString &albumId, const QString &trackId)
{
    if (m_playbackAlbumId == albumId && m_playbackTrackId == trackId)
        return;
    m_playbackAlbumId = albumId;
    m_playbackTrackId = trackId;
    Q_EMIT playbackChanged();
}
QVariantMap LibrarySession::providerCapabilities() const
{
    const auto capabilities = m_provider
            ? m_provider->capabilities()
            : ProviderCapabilities{ false, false, false, false, false };
    return { { u"networkEnabled"_s, capabilities.networkEnabled },
             { u"introductions"_s, capabilities.introductions },
             { u"credits"_s, capabilities.credits },
             { u"enrichment"_s, capabilities.enrichment },
             { u"batch"_s, capabilities.batch } };
}
quint64 LibrarySession::requestMetadata(const QString &albumId, const QString &name, bool force,
                                        const QString &releaseId)
{
    const auto snapshot = m_snapshot;
    const auto generation = m_generation;
    const auto provider = m_provider;
    const auto *album = this->album(albumId);
    if (!album || !m_provider)
        return 0;
    const auto capabilities = m_provider->capabilities();
    if ((name == "introduction"_L1 && !capabilities.introductions)
        || (name == "credits"_L1 && !capabilities.credits)
        || (name == "enrichment"_L1 && !capabilities.enrichment))
        return 0;
    MetadataKind kind;
    if (name == "introduction"_L1)
        kind = MetadataKind::Introduction;
    else if (name == "credits"_L1)
        kind = MetadataKind::Credits;
    else if (name == "enrichment"_L1)
        kind = MetadataKind::Enrichment;
    else
        return 0;
    const auto requests = m_pending;
    for (const auto &request : requests)
        if (request.token.albumId == albumId && request.kind == kind)
            cancelMetadata(request.token.requestId);
    if (m_generation != generation || m_snapshot != snapshot || m_provider != provider || !provider)
        return 0;
    if (m_pending.size() >= MaximumPendingMetadataRequests)
        return 0;
    const RequestToken token{ ++m_requestId, m_generation, albumId };
    m_pending.insert(token.requestId, { token, kind });
    AlbumQuery query;
    query.configuration = m_snapshot->fields.value(u"metadataConfiguration"_s).toObject();
    query.contact = query.configuration.value(u"musicBrainzContact"_s).toString();
    query.enabled = m_provider->capabilities().networkEnabled;
    query.albumId = albumId;
    query.album = resolvedAlbum(albumId);
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
    Q_EMIT metadataChanged();
    Q_EMIT metadataProgressChanged();
    return m_pending.contains(token.requestId) ? token.requestId : 0;
}
void LibrarySession::cancelMetadata(quint64 requestId)
{
    const auto it = m_pending.find(requestId);
    if (it == m_pending.end())
        return;
    const auto token = it->token;
    m_pending.erase(it);
    if (m_batchRequest == requestId) {
        m_batchRequest = 0;
        m_batchActive = false;
        m_batchStatus = "Cancelled"_L1;
        Q_EMIT metadataProgressChanged();
    }
    if (m_provider)
        m_provider->cancel(token);
    ++m_metadataRevision;
    Q_EMIT metadataChanged();
    Q_EMIT metadataProgressChanged();
}
void LibrarySession::receive(const ProviderResult &result)
{
    const auto it = m_pending.find(result.requestId);
    if (it == m_pending.end() || result.generation != m_generation
        || result.generation != it->token.generation || result.albumId != it->token.albumId
        || result.kind != it->kind || !album(result.albumId))
        return;
    const auto generation = m_generation;
    m_pending.erase(it);
    m_metadata.insert(result.albumId + '/'_L1 + kindName(result.kind), result);
    ++m_metadataRevision;
    Q_EMIT metadataChanged();
    if (generation != m_generation)
        return;
    Q_EMIT albumMetadataChanged(result.albumId);
    if (generation != m_generation)
        return;
    Q_EMIT metadataDelivered(result);
    Q_EMIT metadataProgressChanged();
    if (generation != m_generation)
        return;
    if (m_batchActive && m_batchRequest == result.requestId) {
        m_batchRequest = 0;
        ++m_batchCompleted;
        if (result.status == ProviderStatus::Error)
            ++m_batchErrors;
        m_batchStatus = statusName(result.status);
        Q_EMIT metadataProgressChanged();
        QTimer::singleShot(0, this, &LibrarySession::advanceBatch);
    }
}
QVariantMap LibrarySession::metadataConfiguration() const
{
    return m_snapshot->fields.value(u"metadataConfiguration"_s).toObject().toVariantMap();
}
QVariantMap LibrarySession::metadataProgress() const
{
    return { { u"active"_s, m_batchActive },
             { u"kind"_s, m_batchKind },
             { u"total"_s, m_batchAlbums.size() },
             { u"completed"_s, m_batchCompleted },
             { u"errors"_s, m_batchErrors },
             { u"status"_s, m_batchStatus },
             { u"statusText"_s, metadataStatusText(m_batchStatus) } };
}
void LibrarySession::cancelAllMetadata()
{
    invalidate();
    ++m_metadataRevision;
    Q_EMIT metadataChanged();
    Q_EMIT metadataProgressChanged();
}
bool LibrarySession::requestMetadataBatch(const QString &kind, bool missingOnly)
{
    if (!m_provider
        || (kind != "introduction"_L1 && kind != "credits"_L1 && kind != "enrichment"_L1))
        return false;
    const auto capabilities = m_provider->capabilities();
    if ((kind == "introduction"_L1 && !capabilities.introductions)
        || (kind == "credits"_L1 && !capabilities.credits)
        || (kind == "enrichment"_L1 && !capabilities.enrichment))
        return false;
    invalidate();
    m_batchAlbums.clear();
    for (const auto &entry : m_snapshot->albums) {
        const auto state = kind == "introduction"_L1 ? entry.introduction
                : kind == "credits"_L1               ? entry.creditsLookup
                                                     : entry.online;
        if (!missingOnly || state.isEmpty())
            m_batchAlbums.append(entry.id);
    }
    m_batchNext = m_batchCompleted = m_batchErrors = 0;
    m_batchKind = kind;
    m_batchStatus = "Pending"_L1;
    m_batchActive = true;
    advanceBatch();
    return true;
}
void LibrarySession::advanceBatch()
{
    if (!m_batchActive || m_batchRequest)
        return;
    if (m_batchNext >= m_batchAlbums.size()) {
        m_batchActive = false;
        Q_EMIT metadataProgressChanged();
        return;
    }
    m_batchRequest = requestMetadata(m_batchAlbums[m_batchNext++], m_batchKind);
    if (!m_batchRequest) {
        m_batchActive = false;
        m_batchStatus = "Error"_L1;
    }
    Q_EMIT metadataProgressChanged();
}
QVariantMap LibrarySession::metadataFor(const QString &albumId, const QString &kind,
                                        quint64 revision) const
{
    if (revision && revision != m_metadataRevision)
        return { };
    const auto key = albumId + '/'_L1 + kind;
    const auto *entry = album(albumId);
    if (!entry)
        return { };
    const auto present = [&](QVariantMap value) {
        for (const auto &pending : m_pending)
            if (pending.token.albumId == albumId && kindName(pending.kind) == kind) {
                value.insert(u"status"_s, u"Pending"_s);
                value.insert(u"error"_s, QString());
                break;
            }
        return metadataPresentation(std::move(value), entry->title);
    };
    if (m_metadata.contains(key)) {
        auto value = metadataValue(m_metadata.value(key));
        auto payload = value.value(u"payload"_s).toMap();
        const auto resolved = resolvedAlbum(albumId);
        if (kind == "introduction"_L1 && payload.value(u"description"_s).toString().isEmpty()
            && !resolved.description.isEmpty()) {
            payload.insert(u"description"_s, resolved.description);
            if (!resolved.descriptionSource.isEmpty())
                value.insert(u"source"_s, resolved.descriptionSource.toVariantMap());
        } else if (kind == "credits"_L1 && payload.value(u"producers"_s).toList().isEmpty()
                   && !resolved.producers.isEmpty()) {
            payload.insert(u"producers"_s, producerData(resolved.producers).toVariantList());
        }
        value.insert(u"payload"_s, payload);
        return present(std::move(value));
    }
    QJsonObject state, payload, source;
    if (kind == "introduction"_L1) {
        state = entry->introduction;
        payload = { { u"description"_s, entry->description }, { u"introduction"_s, state } };
        source = entry->descriptionSource;
    } else if (kind == "credits"_L1) {
        state = entry->creditsLookup;
        payload = { { u"producers"_s, producerData(entry->producers) },
                    { u"creditsLookup"_s, state } };
        source = { { u"name"_s, state.value(u"source"_s) } };
    } else if (kind == "enrichment"_L1) {
        state = entry->online;
        payload = { { u"online"_s, state },
                    { u"genres"_s, entry->fields.value(u"_onlineGenres"_s) } };
        source = { { u"name"_s, u"MusicBrainz"_s }, { u"url"_s, state.value(u"sourceUrl"_s) } };
    } else {
        return { };
    }
    if (kind != "introduction"_L1 && !state.value(u"sourceDetails"_s).toObject().isEmpty())
        source = state.value(u"sourceDetails"_s).toObject();
    return present({ { u"albumId"_s, albumId },
                     { u"kind"_s, kind },
                     { u"status"_s, state.value(u"status"_s).toString(u"Deferred"_s) },
                     { u"payload"_s, payload.toVariantMap() },
                     { u"source"_s, source.toVariantMap() },
                     { u"checkedAt"_s, state.value(u"checkedAt"_s).toString() },
                     { u"retryAt"_s, state.value(u"retryAt"_s).toString() },
                     { u"error"_s, state.value(u"error"_s).toString() } });
}
Album LibrarySession::resolvedAlbum(const QString &albumId) const
{
    const auto *entry = album(albumId);
    if (!entry)
        return { };
    auto resolved = *entry;
    for (const auto kind :
         { MetadataKind::Introduction, MetadataKind::Credits, MetadataKind::Enrichment }) {
        const auto cached = m_metadata.constFind(albumId + '/'_L1 + kindName(kind));
        if (cached != m_metadata.cend())
            mergeMetadata(resolved, *cached);
    }
    return resolved;
}
void LibrarySession::adoptMetadataConfiguration(Snapshot snapshot)
{
    if (!snapshot)
        return;
    invalidate();
    auto updated = *m_snapshot;
    updated.fields.insert(u"metadataConfiguration"_s,
                          snapshot->fields.value(u"metadataConfiguration"_s));
    m_snapshot = std::make_shared<const LibrarySnapshot>(std::move(updated));
    ++m_metadataRevision;
    Q_EMIT metadataConfigurationChanged();
    Q_EMIT metadataChanged();
    Q_EMIT metadataProgressChanged();
}
void LibrarySession::adoptMetadataSnapshot(Snapshot snapshot)
{
    // Metadata commits preserve IDs, order, tracks and configuration. No navigation/model reset.
    if (!snapshot || snapshot->albums.size() != m_snapshot->albums.size())
        return;
    for (qsizetype i = 0; i < snapshot->albums.size(); ++i)
        if (snapshot->albums[i].id != m_snapshot->albums[i].id)
            return;
    if (snapshot->fields.value(u"metadataConfiguration"_s)
        != m_snapshot->fields.value(u"metadataConfiguration"_s))
        return;
    m_snapshot = std::move(snapshot);
    for (auto it = m_metadata.begin(); it != m_metadata.end();) {
        const auto &result = it.value();
        const auto *entry = album(result.albumId);
        const auto field = result.kind == MetadataKind::Introduction ? u"introduction"_s
                : result.kind == MetadataKind::Credits               ? u"creditsLookup"_s
                                                                     : u"online"_s;
        const auto state = !entry                           ? QJsonObject()
                : result.kind == MetadataKind::Introduction ? entry->introduction
                : result.kind == MetadataKind::Credits      ? entry->creditsLookup
                                                            : entry->online;
        if (!state.isEmpty() && state == result.payload.value(field).toObject())
            it = m_metadata.erase(it);
        else
            ++it;
    }
    m_albums.updateMetadataSnapshot(m_snapshot);
    m_search.updateMetadataSnapshot(m_snapshot);
    ++m_metadataRevision;
    Q_EMIT metadataChanged();
    if (auto *offline = qobject_cast<OfflineMetadataProvider *>(m_provider.data()))
        offline->setCache(m_snapshot);
}
const Album *LibrarySession::album(const QString &id) const
{
    const auto index = m_albumIndices.constFind(id);
    return index == m_albumIndices.cend() ? nullptr : &m_snapshot->albums[*index];
}
const Track *LibrarySession::track(const QString &albumId, const QString &trackId) const
{
    const auto index = m_albumIndices.constFind(albumId);
    const auto tracks = m_trackIndices.constFind(albumId);
    if (index == m_albumIndices.cend() || tracks == m_trackIndices.cend())
        return nullptr;
    const auto row = tracks->constFind(trackId);
    return row == tracks->cend() ? nullptr : &m_snapshot->albums[*index].tracks[*row];
}
}
