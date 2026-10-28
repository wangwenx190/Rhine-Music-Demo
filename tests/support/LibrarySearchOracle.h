// Independent, direct search oracle for the indexed asynchronous model.
// This intentionally scans albums/tracks without using the production search index.

#pragma once
#include "LibraryModels.h"
#include <QCoreApplication>

namespace yalahoo::test {
using namespace Qt::StringLiterals;
inline QList<SearchResult> searchLibrary(const LibrarySnapshot &snapshot, const QString &input,
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
