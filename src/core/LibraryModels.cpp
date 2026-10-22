#include "Units.h"
#include <QCoreApplication>
#include "LibraryModels.h"
#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrentRun>

namespace {
// Column partitioning follows the source album grouping; ordinal labels are decimal.
constexpr auto AlbumsPerColumn = 12;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
QString genreDisplayName(const Genre &genre)
{
    return genre.name.isEmpty() ? QCoreApplication::translate("Yalahoo", "Unclassified")
                                : genre.name;
}
QString artistDisplayName(const QString &artist)
{
    return artist.isEmpty() ? QCoreApplication::translate("LibraryPresentation", "Unknown artist")
                            : artist;
}
QString normalizedKey(const QString &value)
{
    return value.normalized(QString::NormalizationForm_KC).simplified().toLower();
}
void appendGenreColumns(LibraryOrder &result, const LibrarySnapshot &snapshot)
{
    QSet<QString> used;
    used.reserve(std::min(snapshot.genres.size(), snapshot.albums.size()));
    for (const auto &album : snapshot.albums)
        used.insert(album.genreId);
    result.columns.reserve(used.size());

    for (const auto &genre : snapshot.genres) {
        if (used.remove(genre.id))
            result.columns.append({ genre.id, genreDisplayName(genre), { } });
    }
    for (const auto &album : snapshot.albums)
        if (used.remove(album.genreId))
            result.columns.append({ album.genreId, album.genreId, { } });
}
LibraryOrder orderLibrary(const LibrarySnapshot &snapshot, SortMode mode)
{
    LibraryOrder result;
    result.indices.resize(snapshot.albums.size());
    std::iota(result.indices.begin(), result.indices.end(), 0);
    QStringList titles, artists;
    if (mode != SortMode::Genre) {
        QCollator collator(QLocale(u"zh_CN"_s));
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        QList<QCollatorSortKey> titleKeys, artistKeys;
        titleKeys.reserve(snapshot.albums.size());
        if (mode == SortMode::Artist)
            artistKeys.reserve(snapshot.albums.size());
        titles.reserve(snapshot.albums.size());
        if (mode == SortMode::Artist)
            artists.reserve(snapshot.albums.size());
        for (const auto &album : snapshot.albums) {
            titles.append(normalizedKey(album.title));
            titleKeys.append(collator.sortKey(titles.last()));
            if (mode == SortMode::Artist) {
                artists.append(normalizedKey(album.artist));
                artistKeys.append(collator.sortKey(artists.last()));
            }
        }
        std::stable_sort(result.indices.begin(), result.indices.end(), [&](int a, int b) {
            const auto &left = snapshot.albums[a];
            const auto &right = snapshot.albums[b];
            if (mode == SortMode::Artist) {
                const int locale = artistKeys[a].compare(artistKeys[b]);
                const int artist = locale ? locale : QString::compare(artists[a], artists[b]);
                if (artist)
                    return artist < 0;
            }
            const int locale = titleKeys[a].compare(titleKeys[b]);
            const int title = locale ? locale : QString::compare(titles[a], titles[b]);
            return title ? title < 0 : left.id < right.id;
        });
    }
    if (mode == SortMode::Album) {
        result.columns.reserve((result.indices.size() + AlbumsPerColumn - 1) / AlbumsPerColumn);
        for (int start = 0; start < result.indices.size(); start += AlbumsPerColumn) {
            const int end = std::min(start + AlbumsPerColumn, int(result.indices.size()));
            LibraryColumn column{ u"album:%1"_s.arg(start / AlbumsPerColumn),
                                  u"%1\u2013%2"_s
                                          .arg(start + 1, 2, yalahoo::units::DecimalBase, '0'_L1)
                                          .arg(end, 2, yalahoo::units::DecimalBase, '0'_L1),
                                  { } };
            column.albumIds.reserve(end - start);
            for (int i = start; i < end; ++i)
                column.albumIds.append(snapshot.albums[result.indices[i]].id);
            result.columns.append(std::move(column));
        }
    } else {
        if (mode == SortMode::Genre)
            appendGenreColumns(result, snapshot);
        QHash<QString, qsizetype> columnByKey;
        columnByKey.reserve(result.columns.size());
        for (qsizetype i = 0; i < result.columns.size(); ++i)
            columnByKey.insert(result.columns[i].key, i);
        for (int index : result.indices) {
            const auto &album = snapshot.albums[index];
            const QString key = mode == SortMode::Artist ? artists[index] : album.genreId;
            const auto column = columnByKey.tryEmplace(key, result.columns.size());
            if (column.inserted) {
                const QString label = album.artist.simplified();
                result.columns.append(
                        { key,
                          label.isEmpty() ? QCoreApplication::translate("Yalahoo", "Unknown artist")
                                          : label,
                          { album.id } });
            } else
                result.columns[*column.iterator].albumIds.append(album.id);
        }
    }
    for (const auto &column : result.columns)
        result.slotStride =
                std::max(result.slotStride, int(column.albumIds.size()) + AlbumsPerColumn);
    return result;
}
QList<SearchResult> searchLibrary(const LibrarySnapshot &snapshot, const QString &input,
                                  const QString &genreId)
{
    // Deliberately no NFKC / whitespace folding: music-search.ts uses trim + lowercase.
    const QString query = input.trimmed().toLower();
    QHash<QString, QString> names;
    names.reserve(snapshot.genres.size());
    for (const auto &genre : snapshot.genres)
        names.insert(genre.id, genreDisplayName(genre));
    QList<SearchResult> results;
    if (query.isEmpty() && genreId.isEmpty())
        results.reserve(snapshot.albums.size());
    for (const auto &album : snapshot.albums) {
        if (!genreId.isEmpty() && album.genreId != genreId)
            continue;
        QString genre = names.value(album.genreId);
        if (genre.isEmpty())
            genre = QCoreApplication::translate("Yalahoo", "Unclassified");
        if (query.isEmpty()
            || (album.title + ' '_L1 + album.artist + ' '_L1 + genre).toLower().contains(query))
            results.append({ album.id, { } });
        if (query.isEmpty())
            continue;
        for (const auto &track : album.tracks)
            if ((track.title + ' '_L1 + track.artist).toLower().contains(query))
                results.append({ album.id, track.id });
    }
    return results;
}
AlbumListModel::AlbumListModel(QObject *parent)
    : QAbstractListModel(parent), m_snapshot(std::make_shared<const LibrarySnapshot>())
{
}
int AlbumListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_order.indices.size());
}
QHash<int, QByteArray> AlbumListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = { { AlbumId, "albumId"_ba },
                                                  { Title, "title"_ba },
                                                  { Artist, "artist"_ba },
                                                  { Year, "year"_ba },
                                                  { GenreId, "genreId"_ba },
                                                  { CoverUrl, "coverUrl"_ba },
                                                  { Folder, "folder"_ba },
                                                  { Tracks, "tracks"_ba },
                                                  { Producers, "producers"_ba },
                                                  { Description, "description"_ba },
                                                  { DescriptionSource, "descriptionSource"_ba },
                                                  { Introduction, "introduction"_ba },
                                                  { CreditsLookup, "creditsLookup"_ba },
                                                  { Online, "online"_ba },
                                                  { LocalNote, "localNote"_ba },
                                                  { Offline, "offline"_ba },
                                                  { AlbumData, "albumData"_ba } };
    return roles;
}
QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &album = m_snapshot->albums[m_order.indices[index.row()]];
    struct RoleReader final
    {
        int role;
        QVariant (*read)(const Album &album);
    };
    static constexpr RoleReader readers[] = {
        { AlbumId,
          [](const Album &album) -> QVariant {
              return album.id;
          } },
        { Title,
          [](const Album &album) -> QVariant {
              return album.title;
          } },
        { Artist,
          [](const Album &album) -> QVariant {
              return artistDisplayName(album.artist);
          } },
        { Year,
          [](const Album &album) -> QVariant {
              return album.year ? QVariant(*album.year) : QVariant{ };
          } },
        { GenreId,
          [](const Album &album) -> QVariant {
              return album.genreId;
          } },
        { CoverUrl,
          [](const Album &album) -> QVariant {
              return album.coverUrl;
          } },
        { Folder,
          [](const Album &album) -> QVariant {
              return album.folder;
          } },
        { Tracks,
          [](const Album &album) -> QVariant {
              QVariantList tracks;
              tracks.reserve(album.tracks.size());
              for (const auto &track : album.tracks) {
                  auto map = toJson(track).toVariantMap();
                  map.insert(u"sourceUrl"_s, track.sourceUrl);
                  tracks.append(std::move(map));
              }
              return tracks;
          } },
        { Producers,
          [](const Album &album) -> QVariant {
              return producerData(album.producers).toVariantList();
          } },
        { Description,
          [](const Album &album) -> QVariant {
              return album.description;
          } },
        { DescriptionSource,
          [](const Album &album) -> QVariant {
              return album.descriptionSource.toVariantMap();
          } },
        { Introduction,
          [](const Album &album) -> QVariant {
              return album.introduction.toVariantMap();
          } },
        { CreditsLookup,
          [](const Album &album) -> QVariant {
              return album.creditsLookup.toVariantMap();
          } },
        { Online,
          [](const Album &album) -> QVariant {
              return album.online.toVariantMap();
          } },
        { LocalNote,
          [](const Album &album) -> QVariant {
              return album.localNote;
          } },
        { Offline,
          [](const Album &album) -> QVariant {
              return album.offline;
          } },
        { AlbumData,
          [](const Album &album) -> QVariant {
              return toJson(album).toVariantMap();
          } }
    };
    static_assert([] {
        int next = AlbumId;
        for (const auto &reader : readers) {
            if (reader.role != next++)
                return false;
        }
        return next == AlbumData + 1;
    }());
    if (role < AlbumId || role > AlbumData)
        return { };
    return readers[role - AlbumId].read(album);
}
void AlbumListModel::setSnapshot(Snapshot snapshot)
{
    const int previousCount = rowCount();
    beginResetModel();
    m_snapshot = snapshot ? std::move(snapshot) : std::make_shared<const LibrarySnapshot>();
    m_order = orderLibrary(*m_snapshot, m_mode);
    endResetModel();
    if (previousCount != rowCount())
        Q_EMIT countChanged();
    Q_EMIT columnsChanged();
}
void AlbumListModel::updateMetadataSnapshot(Snapshot snapshot)
{
    if (!snapshot || snapshot->albums.size() != m_snapshot->albums.size())
        return;
    const auto previous = m_snapshot;
    m_snapshot = std::move(snapshot);
    const auto updated = m_snapshot;
    const auto indices = m_order.indices;
    for (int row = 0; row < indices.size(); ++row) {
        const auto position = indices[row];
        const auto &before = previous->albums[position];
        const auto &after = updated->albums[position];
        if (before.introduction != after.introduction || before.creditsLookup != after.creditsLookup
            || before.online != after.online)
            Q_EMIT dataChanged(index(row), index(row),
                               { Producers, Description, DescriptionSource, Introduction,
                                 CreditsLookup, Online, AlbumData });
        if (m_snapshot != updated)
            return;
    }
}
void AlbumListModel::setSortMode(SortMode mode)
{
    if (m_mode == mode)
        return;
    beginResetModel();
    m_mode = mode;
    m_order = orderLibrary(*m_snapshot, mode);
    endResetModel();
    Q_EMIT columnsChanged();
}
QVariantList AlbumListModel::columns() const
{
    QVariantList result;
    result.reserve(m_order.columns.size());
    int lane = 0;
    for (const auto &column : m_order.columns)
        result.append(QVariantMap{ { u"key"_s, column.key },
                                   { u"label"_s, column.label },
                                   { u"albumIds"_s, column.albumIds },
                                   { u"lane"_s, lane++ } });
    return result;
}
struct SearchListModel::PendingSearch final
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
SearchListModel::SearchListModel(QObject *parent)
    : QAbstractListModel(parent),
      m_search(std::make_unique<PendingSearch>()),
      m_snapshot(std::make_shared<const LibrarySnapshot>())
{
    m_search->worker.setMaxThreadCount(1);
    connect(&m_search->completion, &QFutureWatcher<QList<SearchResult>>::finished, this, [this] {
        const bool wasSearching = searching();
        m_search->running = false;
        if (m_search->activeTicket == m_search->generation.load(std::memory_order_relaxed))
            replaceResults(m_search->completion.result());
        startSearch();
        if (wasSearching != searching())
            Q_EMIT searchingChanged();
    });
}
bool SearchListModel::searching() const
{
    return m_search->pending
            || (m_search->running
                && m_search->activeTicket == m_search->generation.load(std::memory_order_relaxed));
}
SearchListModel::~SearchListModel()
{
    // Teardown cancels the scan before joining; workers never call the GUI or borrow its entries.
    ++m_search->generation;
    m_search->worker.waitForDone();
}
QList<SearchResult> SearchListModel::filter(const QList<SearchEntry> &entries,
                                            const QString &needle, const QString &genreId,
                                            const std::atomic<quint64> &generation, quint64 ticket)
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
    if (needle == m_search->needle && genreId == m_search->genreId)
        return;
    const bool wasSearching = searching();
    m_search->needle = needle;
    m_search->genreId = genreId;
    ++m_search->generation;
    m_search->pending = true;
    startSearch();
    if (wasSearching != searching())
        Q_EMIT searchingChanged();
}
void SearchListModel::startSearch()
{
    if (!m_search->pending || m_search->running)
        return;
    m_search->pending = false;
    m_search->running = true;
    const auto ticket = m_search->generation.load(std::memory_order_relaxed);
    m_search->activeTicket = ticket;
    // Qt containers share immutable storage. A replacement detaches on the GUI;
    // one running scan and one latest query bound both work and completion delivery.
    m_search->completion.setFuture(QtConcurrent::run(
            &m_search->worker,
            [entries = m_entries, needle = m_search->needle, genreId = m_search->genreId,
             generation = &m_search->generation, ticket] {
                return filter(entries, needle, genreId, *generation, ticket);
            }));
}
int SearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_results.size());
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
    const auto &result = m_results[index.row()];
    const auto albumIndex = m_albumIndices.constFind(result.albumId);
    if (albumIndex == m_albumIndices.cend())
        return { };
    const auto &album = m_snapshot->albums[albumIndex.value()];
    const auto tracks = m_trackIndices.constFind(result.albumId);
    const auto trackIndex =
            tracks == m_trackIndices.cend() ? -1 : tracks->value(result.trackId, -1);
    const auto *track = trackIndex < 0 ? nullptr : &album.tracks[trackIndex];
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
        return m_genreNames.value(album.genreId,
                                  QCoreApplication::translate("Yalahoo", "Unclassified"));
    default:
        return { };
    }
}
QVariantMap SearchListModel::resultAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return { };
    return { { u"albumId"_s, m_results[row].albumId }, { u"trackId"_s, m_results[row].trackId } };
}
void SearchListModel::update(Snapshot snapshot, const QString &query, const QString &genreId)
{
    if (!snapshot)
        snapshot = std::make_shared<const LibrarySnapshot>();
    const bool replaced = m_snapshot != snapshot;
    const bool wasSearching = searching();
    ++m_search->generation;
    m_search->pending = false;
    m_search->needle = query.trimmed().toLower();
    m_search->genreId = genreId;
    const int oldCount = rowCount();
    if (replaced) {
        beginResetModel();
        m_snapshot = std::move(snapshot);
        m_albumIndices.clear();
        m_trackIndices.clear();
        m_entries.clear();
        m_genreNames.clear();
        m_genreNames.reserve(m_snapshot->genres.size());
        qsizetype entryCount = m_snapshot->albums.size();
        for (const auto &album : m_snapshot->albums)
            entryCount += album.tracks.size();
        m_entries.reserve(entryCount);
        for (const auto &genre : m_snapshot->genres)
            m_genreNames.insert(genre.id, genreDisplayName(genre));
        m_albumIndices.reserve(m_snapshot->albums.size());
        m_trackIndices.reserve(m_snapshot->albums.size());
        for (qsizetype i = 0; i < m_snapshot->albums.size(); ++i) {
            const auto &album = m_snapshot->albums[i];
            if (!m_albumIndices.tryEmplace(album.id, i).inserted)
                continue;
            const QString genre = m_genreNames.value(
                    album.genreId, QCoreApplication::translate("Yalahoo", "Unclassified"));
            const auto albumEntry = m_entries.size();
            m_entries.append({ { album.id, { } },
                               album.genreId,
                               (album.title + ' '_L1 + album.artist + ' '_L1 + genre).toLower(),
                               0 });
            for (const auto &track : album.tracks)
                m_entries.append({ { album.id, track.id },
                                   album.genreId,
                                   (track.title + ' '_L1 + track.artist).toLower(),
                                   0 });
            m_entries[albumEntry].nextAlbum = m_entries.size();
            QHash<QString, qsizetype> tracks;
            tracks.reserve(album.tracks.size());
            for (qsizetype j = 0; j < album.tracks.size(); ++j)
                tracks.tryEmplace(album.tracks[j].id, j);
            m_trackIndices.insert(album.id, std::move(tracks));
        }
    }
    auto results = filter(m_entries, m_search->needle, genreId, m_search->generation,
                          m_search->generation.load(std::memory_order_relaxed));
    if (replaced) {
        m_results = std::move(results);
        endResetModel();
        if (oldCount != rowCount())
            Q_EMIT countChanged();
        if (wasSearching != searching())
            Q_EMIT searchingChanged();
        return;
    }
    replaceResults(std::move(results));
    if (wasSearching != searching())
        Q_EMIT searchingChanged();
}
void SearchListModel::replaceResults(QList<SearchResult> results)
{
    const int oldCount = rowCount();
    int prefix = 0;
    while (prefix < m_results.size() && prefix < results.size()
           && m_results[prefix] == results[prefix])
        ++prefix;
    int suffix = 0;
    while (suffix < m_results.size() - prefix && suffix < results.size() - prefix
           && m_results[m_results.size() - 1 - suffix] == results[results.size() - 1 - suffix])
        ++suffix;
    const int removed = int(m_results.size()) - prefix - suffix;
    const int inserted = int(results.size()) - prefix - suffix;
    const int reused = std::min(removed, inserted);
    for (int i = 0; i < reused; ++i)
        m_results[prefix + i] = results[prefix + i];
    if (reused)
        Q_EMIT dataChanged(index(prefix), index(prefix + reused - 1));
    if (removed > reused) {
        beginRemoveRows({ }, prefix + reused, prefix + removed - 1);
        m_results.remove(prefix + reused, removed - reused);
        endRemoveRows();
    }
    if (inserted > reused) {
        beginInsertRows({ }, prefix + reused, prefix + inserted - 1);
        m_results = std::move(results);
        endInsertRows();
    }
    if (oldCount != rowCount())
        Q_EMIT countChanged();
}
}
