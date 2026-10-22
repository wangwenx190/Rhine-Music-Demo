#include "SceneGeometryData.h"
#include "PreparedAssets.h"
#include "Units.h"
#include "RenderData.h"
#include <algorithm>

namespace yalahoo::rhi {
namespace {
constexpr auto EmptyBoundsMagnitude = 1e6f;
constexpr auto ReviewFloorDepth = .05f;
constexpr auto ProductFloorHalfExtent = 100;
constexpr auto ProductFloorDepth = 4.63f;
constexpr int ReviewFloorHalfExtent = 6;
}
SceneGeometryData::SceneGeometryData()
{
    m_vertexBytes.clear();
    m_indexBytes.clear();
    auto append = [&](const assets::Batch &batch) {
        const auto base = quint32(m_vertexBytes.size() / renderData::VertexBytes);
        Mesh mesh{ quint32(m_indexBytes.size() / renderData::FloatBytes),
                   quint32(batch.indices.size()) };
        QVector3D minimum(EmptyBoundsMagnitude, EmptyBoundsMagnitude, EmptyBoundsMagnitude),
                maximum(-EmptyBoundsMagnitude, -EmptyBoundsMagnitude, -EmptyBoundsMagnitude);
        for (const auto &v : batch.vertices)
            for (int axis = 0; axis < yalahoo::units::VectorAxes; ++axis) {
                minimum[axis] = std::min(minimum[axis], v[axis]);
                maximum[axis] = std::max(maximum[axis], v[axis]);
            }
        mesh.center = (minimum + maximum) * .5f;
        m_vertexBytes.append(reinterpret_cast<const char *>(batch.vertices.constData()),
                             batch.vertices.size() * renderData::VertexBytes);
        for (auto index : batch.indices) {
            index += base;
            m_indexBytes.append(reinterpret_cast<const char *>(&index), renderData::FloatBytes);
        }
        return mesh;
    };
    const auto &batches = preparedAssets().geometry;
    m_lid = append(batches[0]);
    m_spine = append(batches[1]);
    m_tray = append(batches[2]);
    assets::Batch quad;
    quad.vertices = {
        { { -yalahoo::rhi::renderData::PaperLeftMagnitude, yalahoo::rhi::renderData::PaperBottom,
            yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 0, 1 } },
        { { yalahoo::rhi::renderData::PaperRight, yalahoo::rhi::renderData::PaperBottom,
            yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 1, 1 } },
        { { yalahoo::rhi::renderData::PaperRight, yalahoo::rhi::renderData::PaperTop,
            yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 1, 0 } },
        { { -yalahoo::rhi::renderData::PaperLeftMagnitude, yalahoo::rhi::renderData::PaperTop,
            yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 0, 0 } }
    };
    quad.indices = { 0, 1, 2, 0, 2, 3 };
    m_paper = append(quad);
    quad.vertices = {
        { { -ReviewFloorHalfExtent, -ReviewFloorDepth, ReviewFloorHalfExtent, 0, 1, 0, 0, 0 } },
        { { ReviewFloorHalfExtent, -ReviewFloorDepth, ReviewFloorHalfExtent, 0, 1, 0, 1, 0 } },
        { { ReviewFloorHalfExtent, -ReviewFloorDepth, -ReviewFloorHalfExtent, 0, 1, 0, 1, 1 } },
        { { -ReviewFloorHalfExtent, -ReviewFloorDepth, -ReviewFloorHalfExtent, 0, 1, 0, 0, 1 } }
    };
    m_floor = append(quad);
    // PlaneGeometry(200, 200), rotation.x=-PI/2, position.y=-4.63.
    // Baked once into the shared immutable geometry; never a shadow caster.
    quad.vertices = {
        { { -ProductFloorHalfExtent, -ProductFloorDepth, ProductFloorHalfExtent, 0, 1, 0, 0, 0 } },
        { { ProductFloorHalfExtent, -ProductFloorDepth, ProductFloorHalfExtent, 0, 1, 0, 1, 0 } },
        { { ProductFloorHalfExtent, -ProductFloorDepth, -ProductFloorHalfExtent, 0, 1, 0, 1, 1 } },
        { { -ProductFloorHalfExtent, -ProductFloorDepth, -ProductFloorHalfExtent, 0, 1, 0, 0, 1 } }
    };
    m_productFloor = append(quad);
    quad.vertices = { { { -1, -1, 0, 0, 0, 1, 0, 0 } },
                      { { 3, -1, 0, 0, 0, 1, 2, 0 } },
                      { { -1, 3, 0, 0, 0, 1, 0, 2 } } };
    quad.indices = { 0, 1, 2 };
    m_backgroundTriangle = append(quad);
}
}
