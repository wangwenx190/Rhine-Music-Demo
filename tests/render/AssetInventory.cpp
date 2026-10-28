// Reports platform adapter facts and immutable resource payloads for native regressions.
#include "RhiItem.h"
#include "PreparedAssets.h"
#include <QQuickWindow>
#include <QJsonArray>
#if defined(Q_OS_WIN)
#  include <rhi/qrhi_platform.h>
#endif
using namespace Qt::StringLiterals;
namespace yalahoo::rhi {
QJsonArray adapterInventory()
{
    QJsonArray result;
#if defined(Q_OS_WIN)
    QRhi::AdapterList adapters;
    if (QQuickWindow::graphicsApi() == QSGRendererInterface::Direct3D11) {
        QRhiD3D11InitParams params;
        adapters = QRhi::enumerateAdapters(QRhi::D3D11, &params);
    }
#  if QT_CONFIG(direct3d12)
    else if (QQuickWindow::graphicsApi() == QSGRendererInterface::Direct3D12) {
        QRhiD3D12InitParams params;
        adapters = QRhi::enumerateAdapters(QRhi::D3D12, &params);
    }
#  endif
    int index = 0;
    for (const auto &adapter : adapters) {
        const auto info = adapter->info();
        result.append(QJsonObject{ { u"index"_s, index++ },
                                   { u"name"_s, QString::fromUtf8(info.deviceName) },
                                   { u"deviceId"_s, qint64(info.deviceId) },
                                   { u"vendorId"_s, qint64(info.vendorId) },
                                   { u"deviceType"_s, int(info.deviceType) } });
    }
    qDeleteAll(adapters);
#endif
    return result;
}
QJsonObject preparedAssetStatistics()
{
    const auto &fixed = preparedAssets();
    qint64 bytePayload = 0, imagePayload = 0, meshPayload = 0;
    for (const auto &bytes : fixed.bytes)
        bytePayload += bytes.size();
    for (const auto &image : fixed.images)
        imagePayload += image.sizeInBytes();
    for (const auto &mesh : fixed.geometry)
        meshPayload += mesh.vertices.size() * sizeof(std::array<float, 8>)
                + mesh.indices.size() * sizeof(quint32);
    return {
        { u"bytePayloadBytes"_s, bytePayload },
        { u"imagePayloadBytes"_s, imagePayload },
        { u"meshPayloadBytes"_s, meshPayload },
        { u"shaderObjects"_s, fixed.shaders.size() },
        { u"allocatedHeapBytes"_s, QJsonValue::Null },
        { u"scope"_s,
          u"Immutable process cache payload; Qt container, shader internal storage and allocator overhead unknown. Separate from GPU descriptors and live cover/cache/lease/checkpoint counters."_s }
    };
}
}
