// Owns native RHI rendering behind a synchronized value input. Rendering resources belong to the
// renderer and asynchronous captures retain their completion state.

#pragma once
#include "SelectionLighting.h"
#include "PostprocessDefaults.h"
#include "SceneFrame.h"
#include "DiagnosticSamples.h"
#include "AtlasUpload.h"
#include <QQuickRhiItem>
#include <QImage>
#include <QJsonObject>
#include <functional>
#include <memory>
#include <atomic>

namespace yalahoo::rhi {
namespace renderDefaults {
// Standalone input defaults follow the source day palette and camera.
inline constexpr auto ShadowSide = 2048;
}

QJsonArray adapterInventory();
QJsonObject preparedAssetStatistics();
// Values only. No QObject or QRhi ownership crosses the synchronization boundary.
struct SceneSubmissionStats final
{
    std::atomic<quint64> rendered{ 0 }, occluded{ 0 };
    std::atomic<int> renderers{ 0 };
};
SceneSubmissionStats &sceneSubmissionStats();
struct Input final
{
    bool sceneOccluded = false;
    quint64 preparationToken = 0;
    QSize openingLayerSize;
    QSize panelLayerSize;
    SceneFrame scene;
    double navigationDpr = 0;
    QList<QSize> textLayerSizes;
    quint64 textResourceRevision = 0;
    QVector<QMatrix4x4> entities;
    QVector<quint64> entityIds;
    QImage cover;
    QList<QImage> coverMipmaps;
    bool coverPainted = false, immutableCover = false, filterCovers = false;
    QSize atlasSize;
    quint64 atlasGeneration = 0;
    QVector<AtlasUpload> atlasUploads;
    QVector<QRect> coverTiles;
    QVector<quint64> coverLeases;
    QVector<QRect> fallbackCoverTiles;
    QVector<quint64> fallbackCoverLeases;
    bool initializingAtlas = false;
    bool profile = false;
    bool poolFrame = false;
    bool captureAtlas = true;
    bool captureFloorDepth = false; // Explicit Review readback only; production remains off.
    bool captureShadowDepth =
            false; // Explicit Review capture; shares existing R32F readback bytes.
    bool postprocess = false, bokeh = false, smaa = false;
    int aoSamples = 0;
    float focus = yalahoo::rhi::postDefaults::FocusDistance,
          aperture = yalahoo::rhi::postDefaults::Aperture;
    float aoRadius = yalahoo::rhi::postDefaults::AoRadius;
    QVector4D rowSettings;
    QVector3D rowColumn, lightColumn, spotPosition, spotTarget, spotColor;
    QVector3D keyColor, skyColor, groundColor, lightIntensities, background;
    float printAmbient = yalahoo::lightingDefaults::PrintAmbient, rowDetail = 0, rowFill = 1,
          spotIntensity = 0, environmentIntensity = yalahoo::lightingDefaults::Environment;
    int shadowSize = renderDefaults::ShadowSide;
    int captureSamples = 1;
    int opticalProbe = 0; // Review-only selector, rejected by production builds.
    float warmth = 1, clarity = 0, exposure = yalahoo::lightingDefaults::Exposure,
          scatterStrength = 1, areaIntensity = 0;
    QVector3D scatterColor, areaPosition, areaTarget;
    bool shells = false, showPaper = true, rawOutput = false;
    float hdrHeadroom = 1;
    bool hdrActive = false;
    QSize displayLayerSize;
    bool lightingEnabled = true;
    bool productFloor = false; // SceneItem enables the source ground, independent Review opts in.
    QVector3D floorColor; // Linear working color, owned by the theme controller.
    bool fogEnabled = false; // Independent Review fixtures opt in; production always enables fog.
    bool shadows = true, geometryDiagnostic = false, shadowDiagnostic = false;
    quint64 request = 0;
    void setLighting(const SelectionLighting &lighting);
};
struct Capture final
{
    quint64 request = 0;
    QImage color, normal, depth, shadow;
    QImage composite, compositeNormal, compositeDepth;
    QImage postprocessed, atlas;
    QImage rawAo, blurredAo, aoNormal, aoDepth, dofDepth;
    QByteArray shadowDepth;
    QJsonObject metadata;
};
// Native render boundary, shared by production and isolated Review tools.
class RhiItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RhiItem)
    Q_PROPERTY(float hdrHeadroom READ hdrHeadroom WRITE setHdrHeadroom NOTIFY hdrHeadroomChanged FINAL)
    Q_PROPERTY(bool hdrActive READ hdrActive WRITE setHdrActive NOTIFY hdrHeadroomChanged FINAL)
public:
    explicit RhiItem(QQuickItem *parent = nullptr);
    float hdrHeadroom() const
    {
        return m_hdrHeadroom;
    }
    void setHdrHeadroom(float value);
    bool hdrActive() const
    {
        return m_hdrActive;
    }
    void setHdrActive(bool value);
    void submit(Input input, QSize target);
    const Input &input() const
    {
        return m_input;
    }
    // The callable is copied during synchronize; callers use a guarded GUI receiver.
    std::function<void(Capture)> captured;
    std::function<void(QJsonObject)> profiled;
    std::function<void(DiagnosticSample)> diagnosed;
    std::function<void(quint64, quint64, bool)> prepared;
Q_SIGNALS:
    void hdrHeadroomChanged();
    void renderFailed(QString message);

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    float m_hdrHeadroom = 1;
    bool m_hdrActive = false;
    Input m_input;
    quint64 m_submission = 0;

public:
    quint64 submission() const
    {
        return m_submission;
    }
};
}
