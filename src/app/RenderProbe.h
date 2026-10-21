// Provides deterministic render inputs and output collection for review. Readback completion is
// treated separately from submitting a frame.

#pragma once
#include "CoverPixels.h"
#include <QQuick3DInstancing>
#include <QQuick3DTextureData>
#include <QVariantList>
// R10 research only. No production pool, decoder, cache or RHI implementation.
class RenderProbeInstances final : public QQuick3DInstancing
{
    Q_DISABLE_COPY_MOVE(RenderProbeInstances)
public:
    RenderProbeInstances() = default;
    using QQuick3DInstancing::QQuick3DInstancing;
    void setPositions(const QVariantList &positions);

protected:
    QByteArray getInstanceBuffer(int *count) override
    {
        *count = m_count;
        return m_buffer;
    }

private:
    QByteArray m_buffer;
    int m_count = 0;
};
class RenderProbeAtlas final : public QQuick3DTextureData
{
    Q_DISABLE_COPY_MOVE(RenderProbeAtlas)
public:
    explicit RenderProbeAtlas(QQuick3DObject *parent = nullptr);
    void setPhase(int phase);
    int phase() const
    {
        return m_phase;
    }
    int submissions() const
    {
        return m_submissions;
    }
    static constexpr int pageBytes = 2048 * 2048 * 4;
    static constexpr int tileBytes = yalahoo::coverPixels::TileBytes;

private:
    QByteArray m_pixels;
    int m_phase = -1, m_submissions = 0;
};
