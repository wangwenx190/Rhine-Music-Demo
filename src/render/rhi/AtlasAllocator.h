#pragma once
#include "AtlasUpload.h"
#include "CoverCache.h"
#include <QHash>
#include <memory>

namespace yalahoo::rhi {
struct AtlasLease
{
    quint64 generation = 0, lease = 0;
    quint64 coverGeneration = 1;
    int page = -1, slot = -1;
    QString identity;
};
// GUI-owned fixed pages; references pin slots until explicitly released.
class AtlasAllocator
{
public:
    explicit AtlasAllocator(int maximumTextureSize, int capacity = 432, int pageTiles = 8);
    AtlasLease acquire(const QString &identity, quint64 coverGeneration = 1);
    void release(const AtlasLease &lease);
    bool publish(const AtlasLease &lease, CoverSnapshot pixels);
    QVector<AtlasUpload> uploads(int page, bool initializing);
    QVector<AtlasUpload> replay(int page) const;
    QRect tile(const AtlasLease &lease) const;
    QSize pageSize() const;
    int pages() const;
    quint64 generation() const;
    void reset();
    qint64 retainedBytes() const;
    int pending() const;

private:
    struct Slot
    {
        QString identity;
        quint64 lease = 0, serial = 0;
        quint64 coverGeneration = 1;
        int references = 0;
        CoverSnapshot pixels;
        bool dirty = false;
    };
    Slot *find(const AtlasLease &lease);
    QSize m_size;
    int m_slotsPerPage = 0;
    QVector<Slot> m_slots;
    QHash<QString, int> m_identities;
    quint64 m_generation = 0, m_lease = 0, m_serial = 0;
};

// Four independently owned snapshots: one selected and up to three returning copies.
// Forking pins the previous pixels while a new selected lease upgrades resolution.
struct SelectedCoverLease
{
    QString identity;
    quint64 generation = 0, lease = 0;
    quint64 coverGeneration = 1;
    CoverSnapshot pixels;
};
class SelectedCoverPool
{
public:
    std::shared_ptr<SelectedCoverLease> acquire(QString identity, CoverSnapshot pixels = { },
                                                quint64 coverGeneration = 1);
    std::shared_ptr<SelectedCoverLease> fork(const std::shared_ptr<SelectedCoverLease> &source);
    bool upgrade(const std::shared_ptr<SelectedCoverLease> &lease, CoverSnapshot pixels);
    void reset();
    static constexpr int capacity = 4;
    static constexpr qint64 maximumBytes = capacity * 1024 * 1024 * 4;

private:
    QVector<std::weak_ptr<SelectedCoverLease>> m_leases;
    quint64 m_generation = 1, m_serial = 0;
};
}
