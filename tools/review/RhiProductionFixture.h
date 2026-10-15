#pragma once
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
            album.id = QString("%1:%2").arg(lane).arg(row);
            album.genreId = QString::number(lane);
            album.title = QString("Anonymous album %1").arg(row);
            album.coverUrl =
                    QUrl(QString("qrc:/assets/demo-covers/%1.png")
                                 .arg(QStringList{ "square", "portrait", "landscape" }[row % 3]));
            result.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(result));
}
}
