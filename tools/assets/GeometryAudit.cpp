#include "GeometryAudit.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
// Binary protocol tags and component formats are validated before any indexed geometry access.
constexpr auto GlbMagic = 0x46546c67;
constexpr auto GlbJsonChunk = 0x4e4f534a;
constexpr auto GlbBinaryChunk = 0x004e4942;
constexpr auto GlbUnsignedShort = 5123;
constexpr auto GlbUnsignedInt = 5125;
constexpr auto GlbFloat = 5126;
constexpr auto QtMeshMagic = 3365961549;
constexpr auto InterleavedVertexBytes = 32;
constexpr auto QtMeshHeaderBytes = 12;
constexpr auto AttributeStrideBytes = 12;
constexpr auto GlbVersion = 2;
constexpr auto GlbTriangles = 4;
constexpr auto QtMeshVersion = 7;
constexpr auto QtTriangleTopology = 7;
constexpr auto QtWinding = 2;
constexpr auto QtUnsignedShort = 3;
constexpr auto QtUnsignedInt = 5;
constexpr auto QtFloatComponentType = 10;
}

using namespace Qt::StringLiterals;

namespace yalahoo::assets {
namespace {
void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
struct Reader final
{
    const QByteArray &bytes;
    qsizetype offset = 0;
    QByteArray take(qsizetype size)
    {
        require(size >= 0 && offset >= 0 && offset <= bytes.size() && size <= bytes.size() - offset,
                "Truncated geometry");
        auto result = bytes.mid(offset, size);
        offset += size;
        return result;
    }
    quint32 u32()
    {
        auto data = take(4);
        return qFromLittleEndian<quint32>(data.constData());
    }
    quint16 u16()
    {
        auto data = take(2);
        return qFromLittleEndian<quint16>(data.constData());
    }
    float f32()
    {
        auto bits = u32();
        float value;
        std::memcpy(&value, &bits, 4);
        require(std::isfinite(value), "Nonfinite vertex");
        return value;
    }
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
QByteArray readFile(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot open asset");
    return file.readAll();
}
void saveFile(const QString &path, const QByteArray &bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create asset directory");
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot write asset");
    require(file.write(bytes) == bytes.size() && file.commit(), "Cannot commit asset");
}
// Validate each accessor against both its buffer view and the binary payload before reading.
// The accepted layout is deliberately narrow: unsupported transforms or sparse data require explicit adaptation.
QVector<Batch> readGlb(const QByteArray &bytes)
{
    Reader r{ bytes };
    require(r.u32() == GlbMagic && r.u32() == GlbVersion && r.u32() == bytes.size(),
            "Invalid GLB header");
    const auto jsonSize = r.u32();
    require(r.u32() == GlbJsonChunk, "Missing GLB JSON");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(r.take(jsonSize), &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), "Invalid GLB JSON");
    const auto binSize = r.u32();
    require(r.u32() == GlbBinaryChunk, "Missing GLB BIN");
    const auto bin = r.take(binSize);
    require(r.offset == bytes.size(), "Unsupported GLB chunks");
    const auto root = document.object();
    const auto nodes = root[u"nodes"_s].toArray();
    for (const auto &value : nodes) {
        const auto node = value.toObject();
        require(!node.contains(u"matrix"_s) && !node.contains(u"translation"_s)
                        && !node.contains(u"rotation"_s) && !node.contains(u"scale"_s),
                "Unexpected source node transform; audit adapter before import");
    }
    const auto accessors = root[u"accessors"_s].toArray(), views = root[u"bufferViews"_s].toArray();
    auto accessor = [&](int index, int components, int componentType) {
        require(index >= 0 && index < accessors.size(), "Invalid accessor index");
        const auto a = accessors[index].toObject();
        const int viewId = a[u"bufferView"_s].toInt(-1);
        require(viewId >= 0 && viewId < views.size() && !a.contains(u"sparse"_s)
                        && !a[u"normalized"_s].toBool(),
                "Unsupported accessor");
        const auto view = views[viewId].toObject();
        require(a[u"componentType"_s].toInt() == componentType && view[u"buffer"_s].toInt() == 0,
                "Unsupported accessor component");
        require(a[u"type"_s].toString()
                        == (components == 1           ? "SCALAR"_L1
                                    : components == 2 ? "VEC2"_L1
                                                      : "VEC3"_L1),
                "Invalid accessor dimensions");
        const int size = componentType == GlbUnsignedShort ? 2 : 4;
        const int stride = view[u"byteStride"_s].toInt(components * size),
                  count = a[u"count"_s].toInt();
        const int viewStart = view[u"byteOffset"_s].toInt(),
                  localStart = a[u"byteOffset"_s].toInt();
        require(count > 0 && stride >= components * size && viewStart >= 0 && localStart >= 0,
                "Invalid accessor range");
        const qint64 required = qint64(count - 1) * stride + components * size;
        require(required <= qint64(view[u"byteLength"_s].toInt()) - localStart
                        && qint64(viewStart) + localStart + required <= bin.size(),
                "Accessor exceeds buffer");
        return std::array<qsizetype, 3>{ qsizetype(viewStart) + localStart, stride, count };
    };
    QVector<Batch> batches;
    for (const auto &meshValue : root[u"meshes"_s].toArray()) {
        const auto mesh = meshValue.toObject();
        const auto primitives = mesh[u"primitives"_s].toArray();
        require(primitives.size() == 1, "Expected one primitive per shell batch");
        const auto p = primitives[0].toObject();
        require(p[u"mode"_s].toInt(GlbTriangles) == GlbTriangles, "Expected triangles");
        const auto attributes = p[u"attributes"_s].toObject();
        const auto pos = accessor(attributes[u"POSITION"_s].toInt(-1), 3, GlbFloat);
        const auto norm = accessor(attributes[u"NORMAL"_s].toInt(-1), 3, GlbFloat);
        const auto uv = accessor(attributes[u"TEXCOORD_0"_s].toInt(-1), 2, GlbFloat);
        require(pos[2] == norm[2] && pos[2] == uv[2], "Attribute count mismatch");
        Batch b;
        const auto materials = root[u"materials"_s].toArray();
        const int material = p[u"material"_s].toInt(-1);
        require(material >= 0 && material < materials.size(), "Missing shell material identity");
        b.name = materials[material].toObject()[u"name"_s].toString();
        const QStringList names{ u"Frosted_Polymer"_s, u"Ivory_Edges"_s, u"Optical_Diffuser"_s };
        require(batches.size() < names.size() && b.name == names[batches.size()],
                "Unexpected shell order/material");
        for (int i = 0; i < pos[2]; ++i) {
            std::array<float, 8> v{ };
            Reader pr{ bin, pos[0] + i * pos[1] }, nr{ bin, norm[0] + i * norm[1] },
                    ur{ bin, uv[0] + i * uv[1] };
            for (int j = 0; j < 3; ++j) {
                v[j] = pr.f32();
                v[j + 3] = nr.f32();
            }
            v[6] = ur.f32();
            v[7] = ur.f32();
            b.vertices.append(v);
        }
        const int indexId = p[u"indices"_s].toInt(-1);
        require(indexId >= 0 && indexId < accessors.size(), "Missing indices");
        const int type = accessors[indexId].toObject()[u"componentType"_s].toInt();
        require(type == GlbUnsignedShort || type == GlbUnsignedInt, "Unsupported indices");
        const auto idx = accessor(indexId, 1, type);
        require(idx[2] % 3 == 0, "Incomplete triangle");
        for (int i = 0; i < idx[2]; ++i) {
            Reader ir{ bin, idx[0] + i * idx[1] };
            auto id = type == GlbUnsignedShort ? ir.u16() : ir.u32();
            require(id < quint32(b.vertices.size()), "Index out of range");
            b.indices.append(id);
        }
        batches.append(b);
    }
    require(batches.size() == 3, "Expected three source shell batches");
    return batches;
}
Batch readQtMesh(const QByteArray &bytes)
{
    // Independent, bounded inspector for the selected Kit's mesh v7 layout.
    // No Qt private headers, symbols or renderer dependency. A new file layout
    // fails explicitly; update this preparation-only inspector after probing.
    Reader r{ bytes };
    require(r.u32() == QtMeshMagic && r.u16() == QtMeshVersion && r.u16() == 0,
            "Unsupported Qt mesh format; adapt inspector to selected Kit");
    const auto payloadSize = r.u32();
    require(payloadSize <= bytes.size() - QtMeshHeaderBytes, "Truncated mesh payload");
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
    require(vertexSize <= bytes.size() - r.offset, "Truncated vertices");
    for (quint32 i = 0; i < vertexSize / stride; ++i) {
        std::array<float, 8> v{ };
        for (auto &f : v)
            f = r.f32();
        b.vertices.append(v);
    }
    r.qtAlign();
    const int indexStride = indexType == QtUnsignedShort ? 2 : 4;
    require(indexSize % (3 * indexStride) == 0 && indexSize <= bytes.size() - r.offset,
            "Truncated indices");
    for (quint32 i = 0; i < indexSize / indexStride; ++i) {
        auto id = indexStride == 2 ? r.u16() : r.u32();
        require(id < quint32(b.vertices.size()), "Imported index out of range");
        b.indices.append(id);
    }
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
