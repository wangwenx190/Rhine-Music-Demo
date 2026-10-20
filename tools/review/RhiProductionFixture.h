// Builds anonymous production scene and cover inputs for RHI review. Coherent revisions exercise
// actual upload paths without reading a private library.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "SceneItem.h"

namespace yalahoo::review {
inline Snapshot productionLibrary()
{
    LibrarySnapshot result;
    for (int lane = 0; lane < 9; ++lane) {
        Genre genre;
        genre.id = QString::number(lane);
        genre.name = genre.id;
        result.genres.append(genre);
        for (int row = 0; row < 64; ++row) {
            Album album;
            album.id = u"%1:%2"_s.arg(lane).arg(row);
            album.genreId = QString::number(lane);
            album.title = u"Anonymous album %1"_s.arg(row);
            album.coverUrl = QUrl(u"qrc:/assets/demo-covers/%1.png"_s.arg(
                    QStringList{ u"square"_s, u"portrait"_s, u"landscape"_s }[row % 3]));
            result.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(result));
}
}
