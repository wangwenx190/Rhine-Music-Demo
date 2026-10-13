#include "AtlasUpload.h"
#include "RhiApi.h"

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
        detached.pixels.detach();
        m_pending[u.slot] = std::move(detached);
    }
}
QVector<AtlasUpload> AtlasUploadQueue::take(bool initializing)
{
    QVector<AtlasUpload> batch;
    const int maximum = int((initializing ? initialBudget : steadyBudget) / tileBytes);
    while (!m_pending.isEmpty() && batch.size() < maximum) {
        auto entry = m_pending.begin();
        batch.append(std::move(entry.value()));
        m_pending.erase(entry);
    }
    return batch;
}
int AtlasUploadQueue::pending() const
{
    return m_pending.size();
}
quint64 AtlasUploadQueue::rejected() const
{
    return m_rejected;
}
}
