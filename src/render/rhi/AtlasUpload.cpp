#include "AtlasUpload.h"
#include "RhiApi.h"
#include <bit>

namespace yalahoo::rhi {
void AtlasUploadQueue::reset(QSize size, quint64 generation)
{
    require(size.width() > 0 && size.height() > 0 && size.width() % 256 == 0
                    && size.height() % 256 == 0 && generation > 0,
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
            || u.slot >= (m_size.width() / 256) * (m_size.height() / 256)
            || u.pixels.size() != QSize(256, 256)) {
            ++m_rejected;
            continue;
        }
        const QPoint destination((u.slot % (m_size.width() / 256)) * 256,
                                 (u.slot / (m_size.width() / 256)) * 256);
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
        if (u.serial <= m_serials.value(u.slot))
            continue;
        if (u.lease < m_leases.value(u.slot)) {
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
QVector<AtlasUpload> AtlasUploadQueue::take(bool initializing)
{
    QVector<AtlasUpload> batch;
    const qint64 maximum = initializing ? initialBudget : steadyBudget;
    qint64 bytes = 0;
    while (!m_pending.isEmpty()) {
        const auto cost = payloadBytes(m_pending.first());
        if (bytes + cost > maximum)
            break;
        bytes += cost;
        auto entry = m_pending.begin();
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
    require(slot >= 0 && slot < (m_size.width() / 256) * (m_size.height() / 256) && lease > 0,
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
    for (int y = tile.y(); y < tile.y() + tile.height(); y += 256)
        for (int x = tile.x(); x < tile.x() + tile.width(); x += 256)
            if (!ready((y / 256) * (m_size.width() / 256) + x / 256, lease))
                return false;
    return true;
}
}
