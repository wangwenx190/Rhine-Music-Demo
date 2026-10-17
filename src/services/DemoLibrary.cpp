#include <QCoreApplication>
#include "DemoLibrary.h"

using namespace Qt::StringLiterals;

namespace yalahoo {
LibrarySnapshot demonstrationLibrary()
{
    yalahoo::LibrarySnapshot demo;
    const QStringList covers{ u"square"_s, u"portrait"_s, u"landscape"_s };
    for (int lane = 0; lane < 9; ++lane) {
        const auto genreId = u"demo-genre-%1"_s.arg(lane);
        yalahoo::Genre genre;
        genre.id = genreId;
        genre.name = QCoreApplication::translate("Yalahoo", "Demo collection %1").arg(lane + 1);
        demo.genres.append(genre);
        for (int row = 0; row < 3; ++row) {
            yalahoo::Album album;
            album.id = u"demo-album-%1-%2"_s.arg(lane).arg(row);
            album.title = QCoreApplication::translate("Yalahoo", "Demo album %1").arg(row + 1);
            album.genreId = genreId;
            album.coverUrl = QUrl(u"qrc:/assets/demo-covers/%1.png"_s.arg(covers[row]));
            demo.albums.append(album);
        }
    }
    return demo;
}
}
