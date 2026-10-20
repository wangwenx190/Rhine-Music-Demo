#include <QCoreApplication>
#include "LibraryModels.h"
#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <numeric>

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
LibraryOrder orderLibrary(const LibrarySnapshot &snapshot, SortMode mode)
{
    LibraryOrder result;
    result.indices.resize(snapshot.albums.size());
    std::iota(result.indices.begin(), result.indices.end(), 0);
    QCollator collator(QLocale(u"zh_CN"_s));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const auto compare = [&](const QString &a, const QString &b) {
        const int locale = collator.compare(a, b);
        return locale ? locale : QString::compare(a, b, Qt::CaseSensitive);
    };
    QStringList titles, artists;
    if (mode != SortMode::Genre) {
        titles.reserve(snapshot.albums.size());
        artists.reserve(snapshot.albums.size());
        for (const auto &album : snapshot.albums) {
            titles.append(normalizedKey(album.title));
            if (mode == SortMode::Artist)
                artists.append(normalizedKey(album.artist));
        }
        std::stable_sort(result.indices.begin(), result.indices.end(), [&](int a, int b) {
            const auto &left = snapshot.albums[a];
            const auto &right = snapshot.albums[b];
            if (mode == SortMode::Artist) {
                const int artist = compare(artists[a], artists[b]);
                if (artist)
                    return artist < 0;
            }
            const int title = compare(titles[a], titles[b]);
            return title ? title < 0 : left.id < right.id;
        });
    }
    if (mode == SortMode::Album) {
        for (int start = 0; start < result.indices.size(); start += 12) {
            const int end = std::min(start + 12, int(result.indices.size()));
            LibraryColumn column{
                u"album:%1"_s.arg(start / 12),
                u"%1\u2013%2"_s.arg(start + 1, 2, 10, '0'_L1).arg(end, 2, 10, '0'_L1),
                { }
            };
            for (int i = start; i < end; ++i)
                column.albumIds.append(snapshot.albums[result.indices[i]].id);
            result.columns.append(column);
        }
    } else {
        QSet<QString> used;
        for (const auto &album : snapshot.albums)
            used.insert(album.genreId);
        if (mode == SortMode::Genre) {
            for (const auto &genre : snapshot.genres) {
                if (used.remove(genre.id))
                    result.columns.append({ genre.id, genreDisplayName(genre), { } });
            }
            for (const auto &album : snapshot.albums)
                if (used.remove(album.genreId))
                    result.columns.append({ album.genreId, album.genreId, { } });
        }
        QHash<QString, qsizetype> columnByKey;
        columnByKey.reserve(result.columns.size());
        for (qsizetype i = 0; i < result.columns.size(); ++i)
            columnByKey.insert(result.columns[i].key, i);
        for (int index : result.indices) {
            const auto &album = snapshot.albums[index];
            const QString key = mode == SortMode::Artist ? artists[index] : album.genreId;
            const auto column = columnByKey.constFind(key);
            if (column == columnByKey.cend()) {
                const QString label = album.artist.simplified();
                columnByKey.insert(key, result.columns.size());
                result.columns.append(
                        { key,
                          label.isEmpty() ? QCoreApplication::translate("Yalahoo", "Unknown artist")
                                          : label,
                          { album.id } });
            } else
                result.columns[column.value()].albumIds.append(album.id);
        }
    }
    for (const auto &column : result.columns)
        result.slotStride = std::max(result.slotStride, int(column.albumIds.size()) + 12);
    return result;
}
QList<SearchResult> searchLibrary(const LibrarySnapshot &snapshot, const QString &input,
                                  const QString &genreId)
{
    // Deliberately no NFKC / whitespace folding: music-search.ts uses trim + lowercase.
    const QString query = input.trimmed().toLower();
    QHash<QString, QString> names;
    for (const auto &genre : snapshot.genres)
        names.insert(genre.id, genreDisplayName(genre));
    QList<SearchResult> results;
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
    return { { AlbumId, "albumId"_ba },
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
}
QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &album = m_snapshot->albums[m_order.indices[index.row()]];
    switch (role) {
    case AlbumId:
        return album.id;
    case Title:
        return album.title;
    case Artist:
        return artistDisplayName(album.artist);
    case Year:
        return album.year ? QVariant(*album.year) : QVariant{ };
    case GenreId:
        return album.genreId;
    case CoverUrl:
        return album.coverUrl;
    case Folder:
        return album.folder;
    case Tracks: {
        QVariantList tracks;
        for (const auto &track : album.tracks) {
            auto map = toJson(track).toVariantMap();
            map.insert(u"sourceUrl"_s, track.sourceUrl);
            tracks.append(map);
        }
        return tracks;
    }
    case Producers:
        return producerData(album.producers).toVariantList();
    case Description:
        return album.description;
    case DescriptionSource:
        return album.descriptionSource.toVariantMap();
    case Introduction:
        return album.introduction.toVariantMap();
    case CreditsLookup:
        return album.creditsLookup.toVariantMap();
    case Online:
        return album.online.toVariantMap();
    case LocalNote:
        return album.localNote;
    case Offline:
        return album.offline;
    case AlbumData:
        return toJson(album).toVariantMap();
    default:
        return { };
    }
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
    int lane = 0;
    for (const auto &column : m_order.columns)
        result.append(QVariantMap{ { u"key"_s, column.key },
                                   { u"label"_s, column.label },
                                   { u"albumIds"_s, column.albumIds },
                                   { u"lane"_s, lane++ } });
    return result;
}
SearchListModel::SearchListModel(QObject *parent)
    : QAbstractListModel(parent), m_snapshot(std::make_shared<const LibrarySnapshot>())
{
}
int SearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_results.size());
}
QHash<int, QByteArray> SearchListModel::roleNames() const
{
    return { { Kind, "kind"_ba },        { AlbumId, "albumId"_ba }, { TrackId, "trackId"_ba },
             { Title, "title"_ba },      { Artist, "artist"_ba },   { CoverUrl, "coverUrl"_ba },
             { Subtitle, "subtitle"_ba } };
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
    const int oldCount = rowCount();
    if (replaced) {
        beginResetModel();
        m_snapshot = std::move(snapshot);
        m_albumIndices.clear();
        m_trackIndices.clear();
        m_entries.clear();
        m_genreNames.clear();
        for (const auto &genre : m_snapshot->genres)
            m_genreNames.insert(genre.id, genreDisplayName(genre));
        m_albumIndices.reserve(m_snapshot->albums.size());
        m_trackIndices.reserve(m_snapshot->albums.size());
        for (qsizetype i = 0; i < m_snapshot->albums.size(); ++i) {
            const auto &album = m_snapshot->albums[i];
            if (m_albumIndices.contains(album.id))
                continue;
            m_albumIndices.insert(album.id, i);
            const QString genre = m_genreNames.value(
                    album.genreId, QCoreApplication::translate("Yalahoo", "Unclassified"));
            m_entries.append({ { album.id, { } },
                               album.genreId,
                               (album.title + ' '_L1 + album.artist + ' '_L1 + genre).toLower(),
                               true });
            for (const auto &track : album.tracks)
                m_entries.append({ { album.id, track.id },
                                   album.genreId,
                                   (track.title + ' '_L1 + track.artist).toLower(),
                                   false });
            QHash<QString, qsizetype> tracks;
            tracks.reserve(album.tracks.size());
            for (qsizetype j = 0; j < album.tracks.size(); ++j)
                if (!tracks.contains(album.tracks[j].id))
                    tracks.insert(album.tracks[j].id, j);
            m_trackIndices.insert(album.id, std::move(tracks));
        }
    }
    const QString needle = query.trimmed().toLower();
    QList<SearchResult> results;
    for (const auto &entry : m_entries) {
        if (!genreId.isEmpty() && entry.genreId != genreId)
            continue;
        if (needle.isEmpty() ? entry.album : entry.text.contains(needle))
            results.append(entry.result);
    }
    if (replaced) {
        m_results = std::move(results);
        endResetModel();
        if (oldCount != rowCount())
            Q_EMIT countChanged();
        return;
    }
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
