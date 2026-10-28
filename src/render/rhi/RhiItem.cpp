#include "RhiItem.h"
#include "RhiObservation.h"
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
    require(opticalProbesAvailable() || input.opticalProbe == 0,
            "Optical math probes require a Review build");
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
    return createSceneRenderer(this);
}
}
