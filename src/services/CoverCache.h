// Decodes artwork on a bounded shared worker pool and publishes immutable pixels. Generations,
// cancellation and a byte-limited cache reject stale results without blocking GUI teardown.

#pragma once
#include "CoverPixels.h"
#include <QImage>
#include <QList>
#include <QPoint>
#include <QJsonObject>
#include <QObject>
#include <memory>

namespace yalahoo {
namespace coverCache {
// Each incremental atlas tile has this physical pixel extent.

}

// Values published once by the worker; consumers only share immutable, owned pixels.
struct CoverMipTile final
{
    QPoint origin;
    QImage image;
    QList<QImage> mipmaps;
};
struct CoverPixels final
{
    QString identity, path, error, fallbackTitle;
    quint64 generation = 0;
    int resolution = coverPixels::TileSide;
    QSize sourceSize;
    bool decoderScaling = false;
    qint64 readerReturnedBytes = 0;
    QImage image;
    QList<QImage> mipmaps;
    QList<CoverMipTile> tiles;
    qint64 byteSize() const;
};
using CoverSnapshot = std::shared_ptr<const CoverPixels>;

class CoverCache final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CoverCache)
public:
    explicit CoverCache(QObject *parent = nullptr);
    ~CoverCache() override;
    // Final process teardown only, while QGuiApplication/font services still exist.
    static void finishDecoders();
    bool request(QString identity, QString path, int resolution, QString title = { });
    CoverSnapshot lookup(const QString &identity, const QString &path, int resolution,
                         const QString &title = { });
    void reset();
    void setSuspended(bool suspended);
    void cancel(const QString &identity);
    void invalidate(const QString &identity);
    quint64 generation() const;
    QJsonObject statistics() const;
    bool hasPendingCapacity() const;
    static QList<QImage> makeMipmaps(const QImage &image);
    static QImage paint(const QImage &source, int resolution, const QString &title);
    static constexpr int concurrency = 3;
    static constexpr int pendingLimit = 96;
    static constexpr qint64 cacheLimit = 16 * 1024 * 1024;
    static qint64 workingSetEstimateFor(int resolution);

Q_SIGNALS:
    void ready(yalahoo::CoverSnapshot pixels);

private:
    struct Job;
    struct State;
    struct Entry;
    static void decodeAndDeliver(const Job &job, const std::shared_ptr<State> &state);
    static QImage readSource(const Job &job, const std::shared_ptr<State> &state,
                             CoverPixels &pixels);
    static CoverSnapshot decode(const Job &job, const std::shared_ptr<State> &state);
    static QString key(const QString &identity, const QString &path, int resolution,
                       const QString &title);
    void pump();
    void complete(const Job &job, CoverSnapshot pixels);
    struct Private;
    std::unique_ptr<Private> d;
};
}
Q_DECLARE_METATYPE(yalahoo::CoverSnapshot)
