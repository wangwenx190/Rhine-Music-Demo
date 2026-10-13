#pragma once
#include <QImage>
#include <QJsonArray>
#include <QMap>
#include <QSize>
#include <QVector>

namespace yalahoo::rhi {
struct AtlasUpload
{
    quint64 generation = 0, lease = 0, serial = 0;
    int slot = 0;
    QImage pixels;
    bool immutableOwned = false;
};
// Render-thread queue. Generation changes invalidate both pending work and slot revisions.
class AtlasUploadQueue
{
public:
    void reset(QSize size, quint64 generation);
    void enqueue(const QVector<AtlasUpload> &uploads);
    QVector<AtlasUpload> take(bool initializing);
    int pending() const;
    quint64 rejected() const;
    void expect(int slot, quint64 lease);
    bool ready(int slot, quint64 lease) const;
    static constexpr qint64 tileBytes = 256 * 256 * 4;
    static constexpr qint64 steadyBudget = 2 * 1024 * 1024;
    static constexpr qint64 initialBudget = 16 * 1024 * 1024;

private:
    QSize m_size;
    quint64 m_generation = 0, m_rejected = 0;
    QMap<int, quint64> m_serials;
    QMap<int, quint64> m_leases;
    QMap<int, quint64> m_uploadedLeases;
    QMap<int, AtlasUpload> m_pending;
};
}
