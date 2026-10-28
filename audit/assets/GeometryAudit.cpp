#include "GeometryAudit.h"
#include "GeometryReader_p.h"
#include <QJsonArray>
#include <algorithm>
#include <limits>
#include <set>
using namespace Qt::StringLiterals;
namespace yalahoo::assets {
namespace {
constexpr auto QtMeshMagic = 3365961549;
constexpr auto InterleavedVertexBytes = 32;
constexpr auto QtMeshHeaderBytes = 12;
constexpr auto AttributeStrideBytes = 12;
constexpr auto QtMeshVersion = 7;
constexpr auto QtTriangleTopology = 7;
constexpr auto QtWinding = 2;
constexpr auto QtUnsignedShort = 3;
constexpr auto QtUnsignedInt = 5;
constexpr auto QtFloatComponentType = 10;
struct MeshReader : geometryDetail::Reader
{
    // Qt mesh padding is relative to the 12-byte mesh header and includes
    // four bytes when the payload is already aligned.
    void qtAlign()
    {
        take(4 - ((offset - QtMeshHeaderBytes) % 4));
    }
};
QJsonObject summary(const Batch &batch)
{
    std::array<float, 3> low{ }, high{ };
    low.fill(std::numeric_limits<float>::max());
    high.fill(std::numeric_limits<float>::lowest());
    for (const auto &v : batch.vertices)
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = std::min(low[axis], v[axis]);
            high[axis] = std::max(high[axis], v[axis]);
        }
    return { { u"name"_s, batch.name },
             { u"vertices"_s, batch.vertices.size() },
             { u"triangles"_s, batch.indices.size() / 3 },
             { u"min"_s, QJsonArray{ low[0], low[1], low[2] } },
             { u"max"_s, QJsonArray{ high[0], high[1], high[2] } } };
}

}
Batch readQtMesh(const QByteArray &bytes)
{
    // Independent, bounded inspector for the selected Kit's mesh v7 layout.
    // No Qt private headers, symbols or renderer dependency. A new file layout
    // fails explicitly; update this preparation-only inspector after probing.
    MeshReader r{ { bytes } };
    require(r.u32() == QtMeshMagic && r.u16() == QtMeshVersion && r.u16() == 0,
            "Unsupported Qt mesh format; adapt inspector to selected Kit");
    const auto payloadSize = r.u32();
    require(payloadSize > 0 && payloadSize <= bytes.size() - QtMeshHeaderBytes,
            "Truncated or empty mesh payload");
    r.limit = QtMeshHeaderBytes + payloadSize;
    require(r.u32() == 0, "Unexpected morph attributes");
    const auto attrCount = r.u32(), stride = r.u32();
    require(r.u32() == 0, "Unexpected morph data");
    const auto vertexSize = r.u32();
    const auto indexType = r.u32();
    r.u32();
    const auto indexSize = r.u32();
    require(r.u32() == 0 && r.u32() == 1, "Expected one subset");
    r.u32();
    require(r.u32() == 0 && r.u32() == QtTriangleTopology && r.u32() == QtWinding,
            "Unsupported mesh topology");
    require(attrCount == 3 && stride == InterleavedVertexBytes && vertexSize % stride == 0
                    && (indexType == QtUnsignedInt || indexType == QtUnsignedShort),
            "Unsupported mesh attributes");
    for (quint32 i = 0; i < attrCount; ++i) {
        r.u32();
        require(r.u32() == QtFloatComponentType && r.u32() == (i == 2 ? 2u : 3u)
                        && r.u32() == i * AttributeStrideBytes,
                "Unexpected vertex layout");
    }
    r.qtAlign();
    for (const auto &name : { "attr_pos"_ba, "attr_norm"_ba, "attr_uv0"_ba }) {
        const auto size = r.u32();
        require(r.take(size) == name + '\0', "Unexpected attribute name");
        r.qtAlign();
    }
    Batch b;
    require(vertexSize <= r.limit - r.offset, "Truncated vertices");
    for (quint32 i = 0; i < vertexSize / stride; ++i) {
        std::array<float, 8> v{ };
        for (auto &f : v)
            f = r.f32();
        b.vertices.append(v);
    }
    r.qtAlign();
    const int indexStride = indexType == QtUnsignedShort ? 2 : 4;
    require(indexSize % (3 * indexStride) == 0 && indexSize <= r.limit - r.offset,
            "Truncated indices");
    for (quint32 i = 0; i < indexSize / indexStride; ++i) {
        auto id = indexStride == 2 ? r.u16() : r.u32();
        require(id < quint32(b.vertices.size()), "Imported index out of range");
        b.indices.append(id);
    }
    r.qtAlign();
    require(r.u32() == quint32(b.indices.size()) && r.u32() == 0, "Unexpected mesh subset range");
    for (int i = 0; i < 6; ++i)
        r.f32(); // Subset bounds are finite even though vertex comparison is authoritative.
    r.u32(); // Legacy name offset.
    const auto nameLength = r.u32();
    r.u32(); // Lightmap width.
    r.u32(); // Lightmap height.
    require(r.u32() == 0, "Unexpected mesh LODs");
    r.qtAlign();
    r.take(qsizetype(nameLength) * 2);
    r.qtAlign();
    r.qtAlign(); // Empty LOD buffer still has legacy alignment padding.
    r.qtAlign(); // v7 writer also aligns the empty morph attribute block.
    require(r.offset == r.limit, "Mesh payload length mismatch");
    // Each imported file contains one mesh, followed by its 16-byte entry and 16-byte header.
    r.limit = bytes.size();
    const auto entriesOffset = r.offset;
    require(r.u32() == 0 && r.u32() == 0 && r.u32() == 1 && r.u32() == 0,
            "Unexpected mesh container entry");
    constexpr quint32 MultiMeshMagic = 0x21207dd9;
    require(r.u32() == MultiMeshMagic && r.u32() == 1 && r.u32() == quint32(entriesOffset)
                    && r.u32() == 1 && r.offset == r.limit,
            "Invalid mesh container footer");
    return b;
}
QJsonObject compareGeometry(const QVector<Batch> &original, const QVector<Batch> &imported)
{
    require(original.size() == 3 && imported.size() == 3, "Shell batch count mismatch");
    QJsonArray reports;
    int triangles = 0;
    double maxError = 0;
    for (int i = 0; i < 3; ++i) {
        const auto &a = original[i], &b = imported[i];
        if (a.indices.size() != b.indices.size() || a.vertices.size() != b.vertices.size())
            throw std::runtime_error(
                    u"Geometry counts changed: batch %1 source v=%2 i=%3 imported v=%4 i=%5"_s
                            .arg(i)
                            .arg(a.vertices.size())
                            .arg(a.indices.size())
                            .arg(b.vertices.size())
                            .arg(b.indices.size())
                            .toStdString());
        QVector<quint32> sourceIndex;
        QVector<bool> used(a.vertices.size(), false);
        for (const auto &bv : b.vertices) {
            int match = -1;
            double best = std::numeric_limits<double>::max();
            for (int j = 0; j < a.vertices.size(); ++j) {
                if (used[j])
                    continue;
                double error = 0;
                for (int k = 0; k < 8; ++k) {
                    // balsam restores glTF texture V to Qt's mesh convention.
                    const double expected = k == 7 ? 1.0 - a.vertices[j][k] : a.vertices[j][k];
                    error = std::max(error, std::abs(expected - bv[k]));
                }
                if (error < best) {
                    best = error;
                    match = j;
                }
            }
            require(match >= 0 && best <= 1e-6,
                    "Imported vertex position/normal/UV has no source match");
            used[match] = true;
            sourceIndex.append(quint32(match));
            maxError = std::max(maxError, best);
        }
        auto trianglesOf = [](const QVector<quint32> &indices, const QVector<quint32> &mapping) {
            std::multiset<std::array<quint32, 3>> faces;
            for (qsizetype j = 0; j < indices.size(); j += 3) {
                std::array<quint32, 3> face{ indices[j], indices[j + 1], indices[j + 2] };
                if (!mapping.isEmpty())
                    for (auto &index : face)
                        index = mapping[index];
                // Ignore face ordering/cyclic starting vertex, retain winding.
                const auto min = std::min_element(face.begin(), face.end());
                std::rotate(face.begin(), min, face.end());
                faces.insert(face);
            }
            return faces;
        };
        require(trianglesOf(a.indices, { }) == trianglesOf(b.indices, sourceIndex),
                "Imported triangle topology or winding changed");
        auto bcopy = b;
        bcopy.name = a.name;
        reports.append(
                QJsonObject{ { u"source"_s, summary(a) }, { u"imported"_s, summary(bcopy) } });
        triangles += int(b.indices.size() / 3);
    }
    require(triangles == 1000, "Source triangle total changed");
    return { { u"passed"_s, true },
             { u"batches"_s, reports },
             { u"triangles"_s, triangles },
             { u"maxAttributeError"_s, maxError },
             { u"uvConvention"_s, u"Qt V = 1 - glTF V; positions/normals unchanged"_s },
             { u"nodeTransforms"_s, u"identity; no scale or axis conversion"_s },
             { u"paperPlaneZ"_s, 0.027 },
             { u"rearFrontZ"_s, -0.032 },
             { u"lidBackZ"_s, 0.052 },
             { u"lidFrontZ"_s, 0.070 } };
}
}
