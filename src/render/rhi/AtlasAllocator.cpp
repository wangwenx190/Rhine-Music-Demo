#include "AtlasAllocator.h"
#include <stdexcept>
#include <limits>

namespace yalahoo::rhi {
namespace {
std::atomic<quint64> nextGeneration{ 0 };
}
AtlasAllocator::AtlasAllocator(int maximumTextureSize, int capacity, int pageTiles)
{
    if (maximumTextureSize < 256 || capacity < 1 || capacity > 436 || pageTiles < 1
        || pageTiles > std::numeric_limits<int>::max() / 256)
        throw std::invalid_argument("GPU cannot hold a cover tile or invalid atlas capacity");
    const int side = qMin(pageTiles, maximumTextureSize / 256);
    if (qint64(side) * side > std::numeric_limits<int>::max())
        throw std::invalid_argument("Atlas page slot count exceeds the supported integer range");
    m_size = { side * 256, side * 256 };
    m_slotsPerPage = side * side;
    m_slots.resize(capacity);
    reset();
}
AtlasLease AtlasAllocator::acquire(const QString &identity, quint64 coverGeneration)
{
    if (identity.isEmpty())
        return { };
    int index = m_identities.value(identity, -1);
    if (index >= 0 && m_slots[index].coverGeneration != coverGeneration)
        return { };
    if (index < 0) {
        for (int i = 0; i < m_slots.size(); ++i) {
            if (!m_slots[i].references) {
                index = i;
                break;
            }
        }
        if (index < 0)
            return { };
        auto &slot = m_slots[index];
        m_identities.remove(slot.identity);
        slot = { };
        slot.identity = identity;
        slot.lease = ++m_lease;
        slot.coverGeneration = coverGeneration;
        m_identities.insert(identity, index);
    }
    auto &slot = m_slots[index];
    ++slot.references;
    return { m_generation,           slot.lease, coverGeneration, index / m_slotsPerPage,
             index % m_slotsPerPage, identity };
}
AtlasAllocator::Slot *AtlasAllocator::find(const AtlasLease &lease)
{
    if (lease.generation != m_generation || lease.page < 0 || lease.page >= pages()
        || lease.slot < 0 || lease.slot >= m_slotsPerPage)
        return nullptr;
    const int index = lease.page * m_slotsPerPage + lease.slot;
    if (index >= m_slots.size())
        return nullptr;
    auto &slot = m_slots[index];
    return slot.lease == lease.lease && slot.identity == lease.identity && slot.references > 0
            ? &slot
            : nullptr;
}
void AtlasAllocator::release(const AtlasLease &lease)
{
    if (auto *slot = find(lease)) {
        if (--slot->references == 0) {
            m_identities.remove(slot->identity);
            slot->pixels.reset();
            slot->dirty = false;
        }
    }
}
bool AtlasAllocator::publish(const AtlasLease &lease, CoverSnapshot pixels)
{
    auto *slot = find(lease);
    if (!slot || !pixels || pixels->identity != lease.identity || pixels->resolution != 256
        || pixels->generation != slot->coverGeneration || pixels->image.size() != QSize(256, 256)
        || pixels->image.format() != QImage::Format_RGBA8888_Premultiplied)
        return false;
    if (slot->pixels == pixels)
        return true;
    slot->pixels = std::move(pixels);
    slot->serial = ++m_serial;
    slot->dirty = true;
    return true;
}
QVector<AtlasUpload> AtlasAllocator::uploads(int page, bool initializing)
{
    QVector<AtlasUpload> batch;
    if (page < 0 || page >= pages())
        return batch;
    const qint64 limit =
            initializing ? AtlasUploadQueue::initialBudget : AtlasUploadQueue::steadyBudget;
    qint64 bytes = 0;
    for (int i = page * m_slotsPerPage; i < qMin((page + 1) * m_slotsPerPage, int(m_slots.size()));
         ++i) {
        auto &slot = m_slots[i];
        if (slot.dirty && slot.references && slot.pixels) {
            AtlasUpload upload{ m_generation,        slot.lease,         slot.serial,
                                i % m_slotsPerPage,  slot.pixels->image, true,
                                slot.pixels->mipmaps };
            const auto cost = AtlasUploadQueue::payloadBytes(upload);
            if (bytes + cost > limit)
                break;
            bytes += cost;
            batch.append(std::move(upload));
            slot.dirty = false;
        }
    }
    return batch;
}
QVector<AtlasUpload> AtlasAllocator::replay(int page) const
{
    QVector<AtlasUpload> batch;
    if (page < 0 || page >= pages())
        return batch;
    for (int i = page * m_slotsPerPage; i < qMin((page + 1) * m_slotsPerPage, int(m_slots.size()));
         ++i) {
        const auto &slot = m_slots[i];
        if (slot.references && slot.pixels)
            batch.append({ m_generation, slot.lease, slot.serial, i % m_slotsPerPage,
                           slot.pixels->image, true, slot.pixels->mipmaps });
    }
    return batch;
}
QRect AtlasAllocator::tile(const AtlasLease &lease) const
{
    const int side = m_size.width() / 256;
    return { (lease.slot % side) * 256, (lease.slot / side) * 256, 256, 256 };
}
QSize AtlasAllocator::pageSize() const
{
    return m_size;
}
int AtlasAllocator::pages() const
{
    return (m_slots.size() + m_slotsPerPage - 1) / m_slotsPerPage;
}
quint64 AtlasAllocator::generation() const
{
    return m_generation;
}
void AtlasAllocator::reset()
{
    m_generation = ++nextGeneration;
    m_identities.clear();
    for (auto &slot : m_slots)
        slot = { };
}
void AtlasAllocator::advanceGeneration()
{
    m_generation = ++nextGeneration;
    for (auto &slot : m_slots)
        if (slot.references && slot.pixels)
            slot.dirty = true;
}
qint64 AtlasAllocator::retainedBytes() const
{
    qint64 bytes = 0;
    for (const auto &slot : m_slots) {
        if (slot.pixels) {
            bytes += slot.pixels->byteSize();
        }
    }
    return bytes;
}
int AtlasAllocator::pending() const
{
    int count = 0;
    for (const auto &slot : m_slots)
        count += slot.dirty;
    return count;
}
std::shared_ptr<SelectedCoverLease>
SelectedCoverPool::acquire(QString identity, CoverSnapshot pixels, quint64 coverGeneration)
{
    m_leases.removeIf([](const auto &lease) {
        return lease.expired();
    });
    if (m_leases.size() >= capacity || identity.isEmpty())
        return { };
    auto lease = std::make_shared<SelectedCoverLease>();
    lease->identity = std::move(identity);
    lease->generation = m_generation;
    lease->lease = ++m_serial;
    lease->coverGeneration = coverGeneration;
    m_leases.append(lease);
    upgrade(lease, std::move(pixels));
    return lease;
}
std::shared_ptr<SelectedCoverLease>
SelectedCoverPool::fork(const std::shared_ptr<SelectedCoverLease> &source)
{
    if (!source || source->generation != m_generation)
        return { };
    return acquire(source->identity, source->pixels, source->coverGeneration);
}
bool SelectedCoverPool::upgrade(const std::shared_ptr<SelectedCoverLease> &lease,
                                CoverSnapshot pixels)
{
    if (!lease || lease->generation != m_generation || !pixels
        || pixels->identity != lease->identity
        || (pixels->resolution <= 0 || (pixels->resolution & (pixels->resolution - 1)) != 0)
        || pixels->generation != lease->coverGeneration
        || pixels->image.size() != QSize(pixels->resolution, pixels->resolution)
        || (lease->pixels && lease->pixels->error.isEmpty() && !pixels->error.isEmpty())
        || (lease->pixels && pixels->resolution < lease->pixels->resolution))
        return false;
    lease->pixels = std::move(pixels);
    return true;
}
void SelectedCoverPool::reset()
{
    ++m_generation;
    // Old leases still count toward the limit until their owners release them.
}
}
