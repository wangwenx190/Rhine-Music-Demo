// Private render-thread state shared with externally compiled capture observers.
#pragma once
#include "RhiRenderer.h"
#include "SceneGeometryData.h"
#include "ScenePipeline.h"
#include "ReadbackQueue.h"
#include <QScopeGuard>
#include "FrameUniforms.h"
#include "SceneGeometry.h"
#include "SceneMath.h"
#include "Units.h"
#include "RenderData.h"
#include "RhiItem.h"
#include "Logging.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include "ResourcePlan.h"
#include <QPointer>
#include <QQuickWindow>
#include "MonotonicTime.h"
#include <limits>
#include "RhiApi.h"
#include "Postprocess.h"
#include "GeometryData.h"
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <QtEndian>
#include <QElapsedTimer>
#include <cstring>
#include <utility>
#include <algorithm>
#include <cstdio>

namespace yalahoo::rhi::renderDetail {
enum FrameBinding {
    FrameUniformBinding,
    CoverBinding,
    EnvironmentBinding,
    ShadowBinding,
    CaptureBinding,
    DfgBinding,
    LtcFirstBinding,
    LtcSecondBinding
};
template <class T>
using Owned = std::unique_ptr<T>;
struct ShellDraw final
{
    Mesh mesh;
    quint32 offset;
    float depth;
    quint64 identity;
    int surface;
    int entity;
};
extern SceneSubmissionStats submissionStats;
void queueRenderFailure(const QPointer<RhiItem> &guard, const QString &message);
class Renderer final : public QQuickRhiItemRenderer
{
    Q_DISABLE_COPY_MOVE(Renderer)
public:
    explicit Renderer(RhiItem *receiver, std::function<void()> outputPreparation)
        : prepareOutput(std::move(outputPreparation))
    {
        const QPointer<RhiItem> guard(receiver);
        failed = [guard](const QString &message) {
            queueRenderFailure(guard, message);
        };
        ++submissionStats.renderers;
    }
    ~Renderer() override
    {
        --submissionStats.renderers;
        readbacks.finish(api);
    }
    // Synchronization runs with GUI state blocked: copy one coherent revision here.
    // Render callbacks use only that value snapshot and owned native resources.
    void synchronize(QQuickRhiItem *item) override
    {
        QElapsedTimer sync;
        auto *source = static_cast<RhiItem *>(item);
        if (source->profiled || source->diagnosed)
            sync.start();
        if (source->submission() == synchronizedSubmission)
            return;
        synchronizedSubmission = source->submission();
        if (source->input().sceneOccluded) {
            input.sceneOccluded = true;
            input.preparationToken = 0;
            return;
        }
        prepared = source->prepared;
        const auto &sourceCover = source->input().cover;
        if (coverSourceKey != sourceCover.cacheKey()) {
            coverSourceKey = sourceCover.cacheKey();
            detachedCover = source->input().immutableCover ? sourceCover : sourceCover.copy();
            if (source->input().immutableCover)
                ++coverSnapshotShares;
            else
                ++coverSnapshotCopies;
            coverDirty = true;
        }
        input = source->input();
        deliver = source->captured;
        profile = source->profiled;
        diagnose = source->diagnosed;
        distinctThread = QThread::currentThread() != source->thread();
        // Keep one owned cover snapshot per revision. The queue owns accepted tile pixels;
        // rejected/replayed uploads need no deep copy while the GUI is blocked in synchronize.
        input.cover = detachedCover;
        if (input.atlasGeneration && atlasGeneration == input.atlasGeneration) {
            try {
                enqueueAtlas();
            } catch (const std::runtime_error &error) {
                reportFailure(error.what());
            }
        }
        synchronizationMs = sync.isValid()
                ? sync.nsecsElapsed() / yalahoo::units::NanosecondsPerMillisecond
                : 0;
    }
    // Rebuild only when native resources or attachment compatibility change.
    // Initialization validates the resource plan before submitting any partial graph.
    void initialize(QRhiCommandBuffer *cb) override
    {
        if (failedRenderer)
            return;
        try {
            if (prepareOutput)
                prepareOutput();
            else
                prepareOutputTarget();
            if (!input.sceneOccluded)
                prepareResources(cb);
        } catch (const std::exception &error) {
            reportFailure(error.what());
        }
    }
    // Keep pass dependencies ordered and retain capture state until real readback completion.
    // Submission timing alone never establishes GPU completion or display presentation.
    void render(QRhiCommandBuffer *cb) override
    {
        if (failedRenderer)
            return;
        readbacks.retire();
        // Pending captures still need QRhi frames when the scene only clears or has no cover.
        const auto continueReadbacks = qScopeGuard([this] {
            if (readbacks.drain())
                update();
        });
        if (input.sceneOccluded) {
            cb->beginPass(itemOutput.rt.get(), Qt::transparent, { 1, 0 });
            cb->endPass();
            ++submissionStats.occluded;
            return;
        }
        try {
            renderFrame(cb);
        } catch (const std::exception &error) {
            reportFailure(error.what());
        }
    }

private:
    void prepareCoverTexture(QSize coverSize);
    void replaceTextureBinding(int binding, QRhiTexture *texture, QRhiSampler *sampler);
    void prepareShadowResources();
    void prepareShellPipelines();
    void drawShellPass(QRhiCommandBuffer *cb, const QVector<ShellDraw> &shells,
                       const QVector<bool> &visible, bool batched, quint32 batchGeometryOffset,
                       int visibleCount, bool recordCapture, QJsonArray &opticalEvents);
    void drawShellVisibility(QRhiCommandBuffer *cb, const QVector<ShellDraw> &shells,
                             const QVector<bool> &visible, bool batched,
                             quint32 batchGeometryOffset, int visibleCount);
    void drawShadowPass(QRhiCommandBuffer *cb, bool batched, quint32 batchGeometryOffset,
                        const QVector<QMatrix4x4> &entities, const QVector<quint32> &shadowOffsets);

    void writeInstanceData(QByteArray &instances, int &instanceCursor,
                           const QVector<QMatrix4x4> &entities, int i);
    void drawOpaquePass(QRhiCommandBuffer *&cb, const bool &recordCapture,
                        QJsonArray &opticalEvents, auto &opaqueMeshDraw, const quint32 &bgOffset,
                        const quint32 &floorOffset, const bool &batched, auto &opaqueInstanceDraw,
                        const quint32 &batchGeometryOffset, const quint32 &batchPaperOffset,
                        QVector<int> &opaqueOrder, const QVector<bool> &visible,
                        QVector<quint32> &opaqueOffsets, Target &target);
    void drawGeometryPass(QRhiCommandBuffer *&cb, const quint32 &bgOffset,
                          const quint32 &floorOffset, const bool &batched,
                          const quint32 &batchGeometryOffset, const int &visibleCount,
                          QVector<int> &opaqueOrder, const QVector<bool> &visible,
                          QVector<quint32> &prepassOffsets, Target &target);

    void prepareOutputTarget();
    void prepareResources(QRhiCommandBuffer *cb);
    void renderFrame(QRhiCommandBuffer *cb);
    QJsonObject resourceInventory();
    void configureVisibilityProbe();
    void inspectCapture(Capture &capture);
    void captureFrame(QRhiCommandBuffer *cb, bool batched, int visibleCount, int cursor,
                      const QByteArray &bytes, quint32 floorOffset, bool shadowRendered,
                      QJsonArray fogUniforms, const float *out, const QJsonArray &captureKinds,
                      const QJsonArray &opticalEvents, const QVector<QMatrix4x4> &entities);
    void publishProfile(QRhiCommandBuffer *cb, double cpuMs, int cursor, bool shadowRendered,
                        int visibleCount);
    void reportFailure(const char *message)
    {
        qCCritical(logRender) << "RHI rendering failed"
                              << "backend=" << (api ? api->backendName() : "uninitialized")
                              << "submission=" << synchronizedSubmission
                              << "target=" << (itemOutput.rt ? itemOutput.rt->pixelSize() : QSize())
                              << "error=" << message;
        if (failedRenderer)
            return;
        failedRenderer = true;
        if (failed)
            failed(QString::fromUtf8(message));
    }
    std::function<void()> prepareOutput;
    std::function<void(QString)> failed;
    std::function<void(quint64, quint64, bool)> prepared;
    bool failedRenderer = false;
    qint64 previousResourceBytes = 0;
    quint64 inventoryGeneration = 0, inventoryPostRevision = 0, inventoryCoverRevision = 0;
    quint64 coverResourceRevision = 0, shadowResourceRevision = 0;
    QJsonObject cachedInventory, allocationPlan;
    struct AllocationKey final
    {
        QSize size, cover;
        int shadow = 0, samples = 0, aoSamples = 0, outputSamples = 0;
        bool shells = false, diagnostic = false, postprocess = false, bokeh = false, smaa = false,
             filterCovers = false;
        double navigationDpr = 0;
        quint64 textResourceRevision = 0;
        bool operator==(const AllocationKey &) const = default;
    };
    AllocationKey allocationKey;
    QSize plannedOpeningLayer, plannedPanelLayer, plannedDisplayLayer;
    bool plannedHdr = false;
    void reset();
    void resetTargetPipelines();
    void enqueueAtlas();
    void drawMesh(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *pipeline,
                  QRhiShaderResourceBindings *bindings, Mesh mesh, quint32 offset);
    void drawInstances(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *pipeline,
                       QRhiShaderResourceBindings *bindings, Mesh mesh, quint32 offset, int count);
    QRhi *api = nullptr;
    Input input;
    quint64 synchronizedSubmission = 0;
    double synchronizationMs = 0;
    std::function<void(Capture)> deliver;
    std::function<void(QJsonObject)> profile;
    std::function<void(DiagnosticSample)> diagnose;
    OutputTarget itemOutput;
    Target shadow, opaque, composite, aoGeometry, dofGeometry;
    Postprocess post;
    AtlasUploadQueue atlasQueue;
    QVector<quint64> expectedCoverLeases;
    QVector<QRect> expectedCoverTiles;
    QVector<quint64> expectedFallbackLeases, effectiveCoverLeases;
    QVector<QRect> expectedFallbackTiles, effectiveCoverTiles;
    quint64 expectedAtlasGeneration = 0;
    Owned<QRhiBuffer> vertices, indices, uniforms, outputUniform;
    Owned<QRhiBuffer> instanceBuffer;
    Owned<QRhiTexture> cover, environment, dfg, ltc1, ltc2;
    ReadbackQueue readbacks;
    quint64 inventoryReadbackRevision = 0;
    Owned<QRhiSampler> linear, nearest, mipLinear, shadowCompare;
    Owned<QRhiShaderResourceBindings> geometryBindings, shadowBindings, outputBindings;
    Owned<QRhiGraphicsPipeline> shadowPipeline, opaquePipeline, backgroundPipeline, outputPipeline;
    Owned<QRhiShaderResourceBindings> shellBindings;
    Owned<QRhiGraphicsPipeline> shellPipeline;
    Owned<QRhiGraphicsPipeline> coveragePipeline;
    Owned<QRhiGraphicsPipeline> copyPipeline;
    Owned<QRhiShaderResourceBindings> copyBindings;
    Owned<QRhiGraphicsPipeline> prepassPipeline;
    Owned<QRhiGraphicsPipeline> prepassBackgroundPipeline;
    Owned<QRhiGraphicsPipeline> opaqueInstances, shadowInstances, prepassInstances,
            coverageInstances;
    std::optional<SceneGeometryData> geometry;
    QSize targetSize;
    int shadowSize = 0, uniformStride = 0, captureSamples = 0;
    bool targetsHaveShells = false;
    bool areaShader = false, opticsProbeShader = false, visibilityProbeShader = false;
    bool diagnosticTargets = false;
    FrameUniforms frameUniforms;
    // Render-thread scratch never escapes to Input or queued captures. Unique containers
    // retain their bounded capacity across frames, independently of immutable GUI snapshots.
    QVector<bool> visible;
    QVector<int> opaqueOrder;
    QVector<float> depths;
    QVector<ShellDraw> shells;
    QVector<QMatrix4x4> viewModels;
    QVector<QMatrix4x4> standaloneEntities;
    QVector<QVector4D> entityCoverUv;
    QVector<quint32> shadowOffsets, opaqueOffsets, prepassOffsets;
    QByteArray uniformScratch, instanceScratch;
    QImage detachedCover;
    bool uploadGeometry = true;
    bool shadowDirty = true, cachedShadows = false;
    QVector<QMatrix4x4> shadowEntities;
    bool coverDirty = true;
    bool distinctThread = false;
    quint64 capturedRequest = 0, generation = 0;
    qint64 coverSourceKey = 0;
    quint64 coverSnapshotCopies = 0;
    quint64 coverSnapshotShares = 0;
    qint64 selectedUploadBytes = 0, selectedTotalUploadBytes = 0;
    quint64 atlasGeneration = 0;
    qint64 frameUploadBytes = 0, totalUploadBytes = 0;
    QJsonArray uploadRegions;
    QSize aoGeometrySize;
    bool postTargets = false;
    bool aoTargets = false, dofTargets = false;
    QRhiTexture *outputSource = nullptr;
    QRhiRenderPassDescriptor *outputPass = nullptr;
    int outputSamples = 0;
    quint64 outputPostRevision = 0;
};
}
