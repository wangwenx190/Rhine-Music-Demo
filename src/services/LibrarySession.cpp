#include "LibrarySession.h"
#include "LibrarySession_p.h"
#include <QCoreApplication>
#include <utility>
#include <algorithm>
#include <QThread>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {

LibrarySession::LibrarySession(IMetadataProvider *provider, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, provider))
{
    connect(&d->m_albums, &AlbumListModel::columnsChanged, this, [this] {
        d->rebuildBrowsePositions();
    });
    connect(&d->m_metadata, &MetadataCoordinator::changed, this, &LibrarySession::metadataChanged);
    connect(&d->m_metadata, &MetadataCoordinator::progressChanged, this,
            &LibrarySession::metadataProgressChanged);
    connect(&d->m_metadata, &MetadataCoordinator::capabilitiesChanged, this,
            &LibrarySession::providerCapabilitiesChanged);
    connect(&d->m_metadata, &MetadataCoordinator::albumChanged, this,
            &LibrarySession::albumMetadataChanged);
    connect(&d->m_metadata, &MetadataCoordinator::delivered, this,
            &LibrarySession::metadataDelivered);
    d->rebuildSummary();
}
QVariantList LibrarySession::genres() const
{
    return d->m_genres;
}
bool LibrarySession::hasGenre(const QString &id) const
{
    return d->m_visibleGenres.contains(id);
}
QVariantMap LibrarySession::summary() const
{
    return d->m_summary;
}
void LibrarySession::Private::rebuildSummary()
{
    int tracks = 0;
    QStringList offline;
    for (const auto &entry : m_index.snapshot()->albums)
        tracks += int(entry.tracks.size());
    for (const auto &root : m_index.snapshot()->roots)
        if (root.status == "offline"_L1)
            offline.append(
                    root.path.toLocalFile()
                    + QCoreApplication::translate(
                            "Yalahoo", " Temporarily offline. The original index was retained."));
    m_summary = { { u"albums"_s, m_index.snapshot()->albums.size() },
                  { u"tracks"_s, tracks },
                  { u"genres"_s, q->genres().size() },
                  { u"offline"_s, offline.join('\n'_L1) } };
}
int LibrarySession::trackIndex(const QString &albumId, const QString &trackId) const
{
    return d->m_index.trackIndex(albumId, trackId);
}
bool LibrarySession::contains(const QString &albumId, const QString &trackId) const
{
    return d->m_index.album(albumId) && (trackId.isEmpty() || d->m_index.track(albumId, trackId));
}
LibrarySession::~LibrarySession()
{
    d->m_metadata.shutdown();
}
void LibrarySession::setSnapshot(LibrarySnapshot snapshot)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!d->m_metadata.invalidate())
        return;
    d->m_index.replace(std::make_shared<const LibrarySnapshot>(std::move(snapshot)));
    d->m_genres.clear();
    d->m_visibleGenres.clear();
    d->m_visibleGenres.reserve(d->m_index.snapshot()->genres.size());
    d->m_genres.reserve(d->m_index.snapshot()->genres.size());
    QSet<QString> usedGenres;
    usedGenres.reserve(d->m_index.snapshot()->genres.size());
    for (const auto &entry : d->m_index.snapshot()->albums)
        usedGenres.insert(entry.genreId);
    for (const auto &genre : d->m_index.snapshot()->genres)
        if (usedGenres.contains(genre.id)) {
            d->m_visibleGenres.insert(genre.id);
            d->m_genres.append(
                    QVariantMap{ { u"id"_s, genre.id }, { u"name"_s, genreDisplayName(genre) } });
        }
    d->rebuildSummary();
    d->m_genreNames.clear();
    d->m_genreNames.reserve(d->m_index.snapshot()->genres.size());
    for (const auto &genre : d->m_index.snapshot()->genres)
        d->m_genreNames.insert(genre.id, genreDisplayName(genre));
    d->m_albums.setSnapshot(d->m_index.snapshot());
    d->m_search.update(d->m_index.snapshot(), d->m_query, d->m_genre);
    if (!d->m_navigationAlbumId.isEmpty()
        && (!d->m_index.album(d->m_navigationAlbumId)
            || (!d->m_navigationTrackId.isEmpty()
                && !d->m_index.track(d->m_navigationAlbumId, d->m_navigationTrackId))))
        navigate({ }, { });
    // Playback IDs survive a library replacement: playing media may outlive its index.
    d->m_metadata.libraryReplaced();
    Q_EMIT browseInfoChanged();
    Q_EMIT snapshotChanged();
    Q_EMIT metadataConfigurationChanged();
}
void LibrarySession::search(const QString &query, const QString &genreId)
{
    d->m_query = query;
    d->m_genre = genreId;
    d->m_search.request(query, genreId);
}
void LibrarySession::sort(const QString &mode)
{
    if (mode == "genre"_L1)
        d->m_albums.setSortMode(SortMode::Genre);
    else if (mode == "artist"_L1)
        d->m_albums.setSortMode(SortMode::Artist);
    else if (mode == "album"_L1)
        d->m_albums.setSortMode(SortMode::Album);
    Q_EMIT browseInfoChanged();
}
void LibrarySession::Private::rebuildBrowsePositions()
{
    m_browsePositions.clear();
    const auto &order = m_albums.order();
    m_browsePositions.reserve(order.indices.size());
    for (int i = 0; i < order.indices.size(); ++i)
        m_browsePositions[m_index.snapshot()->albums[order.indices[i]].id].code = i + 1;
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
    if (!albumId.isEmpty()
        && (!d->m_index.album(albumId)
            || (!trackId.isEmpty() && !d->m_index.track(albumId, trackId))))
        return false;
    if (d->m_navigationAlbumId == albumId && d->m_navigationTrackId == trackId)
        return true;
    d->m_navigationAlbumId = albumId;
    d->m_navigationTrackId = trackId;
    Q_EMIT navigationChanged();
    Q_EMIT browseInfoChanged();
    return true;
}
QVariantMap LibrarySession::browseInfo() const
{
    const auto *selected = d->m_index.album(d->m_navigationAlbumId);
    if (!selected)
        return { };
    const QString genre = d->m_genreNames.value(selected->genreId);
    const auto position = d->m_browsePositions.value(selected->id);
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
             { u"columnTotal"_s, d->m_albums.order().columns.size() },
             { u"columnLabel"_s, position.label } };
}
bool LibrarySession::setPlaybackIds(const QString &albumId, const QString &trackId)
{
    if (!(albumId.isEmpty() && trackId.isEmpty()) && !d->m_index.track(albumId, trackId))
        return false;
    d->adoptPlaybackIds(albumId, trackId);
    return true;
}
void LibrarySession::Private::adoptPlaybackIds(const QString &albumId, const QString &trackId)
{
    if (m_playbackAlbumId == albumId && m_playbackTrackId == trackId)
        return;
    m_playbackAlbumId = albumId;
    m_playbackTrackId = trackId;
    Q_EMIT q->playbackChanged();
}
void LibrarySession::adoptMetadataConfiguration(const Snapshot &snapshot)
{
    if (!snapshot)
        return;
    // Provider cancellation may reenter the session. Retain only the configuration
    // value before callbacks rather than borrowing the caller's snapshot through them.
    const auto configuration = snapshot->fields.value(u"metadataConfiguration"_s);
    if (!d->m_metadata.invalidate())
        return;
    d->m_index.setMetadataConfiguration(configuration);
    d->m_metadata.configurationChanged();
    Q_EMIT metadataConfigurationChanged();
}
void LibrarySession::adoptMetadataSnapshot(Snapshot snapshot)
{
    if (!snapshot
        || snapshot->fields.value(u"metadataConfiguration"_s)
                != d->m_index.snapshot()->fields.value(u"metadataConfiguration"_s)
        || !d->m_index.replaceMetadata(std::move(snapshot)))
        return;
    const auto updated = d->m_index.snapshot();
    d->m_albums.updateMetadataSnapshot(updated);
    // A model observer may replace the library during dataChanged.
    if (d->m_index.snapshot() != updated)
        return;
    d->m_search.updateMetadataSnapshot(updated);
    d->m_metadata.metadataCommitted();
}
Snapshot LibrarySession::snapshot() const
{
    return d->m_index.snapshot();
}
QString LibrarySession::navigationAlbumId() const
{
    return d->m_navigationAlbumId;
}
QString LibrarySession::navigationTrackId() const
{
    return d->m_navigationTrackId;
}
QString LibrarySession::playbackAlbumId() const
{
    return d->m_playbackAlbumId;
}
QString LibrarySession::playbackTrackId() const
{
    return d->m_playbackTrackId;
}
const Album *LibrarySession::album(const QString &id) const
{
    return d->m_index.album(id);
}
const Album *LibrarySession::navigationAlbum() const
{
    return d->m_index.album(d->m_navigationAlbumId);
}
SearchListModel *LibrarySession::searchResults()
{
    return &d->m_search;
}
AlbumListModel *LibrarySession::albums()
{
    return &d->m_albums;
}

QVariantMap LibrarySession::metadata() const
{
    return d->m_metadata.metadata();
}

QVariantMap LibrarySession::providerCapabilities() const
{
    return d->m_metadata.capabilities();
}

quint64 LibrarySession::requestMetadata(const QString &albumId, const QString &name, bool force,
                                        const QString &releaseId)
{
    return d->m_metadata.request(albumId, name, force, releaseId);
}

void LibrarySession::cancelMetadata(quint64 requestId)
{
    d->m_metadata.cancel(requestId);
}

QVariantMap LibrarySession::metadataConfiguration() const
{
    return d->m_metadata.configuration();
}

QVariantMap LibrarySession::metadataProgress() const
{
    return d->m_metadata.progress();
}

void LibrarySession::cancelAllMetadata()
{
    d->m_metadata.cancelAll();
}

bool LibrarySession::requestMetadataBatch(const QString &kind, bool missingOnly)
{
    return d->m_metadata.requestBatch(kind, missingOnly);
}

QVariantMap LibrarySession::metadataFor(const QString &albumId, const QString &kind,
                                        quint64 revision) const
{
    return d->m_metadata.forAlbum(albumId, kind, revision);
}

Album LibrarySession::resolvedAlbum(const QString &albumId) const
{
    return d->m_metadata.resolvedAlbum(albumId);
}

bool LibrarySession::metadataBusy() const
{
    return d->m_metadata.busy();
}

quint64 LibrarySession::metadataRevision() const
{
    return d->m_metadata.revision();
}

}
