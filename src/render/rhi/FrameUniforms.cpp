#include "FrameUniforms.h"
#include "RhiApi.h"
#include "SceneMath.h"
#include "Units.h"
#include "ShellMaterial.h"
#include <algorithm>
#include <cmath>

namespace yalahoo::rhi {
namespace {
constexpr float ShadowStrength = .32f;
// Geometry and source lighting parameters retain their original float precision at GPU upload.
constexpr auto ShadowNormalBias = .035f;
constexpr auto ShadowDepthBias = .0003f;
constexpr auto RibbonLaneWidth = 27;
constexpr auto GaussianHalfMaximumExponent = .6931471805599453f;
constexpr auto LightDistanceHeight = 3.35f;
constexpr auto LightDistanceDepth = 5.5f;
constexpr auto MinimumPrintAmbient = .08f;
constexpr auto AreaModeBoundary = 1.5f;
constexpr auto HybridModeBoundary = 2.5f;
constexpr auto ShelfAmbientBlend = .25f;
constexpr auto CoreLaneWidth = 3.2f;
constexpr auto LaneFieldPower = 4.0f;
constexpr auto SpillLaneWidth = 7.2f;
constexpr auto HotRowWidth = 1.1f;
constexpr auto NeighborRowWidth = 2.4f;
constexpr auto CoreLightGain = .58f;
constexpr auto SpillLightGain = .42f;
constexpr auto CoreAmbientFloor = .12f;

constexpr float FloorRoughness = .95f;
constexpr int ShadowFilterRadius = 4;
constexpr int LightDistanceWidth = 6;
}
void FrameUniforms::prepare(const Input &input, const QMatrix4x4 &clipCorrection, bool yUp,
                            bool zeroToOne, QSize targetSize, int shadowSize,
                            const QVector<QMatrix4x4> &entities, const QVector<bool> &visible,
                            bool batched)
{
    nativeVP = clipCorrection * input.scene.projection * input.scene.view;
    canonicalLightVP = lightViewProjection();
    nativeLightVP = clipCorrection * canonicalLightVP;
    if (input.shells || !batched) {
        lightModels.resize(entities.size());
        for (int i = 0; i < entities.size(); ++i)
            if (visible[i] || !batched)
                lightModels[i] = nativeLightVP * entities[i];
    }
    for (int kind = ShadowKind; kind <= ProductFloorKind; ++kind)
        uniformBases[kind + 1] =
                uniformTemplate(input, kind, yUp, zeroToOne, targetSize, shadowSize);
    const QVector4D printSettings(input.printAmbient, input.rowDetail, input.rowFill, 0);
    if (lightEntities != entities || cachedRowSettings != input.rowSettings
        || cachedPrintSettings != printSettings || cachedRowColumn != input.rowColumn
        || cachedLightColumn != input.lightColumn) {
        entityLight0.clear();
        entityLight1.clear();
        entityLight0.reserve(entities.size());
        entityLight1.reserve(entities.size());
        for (const auto &model : entities) {
            const auto origin = model.column(yalahoo::units::VectorAxes).toVector3D();
            // A zero ribbon width is the valid disabled/default configuration. Avoid 0/0
            // even when the shader will not use the field in the current lighting mode.
            float field = 0;
            if (input.rowSettings.z() > 0) {
                const float row = (origin.z() - input.rowColumn.z()
                                   - input.rowSettings.w() * sceneGeometry::RowSpacing)
                        / (.5f * input.rowSettings.z() * sceneGeometry::RowSpacing);
                const float lane = (origin.x() - input.rowColumn.x()) / RibbonLaneWidth;
                const float lane2 = lane * lane;
                field = std::exp(-GaussianHalfMaximumExponent * row * row)
                        * std::exp(-lane2 * lane2 * lane2);
            }
            const auto distance = (origin - input.lightColumn)
                    / QVector3D(LightDistanceWidth, LightDistanceHeight, LightDistanceDepth);
            const float printLight = std::exp(-QVector3D::dotProduct(distance, distance));
            const auto mix = [](float a, float b, float t) {
                return a * (1 - t) + b * t;
            };
            const float baseline = mix(input.printAmbient, 1, printLight);
            const float ambient = std::max(MinimumPrintAmbient, input.printAmbient * .5f);
            const float fill = input.rowSettings.x() > AreaModeBoundary
                            && input.rowSettings.x() < HybridModeBoundary
                    ? input.rowFill
                    : 0;
            const float fieldExposure = mix(mix(ambient, baseline, fill), 1,
                                            std::clamp(input.rowSettings.y() * field, 0.0f, 1.0f));
            const float shelf = input.rowSettings.x() < AreaModeBoundary
                    ? mix(ambient, input.printAmbient, ShelfAmbientBlend)
                    : fieldExposure;
            const float printExposure =
                    input.rowSettings.x() < .5f ? baseline : mix(shelf, baseline, input.rowDetail);
            const float laneDistance = std::abs(origin.x() - input.lightColumn.x());
            const float core = std::exp(-std::pow(laneDistance / CoreLaneWidth, LaneFieldPower));
            const float spill = std::exp(-std::pow(laneDistance / SpillLaneWidth, LaneFieldPower));
            const float z = origin.z() - input.lightColumn.z();
            const float rowLight = std::exp(-std::pow(z / LightDistanceDepth, 2.0f));
            const float hot = std::exp(-std::pow(z / HotRowWidth, 2.0f));
            const float neighbor = std::exp(-std::pow(z / NeighborRowWidth, 2.0f));
            entityLight0.append({ field, printExposure,
                                  CoreLightGain * core + SpillLightGain * spill,
                                  CoreLightGain * core * mix(CoreAmbientFloor, 1, rowLight)
                                          + SpillLightGain * spill * neighbor });
            entityLight1.append({ hot, 0, 0, 0 });
        }
        lightEntities = entities;
        cachedRowSettings = input.rowSettings;
        cachedPrintSettings = printSettings;
        cachedRowColumn = input.rowColumn;
        cachedLightColumn = input.lightColumn;
    }
}
Uniform FrameUniforms::uniformTemplate(const Input &input, int kind, bool yUp, bool zeroToOne,
                                       QSize targetSize, int shadowSize) const
{
    Uniform u{ };
    copy(u.view, input.scene.view);
    copy(u.lightVP, canonicalLightVP);
    copy(u.background, input.background);
    copy(u.fog,
         QVector4D(float(input.scene.fogNear), float(input.scene.fogFar), input.fogEnabled ? 1 : 0,
                   input.filterCovers ? 1 : 0));
    copy(u.rowSettings, input.rowSettings);
    copy(u.rowColumn, input.rowColumn);
    copy(u.lightColumn, input.lightColumn);
    copy(u.spotPosition, input.spotPosition);
    copy(u.spotTarget, input.spotTarget);
    copy(u.spotColor, input.spotColor);
    copy(u.keyColor, input.keyColor);
    copy(u.skyColor, input.skyColor);
    copy(u.groundColor, input.groundColor);
    copy(u.lightIntensities, input.lightIntensities, float(targetSize.height()));
    copy(u.settings,
         QVector4D(input.printAmbient, input.rowDetail, input.rowFill, input.environmentIntensity));
    copy(u.clip,
         QVector4D(float(input.scene.nearPlane), float(input.scene.farPlane), yUp ? 1 : 0,
                   zeroToOne ? 1 : 0));
    copy(u.draw,
         QVector4D(float(kind), input.shadows ? 1 : 0, float(shadowSize), ShadowFilterRadius));
    copy(u.optics,
         QVector4D(input.spotIntensity, ShadowStrength, ShadowNormalBias, -ShadowDepthBias));
    copy(u.nativeVP, nativeVP);
    copy(u.camera, input.scene.camera.position.qt(), input.exposure);
    if (kind == ProductFloorKind) {
        copy(u.tint, input.floorColor);
        copy(u.material, QVector4D(0, FloorRoughness, 0, 0));
        copy(u.coat, QVector4D(0, 0, 0, input.environmentIntensity));
    } else {
        const auto material = shellMaterial(std::clamp(kind - LidKind, 0, 2), input.warmth,
                                            input.clarity, input.environmentIntensity);
        copy(u.tint, material.tint);
        copy(u.attenuation, material.attenuation);
        copy(u.material, material.transport);
        copy(u.coat, material.coat);
    }
    copy(u.areaPosition, input.areaPosition, input.areaIntensity);
    copy(u.areaTarget, input.areaTarget, input.lightingEnabled ? 1 : 0);
    copy(u.scatter, input.scatterColor, input.scatterStrength);
    copy(u.coverUv, QVector4D(0, 0, 1, 1));
    const QSize pixels = input.atlasGeneration
            ? QSize(renderData::CoverTilePixels, renderData::CoverTilePixels)
            : input.cover.size();
    copy(u.paperSize,
         QVector4D(float(pixels.width()), float(pixels.height()), input.coverPainted ? 1 : 0,
                   float(input.opticalProbe)));
    return u;
}
Uniform FrameUniforms::uniform(const Input &input, const QMatrix4x4 &model, int kind, int entity,
                               const QVector<QVector4D> &entityCoverUv) const
{
    auto u = uniformBases[kind + 1];
    // mvp remains reserved in the std140 ABI; both geometry paths reuse world position.
    copy(u.model, model);
    copy(u.lightMvp, entity >= 0 ? lightModels[entity] : nativeLightVP * model);
    if (input.atlasGeneration && entity >= 0)
        copy(u.coverUv, entityCoverUv[entity]);
    if (kind == LidKind && entity >= 0 && input.scene.individualMaterials
        && entity < input.scene.instances.size()) {
        // Only the lid roughness depends on clarity. Preserve the cached material
        // template and all other optical parameters; no QColor or resource rebuild.
        u.material[1] =
                std::clamp(lidRoughness(input.warmth, float(input.scene.instances[entity].clarity)),
                           0.0f, 1.0f);
    }
    if (entity >= 0) {
        copy(u.originLight0, entityLight0[entity]);
        copy(u.originLight1, entityLight1[entity]);
    }
    return u;
}
}
