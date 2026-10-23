#include "LibraryModels.h"
#include "Units.h"
#include <QCoreApplication>
#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <iterator>
#include <numeric>

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
}
