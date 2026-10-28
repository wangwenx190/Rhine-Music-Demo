#include "GeometryData.h"
#include "GeometryReader_p.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
using namespace Qt::StringLiterals;
namespace yalahoo::assets {
namespace {
using geometryDetail::Reader;
constexpr auto GlbMagic = 0x46546c67;
constexpr auto GlbJsonChunk = 0x4e4f534a;
constexpr auto GlbBinaryChunk = 0x004e4942;
constexpr auto GlbUnsignedShort = 5123;
constexpr auto GlbUnsignedInt = 5125;
constexpr auto GlbFloat = 5126;
constexpr auto GlbVersion = 2;
constexpr auto GlbTriangles = 4;

// Validate each accessor against both its buffer view and the binary payload before reading.
// The accepted layout is deliberately narrow: unsupported transforms or sparse data require explicit adaptation.
auto readGeometryAccessor(const QJsonArray &accessors, const QJsonArray &views,
                          const QByteArray &bin, int index, int components, int componentType)
{
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
    const int viewStart = view[u"byteOffset"_s].toInt(), localStart = a[u"byteOffset"_s].toInt();
    require(count > 0 && stride >= components * size && viewStart >= 0 && localStart >= 0,
            "Invalid accessor range");
    const qint64 required = qint64(count - 1) * stride + components * size;
    require(required <= qint64(view[u"byteLength"_s].toInt()) - localStart
                    && qint64(viewStart) + localStart + required <= bin.size(),
            "Accessor exceeds buffer");
    return std::array<qsizetype, 3>{ qsizetype(viewStart) + localStart, stride, count };
}
}
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
        return readGeometryAccessor(accessors, views, bin, index, components, componentType);
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
}
