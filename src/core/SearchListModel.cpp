#include "LibraryModels.h"
#include "LibraryIndex.h"
#include <atomic>
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
struct SearchListModel::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SearchListModel *owner) : q(owner)
    {
    }
    SearchListModel *const q;

    struct SearchEntry final
    {
        SearchResult result;
        QString genreId, text;
        qsizetype nextAlbum = 0;
    };
    struct PendingSearch final
    {
        Q_DISABLE_COPY_MOVE(PendingSearch)
        PendingSearch() = default;
        QThreadPool worker;
        QFutureWatcher<QList<SearchResult>> completion;
        std::atomic<quint64> generation{ 0 };
        quint64 activeTicket = 0;
        QString needle, genreId;
        bool pending = false, running = false;
    };
    void startSearch();
    void replaceResults(QList<SearchResult> results);
    void notifyCount();
    static QList<SearchResult> filter(const QList<SearchEntry> &entries, const QString &needle,
                                      const QString &genreId,
                                      const std::atomic<quint64> &generation, quint64 ticket);
    PendingSearch m_search;
    LibraryIndex m_library;
    QList<SearchEntry> m_entries;
    QHash<QString, QString> m_genreNames;
    QList<SearchResult> m_results;
    int m_notifiedCount = 0;
};
SearchListModel::SearchListModel(QObject *parent)
    : QAbstractListModel(parent), d(std::make_unique<Private>(this))
{
    d->m_search.worker.setMaxThreadCount(1);
    connect(&d->m_search.completion, &QFutureWatcher<QList<SearchResult>>::finished, this, [this] {
        const bool wasSearching = searching();
        d->m_search.running = false;
        if (d->m_search.activeTicket == d->m_search.generation.load(std::memory_order_relaxed))
            d->replaceResults(d->m_search.completion.result());
        d->startSearch();
        if (wasSearching != searching())
            Q_EMIT searchingChanged();
    });
}
bool SearchListModel::searching() const
{
    return d->m_search.pending
            || (d->m_search.running
                && d->m_search.activeTicket
                        == d->m_search.generation.load(std::memory_order_relaxed));
}
SearchListModel::~SearchListModel()
{
    // Teardown cancels the scan before joining; workers never call the GUI or borrow its entries.
    ++d->m_search.generation;
    d->m_search.worker.waitForDone();
}
QList<SearchResult> SearchListModel::Private::filter(const QList<Private::SearchEntry> &entries,
                                                     const QString &needle, const QString &genreId,
                                                     const std::atomic<quint64> &generation,
                                                     quint64 ticket)
{
    QList<SearchResult> results;
    const bool albumsOnly = needle.isEmpty();
    for (qsizetype i = 0; i < entries.size();) {
        if (generation.load(std::memory_order_relaxed) != ticket)
            return { };
        const auto &entry = entries[i];
        // Album headers skip all tracks when either the query or genre excludes them.
        // Track entries are reached only inside an admitted album.
        if (!genreId.isEmpty() && entry.genreId != genreId) {
            i = entry.nextAlbum;
            continue;
        }
        i = albumsOnly ? entry.nextAlbum : i + 1;
        if (albumsOnly || entry.text.contains(needle))
            results.append(entry.result);
    }
    return results;
}
void SearchListModel::request(const QString &query, const QString &genreId)
{
    const auto needle = query.trimmed().toLower();
    if (needle == d->m_search.needle && genreId == d->m_search.genreId)
        return;
    const bool wasSearching = searching();
    d->m_search.needle = needle;
    d->m_search.genreId = genreId;
    ++d->m_search.generation;
    d->m_search.pending = true;
    d->startSearch();
    if (wasSearching != searching())
        Q_EMIT searchingChanged();
}
void SearchListModel::Private::startSearch()
{
    if (!m_search.pending || m_search.running)
        return;
    m_search.pending = false;
    m_search.running = true;
    const auto ticket = m_search.generation.load(std::memory_order_relaxed);
    m_search.activeTicket = ticket;
    // Qt containers share immutable storage. A replacement detaches on the GUI;
    // one running scan and one latest query bound both work and completion delivery.
    m_search.completion.setFuture(QtConcurrent::run(
            &m_search.worker,
            [entries = m_entries, needle = m_search.needle, genreId = m_search.genreId,
             generation = &m_search.generation, ticket] {
                return filter(entries, needle, genreId, *generation, ticket);
            }));
}
int SearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(d->m_results.size());
}
QHash<int, QByteArray> SearchListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Kind, "kind"_ba },        { AlbumId, "albumId"_ba }, { TrackId, "trackId"_ba },
        { Title, "title"_ba },      { Artist, "artist"_ba },   { CoverUrl, "coverUrl"_ba },
        { Subtitle, "subtitle"_ba }
    };
    return roles;
}
QVariant SearchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &result = d->m_results[index.row()];
    const auto *entry = d->m_library.album(result.albumId);
    if (!entry)
        return { };
    const auto &album = *entry;
    const auto *track = d->m_library.track(result.albumId, result.trackId);
    switch (role) {
    case Kind:
        return track ? u"track"_s : u"album"_s;
    case AlbumId:
        return result.albumId;
    case TrackId:
        return result.trackId;
    case Title:
        return track ? track->title : album.title;
    case Artist:
        return artistDisplayName(track ? track->artist : album.artist);
    case CoverUrl:
        return album.coverUrl;
    case Subtitle:
        if (track)
            return album.title;
        return d->m_genreNames.value(album.genreId,
                                     QCoreApplication::translate("Yalahoo", "Unclassified"));
    default:
        return { };
    }
}
QVariantMap SearchListModel::resultAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return { };
    return { { u"albumId"_s, d->m_results[row].albumId },
             { u"trackId"_s, d->m_results[row].trackId } };
}
void SearchListModel::update(Snapshot snapshot, const QString &query, const QString &genreId)
{
    if (!snapshot)
        snapshot = std::make_shared<const LibrarySnapshot>();
    const bool replaced = d->m_library.snapshot() != snapshot;
    const bool wasSearching = searching();
    ++d->m_search.generation;
    d->m_search.pending = false;
    d->m_search.needle = query.trimmed().toLower();
    d->m_search.genreId = genreId;
    if (replaced) {
        beginResetModel();
        d->m_library.replace(std::move(snapshot));
        d->m_entries.clear();
        d->m_genreNames.clear();
        d->m_genreNames.reserve(d->m_library.snapshot()->genres.size());
        qsizetype entryCount = d->m_library.snapshot()->albums.size();
        for (const auto &album : d->m_library.snapshot()->albums)
            entryCount += album.tracks.size();
        d->m_entries.reserve(entryCount);
        for (const auto &genre : d->m_library.snapshot()->genres)
            d->m_genreNames.insert(genre.id, genreDisplayName(genre));
        for (qsizetype i = 0; i < d->m_library.snapshot()->albums.size(); ++i) {
            const auto &album = d->m_library.snapshot()->albums[i];
            if (d->m_library.album(album.id) != &album)
                continue;
            const QString genre = d->m_genreNames.value(
                    album.genreId, QCoreApplication::translate("Yalahoo", "Unclassified"));
            const auto albumEntry = d->m_entries.size();
            d->m_entries.append({ { album.id, { } },
                                  album.genreId,
                                  (album.title + ' '_L1 + album.artist + ' '_L1 + genre).toLower(),
                                  0 });
            for (const auto &track : album.tracks)
                d->m_entries.append({ { album.id, track.id },
                                      album.genreId,
                                      (track.title + ' '_L1 + track.artist).toLower(),
                                      0 });
            d->m_entries[albumEntry].nextAlbum = d->m_entries.size();
        }
    }
    auto results = d->filter(d->m_entries, d->m_search.needle, genreId, d->m_search.generation,
                             d->m_search.generation.load(std::memory_order_relaxed));
    if (replaced) {
        d->m_results = std::move(results);
        endResetModel();
        d->notifyCount();
        if (wasSearching != searching())
            Q_EMIT searchingChanged();
        return;
    }
    d->replaceResults(std::move(results));
    if (wasSearching != searching())
        Q_EMIT searchingChanged();
}
void SearchListModel::Private::replaceResults(QList<SearchResult> results)
{
    const auto ticket = m_search.generation.load(std::memory_order_relaxed);
    int prefix = 0;
    const auto &before = std::as_const(m_results);
    const auto &after = std::as_const(results);
    while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix])
        ++prefix;
    int suffix = 0;
    while (suffix < before.size() - prefix && suffix < after.size() - prefix
           && before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix])
        ++suffix;
    const int removed = int(m_results.size()) - prefix - suffix;
    const int inserted = int(results.size()) - prefix - suffix;
    const int reused = std::min(removed, inserted);
    for (int i = 0; i < reused; ++i)
        m_results[prefix + i] = after[prefix + i];
    // Synchronous observers can replace the snapshot or request another query.
    // Already-notified edits are valid, but no stale edit may follow that callback.
    if (reused)
        Q_EMIT q->dataChanged(q->index(prefix), q->index(prefix + reused - 1));
    if (ticket != m_search.generation.load(std::memory_order_relaxed)) {
        notifyCount();
        return;
    }
    if (removed > reused) {
        q->beginRemoveRows({ }, prefix + reused, prefix + removed - 1);
        m_results.remove(prefix + reused, removed - reused);
        q->endRemoveRows();
        if (ticket != m_search.generation.load(std::memory_order_relaxed)) {
            notifyCount();
            return;
        }
    }
    if (inserted > reused) {
        q->beginInsertRows({ }, prefix + reused, prefix + inserted - 1);
        m_results = std::move(results);
        q->endInsertRows();
        if (ticket != m_search.generation.load(std::memory_order_relaxed)) {
            notifyCount();
            return;
        }
    }
    notifyCount();
}
void SearchListModel::Private::notifyCount()
{
    const int count = q->rowCount();
    if (count != m_notifiedCount) {
        m_notifiedCount = count;
        Q_EMIT q->countChanged();
    }
}
void SearchListModel::updateMetadataSnapshot(Snapshot snapshot)
{
    if (!d->m_library.replaceMetadata(std::move(snapshot)))
        return;
}

}
