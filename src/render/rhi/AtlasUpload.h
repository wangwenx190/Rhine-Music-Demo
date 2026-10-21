// Coalesces dirty atlas tiles into bounded backend upload batches. Initialization and replay are
// distinct from incremental updates to avoid redundant transfers.

#pragma once
#include "CoverPixels.h"
#include <QImage>
#include <QJsonArray>
#include <QMap>
#include <QHash>
#include <QSize>
#include <QVector>
#include <utility>

namespace yalahoo::rhi {
struct AtlasUpload final
{
    AtlasUpload(quint64 generation = 0, quint64 lease = 0, quint64 serial = 0, int slot = 0,
                QImage pixels = { }, bool immutableOwned = false, QList<QImage> mipmaps = { })
        : generation(generation),
          lease(lease),
          serial(serial),
          slot(slot),
          pixels(std::move(pixels)),
          immutableOwned(immutableOwned),
          mipmaps(std::move(mipmaps))
    {
    }
    quint64 generation = 0, lease = 0, serial = 0;
    int slot = 0;
    QImage pixels;
    bool immutableOwned = false;
    QList<QImage> mipmaps;
};
// Render-thread queue. Generation changes invalidate both pending work and slot revisions.
class AtlasUploadQueue final
{
    Q_DISABLE_COPY_MOVE(AtlasUploadQueue)
public:
    AtlasUploadQueue() = default;
    void reset(QSize size, quint64 generation);
    void enqueue(const QVector<AtlasUpload> &uploads);
    QVector<AtlasUpload> take(bool initializing);
    int pending() const;
    quint64 rejected() const;
    void expect(int slot, quint64 lease);
    bool ready(int slot, quint64 lease) const;
    bool ready(QRect tile, quint64 lease) const;
    static qint64 payloadBytes(const AtlasUpload &upload);
    static constexpr qint64 tileBytes = coverPixels::TileBytes;
    static constexpr qint64 steadyBudget = 2 * 1024 * 1024;
    static constexpr qint64 initialBudget = 16 * 1024 * 1024;

private:
    QSize m_size;
    quint64 m_generation = 0, m_rejected = 0;
    QHash<int, quint64> m_serials;
    QHash<int, quint64> m_leases;
    QHash<int, quint64> m_uploadedLeases;
    QMap<int, AtlasUpload> m_pending;
};
}
