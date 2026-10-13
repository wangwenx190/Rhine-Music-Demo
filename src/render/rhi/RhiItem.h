#pragma once
#include "SceneFrame.h"
#include "AtlasUpload.h"
#include <QQuickRhiItem>
#include <QImage>
#include <QJsonObject>
#include <functional>
#include <memory>

namespace yalahoo::rhi {
QJsonArray adapterInventory();
// Values only. No QObject or QRhi ownership crosses the synchronization boundary.
struct Input
{
    SceneFrame scene;
    QVector<QMatrix4x4> entities;
    QVector<quint64> entityIds;
    QImage cover;
    bool coverPainted = false, immutableCover = false;
    QSize atlasSize;
    quint64 atlasGeneration = 0;
    QVector<AtlasUpload> atlasUploads;
    QVector<QRect> coverTiles;
    QVector<quint64> coverLeases;
    bool initializingAtlas = false;
    bool profile = false;
    bool postprocess = false, bokeh = false, smaa = false;
    int aoSamples = 0;
    float aoResolution = 1, focus = 25, aperture = .0003f;
    float aoRadius = .18f;
    QVector4D rowSettings;
    QVector3D rowColumn, lightColumn, spotPosition, spotTarget, spotColor;
    QVector3D keyColor, skyColor, groundColor, lightIntensities, background;
    float printAmbient = .5f, rowDetail = 0, rowFill = 1, spotIntensity = 0,
          environmentIntensity = .25f;
    int shadowSize = 2048;
    int captureSamples = 1;
    float warmth = 1, clarity = 0, exposure = 1.05f, scatterStrength = 1, areaIntensity = 0;
    QVector3D scatterColor, areaPosition, areaTarget;
    bool shells = false, showPaper = true, rawOutput = false;
    bool lightingEnabled = true;
    bool shadows = true, geometryDiagnostic = false, shadowDiagnostic = false;
    quint64 request = 0;
};
struct Capture
{
    quint64 request = 0;
    QImage color, normal, depth, shadow;
    QImage composite, compositeNormal, compositeDepth;
    QImage postprocessed, atlas;
    QImage rawAo, blurredAo, aoNormal, aoDepth;
    QJsonObject metadata;
};
// Native render boundary, shared by production and isolated Review tools.
class RhiItem : public QQuickRhiItem
{
    Q_OBJECT
public:
    explicit RhiItem(QQuickItem *parent = nullptr);
    void submit(Input input, QSize target);
    const Input &input() const
    {
        return m_input;
    }
    // The callable is copied during synchronize; callers use a guarded GUI receiver.
    std::function<void(Capture)> captured;
    std::function<void(QJsonObject)> profiled;
signals:
    void renderFailed(QString message);

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    Input m_input;
    quint64 m_submission = 0;

public:
    quint64 submission() const
    {
        return m_submission;
    }
};
}
