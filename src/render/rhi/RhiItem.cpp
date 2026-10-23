#include "RhiItem.h"
#include "RhiRenderer.h"
#include "RhiApi.h"
#include "RenderData.h"
#include "PreparedAssets.h"
#include "Logging.h"
#include <QQuickWindow>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

#if defined(Q_OS_WIN)
#  include <rhi/qrhi_platform.h>
#endif

namespace yalahoo::rhi {
namespace {
constexpr int MaximumOpticalProbe = 13;
constexpr int MinimumAoSamples = 16;
constexpr int MediumAoSamples = 32;
constexpr int MaximumAoSamples = 64;
}
void Input::setLighting(const SelectionLighting &l)
{
    background = l.backgroundLinear();
    keyColor = l.keyColor();
    skyColor = l.skyColor();
    groundColor = l.groundColor();
    lightIntensities = l.lightIntensities();
    spotPosition = l.spotPosition();
    spotTarget = l.spotTarget();
    spotColor = l.spotColor();
    spotIntensity = float(l.spotIntensity());
    environmentIntensity = float(l.environmentIntensity());
    rowSettings = l.rowSettings();
    rowColumn = l.rowColumn();
    rowDetail = float(l.rowDetail());
    rowFill = float(l.rowFill());
    lightColumn = l.lightColumn();
    printAmbient = float(l.printAmbient());
    scatterColor = l.scatterColor();
    scatterStrength = float(l.scatterStrength());
    areaPosition = l.areaPosition();
    areaTarget = l.areaTarget();
    areaIntensity = float(l.areaIntensity());
    warmth = float(l.warmth());
    exposure = float(l.exposure());
}
RhiItem::RhiItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    try {
        preparedAssets();
    } catch (const std::runtime_error &error) {
        qCFatal(yalahoo::logRender, "Invalid embedded RHI assets: %s", error.what());
    }
    setAlphaBlending(false);
    setSmooth(true);
    setAutoRenderTarget(false);
}
void RhiItem::setHdrHeadroom(float value)
{
    value = std::isfinite(value) ? std::max(1.0f, value) : 1;
    if (value == m_hdrHeadroom)
        return;
    m_hdrHeadroom = value;
    Q_EMIT hdrHeadroomChanged();
}
void RhiItem::setHdrActive(bool value)
{
    if (value == m_hdrActive)
        return;
    m_hdrActive = value;
    setColorBufferFormat(value ? TextureFormat::RGBA16F : TextureFormat::RGBA8);
    Q_EMIT hdrHeadroomChanged();
}
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
void RhiItem::submit(Input input, QSize target)
{
    require(input.fallbackCoverTiles.isEmpty()
                    || (input.fallbackCoverTiles.size() == input.entities.size()
                        && input.fallbackCoverLeases.size() == input.entities.size()
                        && input.coverLeases.size() == input.entities.size()),
            "Atlas fallback/entity mismatch");
    for (const auto &tile : std::as_const(input.fallbackCoverTiles))
        require(QRect(QPoint(), input.atlasSize).contains(tile)
                        && tile.size()
                                == QSize(renderData::CoverTilePixels, renderData::CoverTilePixels)
                        && tile.x() % renderData::CoverTilePixels == 0
                        && tile.y() % renderData::CoverTilePixels == 0,
                "Atlas fallback outside page");
    require(input.entities.size() <= renderData::MaximumCaseEntities,
            "RHI visible pool capacity exceeded");
    require(!input.cover.isNull() || input.atlasGeneration,
            "Missing cover at RHI synchronization boundary");
    require(input.aoSamples == 0 || input.aoSamples == MinimumAoSamples
                    || input.aoSamples == MediumAoSamples || input.aoSamples == MaximumAoSamples,
            "Invalid RHI AO kernel size");
    if (input.atlasGeneration) {
        require(m_input.atlasGeneration != input.atlasGeneration
                        || m_input.atlasSize == input.atlasSize,
                "Changing atlas dimensions requires a new generation");
        require(input.coverTiles.size() == input.entities.size(), "Atlas tile/entity mismatch");
        require(input.coverLeases.isEmpty() || input.coverLeases.size() == input.entities.size(),
                "Atlas lease/entity mismatch");
        require(input.atlasSize.width() > 0 && input.atlasSize.height() > 0
                        && input.atlasSize.width() % renderData::CoverTilePixels == 0
                        && input.atlasSize.height() % renderData::CoverTilePixels == 0,
                "Atlas size must use whole tiles");
        for (const auto &tile : std::as_const(input.coverTiles))
            require(QRect(QPoint(), input.atlasSize).contains(tile) && tile.width() == tile.height()
                            && tile.width() >= renderData::CoverTilePixels
                            && (tile.width() & (tile.width() - 1)) == 0
                            && tile.x() % renderData::CoverTilePixels == 0
                            && tile.y() % renderData::CoverTilePixels == 0,
                    "Atlas tile outside page");
    }
    require(target.width() > 0 && target.height() > 0, "Invalid RHI target size");
    require(input.captureSamples > 0, "Invalid RHI sample count");
    require(!input.fogEnabled
                    || (std::isfinite(input.scene.fogNear) && std::isfinite(input.scene.fogFar)
                        && input.scene.fogFar > input.scene.fogNear),
            "Invalid scene fog range");
    require(input.opticalProbe >= 0 && input.opticalProbe <= MaximumOpticalProbe
                    && (input.opticalProbe == 0 || input.shells),
            "Invalid optical math probe");
#ifndef YALAHOO_RHI_AUDIT
    require(input.opticalProbe == 0, "Optical math probes require a Review build");
#endif
    if (input.shells && !input.entities.isEmpty()) {
        require(input.entities.size() == 1 || input.entityIds.size() == input.entities.size(),
                "Shell instances require stable IDs");
        require(input.entityIds.isEmpty() || input.entityIds.size() == input.entities.size(),
                "Mismatched shell IDs");
        // Ordinary animation changes transforms while identities stay fixed. The previous
        // accepted shell snapshot proves uniqueness until the ID sequence changes. Inputs
        // without shell entities did not establish that invariant and cannot seed this cache.
        if (!m_input.shells || m_input.entities.isEmpty() || input.entityIds != m_input.entityIds) {
            std::array<quint64, renderData::MaximumCaseEntities> ids;
            const auto end =
                    std::copy(input.entityIds.cbegin(), input.entityIds.cend(), ids.begin());
            std::sort(ids.begin(), end);
            require(std::adjacent_find(ids.begin(), end) == end, "Duplicate shell instance ID");
        }
    }
    ++m_submission;
    m_input = std::move(input);
    setFixedColorBufferWidth(target.width());
    setFixedColorBufferHeight(target.height());
    update();
}
QQuickRhiItemRenderer *RhiItem::createRenderer()
{
    return createSceneRenderer();
}
}
