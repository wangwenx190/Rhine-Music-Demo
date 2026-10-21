#include "RenderData.h"
#include "AtlasUpload.h"
#include "RhiApi.h"
#include <bit>
#include <algorithm>

namespace yalahoo::rhi {
void AtlasUploadQueue::reset(QSize size, quint64 generation)
{
    require(size.width() > 0 && size.height() > 0 && size.width() % renderData::CoverTilePixels == 0
                    && size.height() % renderData::CoverTilePixels == 0 && generation > 0,
            "Invalid atlas dimensions/generation");
    m_size = size;
    m_generation = generation;
    m_serials.clear();
    m_leases.clear();
    m_uploadedLeases.clear();
    m_pending.clear();
}
void AtlasUploadQueue::enqueue(const QVector<AtlasUpload> &uploads)
{
    for (const auto &u : uploads) {
        if (u.generation != m_generation || !u.lease || !u.serial || u.slot < 0
            || u.slot >= (m_size.width() / renderData::CoverTilePixels)
                            * (m_size.height() / renderData::CoverTilePixels)
            || u.pixels.size() != QSize(renderData::CoverTilePixels, renderData::CoverTilePixels)) {
            ++m_rejected;
            continue;
        }
        if (u.serial <= m_serials.value(u.slot))
            continue;
        if (u.lease < m_leases.value(u.slot)) {
            ++m_rejected;
            continue;
        }
        const QPoint destination((u.slot % (m_size.width() / renderData::CoverTilePixels))
                                         * renderData::CoverTilePixels,
                                 (u.slot / (m_size.width() / renderData::CoverTilePixels))
                                         * renderData::CoverTilePixels);
        QSize levelSize = m_size;
        const int levels = int(std::bit_width(unsigned(qMax(m_size.width(), m_size.height())))) - 1;
        if (u.mipmaps.size() > levels) {
            ++m_rejected;
            continue;
        }
        bool valid = payloadBytes(u) <= steadyBudget;
        for (int level = 1; level <= u.mipmaps.size(); ++level) {
            levelSize = { qMax(1, levelSize.width() / 2), qMax(1, levelSize.height() / 2) };
            const auto &mip = u.mipmaps[level - 1];
            valid = valid && !mip.isNull()
                    && QRect(QPoint(), levelSize)
                               .contains(QRect(
                                       QPoint(destination.x() >> level, destination.y() >> level),
                                       mip.size()));
        }
        if (!valid) {
            ++m_rejected;
            continue;
        }
        m_serials[u.slot] = u.serial;
        m_leases[u.slot] = u.lease;
        auto detached = u;
        detached.pixels = u.pixels.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if (!u.immutableOwned) {
            detached.pixels.detach();
            for (auto &mip : detached.mipmaps)
                mip.detach();
        }
        m_pending[u.slot] = std::move(detached);
    }
}
// Drain only a bounded batch; retain newer queued tiles for the next frame.
// Initialization and incremental budgets include all submitted mip bytes.
QVector<AtlasUpload> AtlasUploadQueue::take(bool initializing)
{
    QVector<AtlasUpload> batch;
    const qint64 maximum = initializing ? initialBudget : steadyBudget;
    batch.reserve(std::min(qsizetype(m_pending.size()), qsizetype(maximum / tileBytes)));
    qint64 bytes = 0;
    while (!m_pending.isEmpty()) {
        auto entry = m_pending.begin();
        const auto cost = payloadBytes(*entry);
        if (bytes + cost > maximum)
            break;
        bytes += cost;
        m_uploadedLeases[entry.key()] = entry->lease;
        batch.append(std::move(entry.value()));
        m_pending.erase(entry);
    }
    return batch;
}
qint64 AtlasUploadQueue::payloadBytes(const AtlasUpload &upload)
{
    qint64 bytes = upload.pixels.sizeInBytes();
    for (const auto &mip : upload.mipmaps)
        bytes += mip.sizeInBytes();
    return bytes;
}
int AtlasUploadQueue::pending() const
{
    return m_pending.size();
}
quint64 AtlasUploadQueue::rejected() const
{
    return m_rejected;
}
void AtlasUploadQueue::expect(int slot, quint64 lease)
{
    require(slot >= 0
                    && slot < (m_size.width() / renderData::CoverTilePixels)
                                    * (m_size.height() / renderData::CoverTilePixels)
                    && lease > 0,
            "Invalid expected atlas lease");
    if (lease > m_leases.value(slot)) {
        m_leases[slot] = lease;
        m_serials.remove(slot);
        m_pending.remove(slot);
    }
}
bool AtlasUploadQueue::ready(int slot, quint64 lease) const
{
    return lease > 0 && m_uploadedLeases.value(slot) == lease && m_leases.value(slot) == lease;
}
bool AtlasUploadQueue::ready(QRect tile, quint64 lease) const
{
    for (int y = tile.y(); y < tile.y() + tile.height(); y += renderData::CoverTilePixels)
        for (int x = tile.x(); x < tile.x() + tile.width(); x += renderData::CoverTilePixels)
            if (!ready((y / renderData::CoverTilePixels)
                                       * (m_size.width() / renderData::CoverTilePixels)
                               + x / renderData::CoverTilePixels,
                       lease))
                return false;
    return true;
}
}
