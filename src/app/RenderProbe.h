#pragma once
#include <QQuick3DInstancing>
#include <QQuick3DTextureData>
#include <QVariantList>
// R10 research only. No production pool, decoder, cache or RHI implementation.
class RenderProbeInstances : public QQuick3DInstancing
{
public:
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
class RenderProbeAtlas : public QQuick3DTextureData
{
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
    static constexpr int tileBytes = 256 * 256 * 4;

private:
    QByteArray m_pixels;
    int m_phase = -1, m_submissions = 0;
};
