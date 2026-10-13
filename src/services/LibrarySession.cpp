#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include <utility>
#include <QThread>

namespace yalahoo {
LibrarySession::LibrarySession(IMetadataProvider *provider, QObject *parent)
    : QObject(parent),
      m_provider(provider),
      m_albums(this),
      m_search(this),
      m_snapshot(std::make_shared<const LibrarySnapshot>())
{
    qRegisterMetaType<ProviderResult>();
    if (provider) {
        connect(provider, &IMetadataProvider::resultReady, this, &LibrarySession::receive,
                Qt::QueuedConnection);
        connect(provider, &QObject::destroyed, this, [this] {
            m_pending.clear();
            emit providerCapabilitiesChanged();
        });
    }
}
LibrarySession::~LibrarySession()
{
    invalidate();
}
void LibrarySession::invalidate()
{
    const auto pending = std::exchange(m_pending, { });
    ++m_generation;
    if (m_provider)
        for (const auto &request : pending)
            m_provider->cancel(request.token);
}
void LibrarySession::setSnapshot(LibrarySnapshot snapshot)
{
    Q_ASSERT(QThread::currentThread() == thread());
    invalidate();
    m_snapshot = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    m_albumIndices.clear();
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
    emit metadataChanged();
}
void LibrarySession::search(const QString &query, const QString &genreId)
{
    m_query = query;
    m_genre = genreId;
    m_search.update(m_snapshot, query, genreId);
}
void LibrarySession::sort(const QString &mode)
{
    if (mode == QLatin1String("genre"))
        m_albums.setSortMode(SortMode::Genre);
    else if (mode == QLatin1String("artist"))
        m_albums.setSortMode(SortMode::Artist);
    else if (mode == QLatin1String("album"))
        m_albums.setSortMode(SortMode::Album);
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
    emit navigationChanged();
    return true;
}
bool LibrarySession::setPlaybackIds(const QString &albumId, const QString &trackId)
{
    if (!(albumId.isEmpty() && trackId.isEmpty()) && !track(albumId, trackId))
        return false;
    if (m_playbackAlbumId == albumId && m_playbackTrackId == trackId)
        return true;
    m_playbackAlbumId = albumId;
    m_playbackTrackId = trackId;
    emit playbackChanged();
    return true;
}
QVariantMap LibrarySession::providerCapabilities() const
{
    const auto capabilities = m_provider
            ? m_provider->capabilities()
            : ProviderCapabilities{ false, false, false, false, false };
    return { { "networkEnabled", capabilities.networkEnabled },
             { "introductions", capabilities.introductions },
             { "credits", capabilities.credits },
             { "enrichment", capabilities.enrichment },
             { "batch", capabilities.batch } };
}
quint64 LibrarySession::requestMetadata(const QString &albumId, const QString &name)
{
    const auto *album = this->album(albumId);
    if (!album || !m_provider)
        return 0;
    MetadataKind kind;
    if (name == QLatin1String("introduction"))
        kind = MetadataKind::Introduction;
    else if (name == QLatin1String("credits"))
        kind = MetadataKind::Credits;
    else if (name == QLatin1String("enrichment"))
        kind = MetadataKind::Enrichment;
    else
        return 0;
    const auto requests = m_pending;
    for (const auto &request : requests)
        if (request.token.albumId == albumId && request.kind == kind)
            cancelMetadata(request.token.requestId);
    const RequestToken token{ ++m_requestId, m_generation, albumId };
    m_pending.insert(token.requestId, { token, kind });
    AlbumQuery query;
    query.albumId = albumId;
    query.title = album->title;
    query.artist = album->artist;
    query.albumIds = { albumId };
    for (const auto &track : album->tracks)
        query.trackIds.append(track.id);
    switch (kind) {
    case MetadataKind::Introduction:
        m_provider->requestIntroduction(query, token);
        break;
    case MetadataKind::Credits:
        m_provider->requestCredits(query, token);
        break;
    case MetadataKind::Enrichment:
        m_provider->requestEnrichment(query, token);
        break;
    }
    return token.requestId;
}
void LibrarySession::cancelMetadata(quint64 requestId)
{
    const auto it = m_pending.find(requestId);
    if (it == m_pending.end())
        return;
    const auto token = it->token;
    m_pending.erase(it);
    if (m_provider)
        m_provider->cancel(token);
}
void LibrarySession::receive(const ProviderResult &result)
{
    const auto it = m_pending.find(result.requestId);
    if (it == m_pending.end() || result.generation != m_generation
        || result.generation != it->token.generation || result.albumId != it->token.albumId
        || result.kind != it->kind || !album(result.albumId))
        return;
    m_pending.erase(it);
    m_metadata.insert(result.albumId + '/' + kindName(result.kind),
                      QVariantMap{ { "requestId", QVariant::fromValue(result.requestId) },
                                   { "generation", QVariant::fromValue(result.generation) },
                                   { "albumId", result.albumId },
                                   { "kind", kindName(result.kind) },
                                   { "status", statusName(result.status) },
                                   { "payload", result.payload.toVariantMap() },
                                   { "source", result.source.toVariantMap() },
                                   { "checkedAt", result.checkedAt },
                                   { "retryAt", result.retryAt },
                                   { "error", result.error } });
    emit metadataChanged();
    emit metadataDelivered(result);
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
