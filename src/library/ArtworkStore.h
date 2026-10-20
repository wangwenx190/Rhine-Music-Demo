#pragma once
#include <QImage>
#include <QUrl>

namespace yalahoo {
// Worker-only, content-addressed artwork. Never writes the source image/song.
class ArtworkStore
{
public:
    static constexpr qint64 maximumBytes = 30 * 1024 * 1024;
    static QUrl copy(const QString &source, const QString &dataRoot, QString *error = nullptr);
    static QUrl save(const QImage &image, const QString &dataRoot, QString *error = nullptr);
};
}
