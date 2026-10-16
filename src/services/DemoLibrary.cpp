#include "DemoLibrary.h"
namespace yalahoo {
LibrarySnapshot demonstrationLibrary()
{
    yalahoo::LibrarySnapshot demo;
    const QStringList covers{ "square", "portrait", "landscape" };
    for (int lane = 0; lane < 9; ++lane) {
        const auto genreId = QStringLiteral("demo-genre-%1").arg(lane);
        yalahoo::Genre genre;
        genre.id = genreId;
        genre.name = QStringLiteral("演示收藏 %1").arg(lane + 1);
        demo.genres.append(genre);
        for (int row = 0; row < 3; ++row) {
            yalahoo::Album album;
            album.id = QStringLiteral("demo-album-%1-%2").arg(lane).arg(row);
            album.title = QStringLiteral("演示专辑 %1").arg(row + 1);
            album.genreId = genreId;
            album.coverUrl =
                    QUrl(QStringLiteral("qrc:/assets/demo-covers/%1.png").arg(covers[row]));
            demo.albums.append(album);
        }
    }
    return demo;
}
}
