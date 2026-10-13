#include "LibraryModels.h"
#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <numeric>

namespace yalahoo {
QString normalizedKey(const QString &value)
{
    return value.normalized(QString::NormalizationForm_KC).simplified().toLower();
}
LibraryOrder orderLibrary(const LibrarySnapshot &snapshot, SortMode mode)
{
    LibraryOrder result;
    result.indices.resize(snapshot.albums.size());
    std::iota(result.indices.begin(), result.indices.end(), 0);
    QCollator collator(QLocale(QStringLiteral("zh_CN")));
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
            LibraryColumn column{ QStringLiteral("album:%1").arg(start / 12),
                                  QStringLiteral("%1–%2")
                                          .arg(start + 1, 2, 10, QLatin1Char('0'))
                                          .arg(end, 2, 10, QLatin1Char('0')),
                                  { } };
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
                    result.columns.append({ genre.id, genre.name, { } });
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
                result.columns.append({ key,
                                        label.isEmpty() ? QStringLiteral("未知歌手") : label,
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
        names.insert(genre.id, genre.name);
    QList<SearchResult> results;
    for (const auto &album : snapshot.albums) {
        if (!genreId.isEmpty() && album.genreId != genreId)
            continue;
        QString genre = names.value(album.genreId);
        if (genre.isEmpty())
            genre = QStringLiteral("未分类");
        if (query.isEmpty()
            || (album.title + ' ' + album.artist + ' ' + genre).toLower().contains(query))
            results.append({ album.id, { } });
        if (query.isEmpty())
            continue;
        for (const auto &track : album.tracks)
            if ((track.title + ' ' + track.artist).toLower().contains(query))
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
    return { { AlbumId, "albumId" },
             { Title, "title" },
             { Artist, "artist" },
             { Year, "year" },
             { GenreId, "genreId" },
             { CoverUrl, "coverUrl" },
             { Folder, "folder" },
             { Tracks, "tracks" },
             { Producers, "producers" },
             { Description, "description" },
             { DescriptionSource, "descriptionSource" },
             { Introduction, "introduction" },
             { CreditsLookup, "creditsLookup" },
             { Online, "online" },
             { LocalNote, "localNote" },
             { Offline, "offline" },
             { AlbumData, "albumData" } };
}
QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= rowCount())
        return { };
    const auto &album = m_snapshot->albums[m_order.indices[index.row()]];
    switch (role) {
    case AlbumId:
        return album.id;
    case Title:
        return album.title;
    case Artist:
        return album.artist;
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
            map.insert("sourceUrl", track.sourceUrl);
            tracks.append(map);
        }
        return tracks;
    }
    case Producers:
        return toJson(album).value("producers").toVariant();
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
    beginResetModel();
    m_snapshot = snapshot ? std::move(snapshot) : std::make_shared<const LibrarySnapshot>();
    m_order = orderLibrary(*m_snapshot, m_mode);
    endResetModel();
    emit countChanged();
    emit columnsChanged();
}
void AlbumListModel::setSortMode(SortMode mode)
{
    if (m_mode == mode)
        return;
    beginResetModel();
    m_mode = mode;
    m_order = orderLibrary(*m_snapshot, mode);
    endResetModel();
    emit columnsChanged();
}
QVariantList AlbumListModel::columns() const
{
    QVariantList result;
    int lane = 0;
    for (const auto &column : m_order.columns)
        result.append(QVariantMap{ { "key", column.key },
                                   { "label", column.label },
                                   { "albumIds", column.albumIds },
                                   { "lane", lane++ } });
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
    return { { Kind, "kind" },
             { AlbumId, "albumId" },
             { TrackId, "trackId" },
             { Title, "title" },
             { Artist, "artist" } };
}
QVariant SearchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= rowCount())
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
        return track ? QStringLiteral("track") : QStringLiteral("album");
    case AlbumId:
        return result.albumId;
    case TrackId:
        return result.trackId;
    case Title:
        return track ? track->title : album.title;
    case Artist:
        return track ? track->artist : album.artist;
    default:
        return { };
    }
}
void SearchListModel::update(Snapshot snapshot, const QString &query, const QString &genreId)
{
    beginResetModel();
    if (!snapshot)
        snapshot = std::make_shared<const LibrarySnapshot>();
    if (m_snapshot != snapshot) {
        m_snapshot = std::move(snapshot);
        m_albumIndices.clear();
        m_trackIndices.clear();
        m_albumIndices.reserve(m_snapshot->albums.size());
        m_trackIndices.reserve(m_snapshot->albums.size());
        for (qsizetype i = 0; i < m_snapshot->albums.size(); ++i) {
            const auto &album = m_snapshot->albums[i];
            if (m_albumIndices.contains(album.id))
                continue;
            m_albumIndices.insert(album.id, i);
            QHash<QString, qsizetype> tracks;
            tracks.reserve(album.tracks.size());
            for (qsizetype j = 0; j < album.tracks.size(); ++j)
                if (!tracks.contains(album.tracks[j].id))
                    tracks.insert(album.tracks[j].id, j);
            m_trackIndices.insert(album.id, std::move(tracks));
        }
    }
    m_results = searchLibrary(*m_snapshot, query, genreId);
    endResetModel();
    emit countChanged();
}
}
