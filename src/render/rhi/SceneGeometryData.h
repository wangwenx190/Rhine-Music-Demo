// Builds immutable case/paper/floor mesh ranges and packed CPU geometry once per renderer.
// Values can be read during uploads and sorting without retaining asset-container references.

#pragma once
#include <QByteArray>
#include <QVector3D>

namespace yalahoo::rhi {
struct Mesh final
{
    quint32 start = 0, count = 0;
    QVector3D center{ };
};
class SceneGeometryData final
{
public:
    SceneGeometryData();
    const Mesh &lid() const
    {
        return m_lid;
    }
    const Mesh &spine() const
    {
        return m_spine;
    }
    const Mesh &tray() const
    {
        return m_tray;
    }
    const Mesh &paper() const
    {
        return m_paper;
    }
    const Mesh &floor() const
    {
        return m_floor;
    }
    const Mesh &productFloor() const
    {
        return m_productFloor;
    }
    const Mesh &backgroundTriangle() const
    {
        return m_backgroundTriangle;
    }
    const QByteArray &vertexBytes() const
    {
        return m_vertexBytes;
    }
    const QByteArray &indexBytes() const
    {
        return m_indexBytes;
    }

private:
    Mesh m_lid, m_spine, m_tray, m_paper, m_floor, m_productFloor, m_backgroundTriangle;
    QByteArray m_vertexBytes, m_indexBytes;
};
}
