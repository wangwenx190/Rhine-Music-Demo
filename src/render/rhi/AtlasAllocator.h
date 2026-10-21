// Assigns bounded atlas slots to cover identities through explicit leases. Generation checks
// reject stale tiles after eviction or atlas recreation.

#pragma once
#include "AtlasUpload.h"
#include "CoverCache.h"
#include <QHash>
#include <memory>
#include <vector>

namespace yalahoo::rhi {
namespace atlasDefaults {
// Default array capacity excludes independently owned selected/returning covers.
inline constexpr auto ArrayCapacity = 432;
inline constexpr auto PageTileSide = 8;
}

struct AtlasLease final
{
    quint64 generation = 0, lease = 0;
    quint64 coverGeneration = 1;
    int page = -1, slot = -1;
    QString identity;
};
// GUI-owned fixed pages; references pin slots until explicitly released.
class AtlasAllocator final
{
    Q_DISABLE_COPY_MOVE(AtlasAllocator)
public:
    explicit AtlasAllocator(int maximumTextureSize, int capacity = atlasDefaults::ArrayCapacity,
                            int pageTiles = atlasDefaults::PageTileSide);
    AtlasLease acquire(const QString &identity, quint64 coverGeneration = 1);
    void release(const AtlasLease &lease);
    bool publish(const AtlasLease &lease, CoverSnapshot pixels);
    QVector<AtlasUpload> uploads(int page, bool initializing);
    // GUI-only immutable descriptor cache; destination stride participates in its key.
    QVector<AtlasUpload> replay(int page, int destinationColumns = 0) const;
    QRect tile(const AtlasLease &lease) const;
    QSize pageSize() const;
    int pages() const;
    quint64 generation() const;
    void reset();
    void advanceGeneration();
    qint64 retainedBytes() const;
    int pending() const;

private:
    struct Slot final
    {
        QString identity;
        quint64 lease = 0, serial = 0;
        quint64 coverGeneration = 1;
        int references = 0;
        CoverSnapshot pixels;
        bool dirty = false;
    };
    Slot *find(const AtlasLease &lease);
    void invalidateReplay(int index);
    struct Replay final
    {
        QVector<AtlasUpload> uploads;
        int columns = 0;
    };
    QSize m_size;
    int m_slotsPerPage = 0;
    QVector<Slot> m_slots;
    std::vector<int> m_freeSlots;
    QHash<QString, int> m_identities;
    mutable QVector<Replay> m_replays;
    quint64 m_generation = 0, m_lease = 0, m_serial = 0;
};

// Four independently owned snapshots: one selected and up to three returning copies.
// Forking pins the previous pixels while a new selected lease upgrades resolution.
struct SelectedCoverLease final
{
    Q_DISABLE_COPY_MOVE(SelectedCoverLease)
    SelectedCoverLease() = default;
    QString identity;
    quint64 generation = 0, lease = 0;
    quint64 coverGeneration = 1;
    CoverSnapshot pixels;
};
class SelectedCoverPool final
{
    Q_DISABLE_COPY_MOVE(SelectedCoverPool)
public:
    SelectedCoverPool() = default;
    std::shared_ptr<SelectedCoverLease> acquire(QString identity, CoverSnapshot pixels = { },
                                                quint64 coverGeneration = 1);
    std::shared_ptr<SelectedCoverLease> fork(const std::shared_ptr<SelectedCoverLease> &source);
    bool upgrade(const std::shared_ptr<SelectedCoverLease> &lease, CoverSnapshot pixels);
    void reset();
    static constexpr int capacity = 4;

private:
    QVector<std::weak_ptr<SelectedCoverLease>> m_leases;
    quint64 m_generation = 1, m_serial = 0;
};
}
