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
#include "TransferAudit.h"
#include "GeometryAudit.h"
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <QtEndian>
#include <QElapsedTimer>
#include <cstring>
#include <utility>
#include <algorithm>
#include <cstdio>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
namespace {
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
#ifdef YALAHOO_RHI_AUDIT
constexpr int VisibilityProbe = 10;
#endif
constexpr int MinimumBatchedEntities = 8;
constexpr int ExtraGeometryVertices = 15;
constexpr int ExtraGeometryIndices = 21;

constinit SceneSubmissionStats submissionStats;
template <class T>
using Owned = std::unique_ptr<T>;
QByteArray floatTextureBytes(const QString &path, qsizetype expected)
{
    const auto bytes = preparedAssets().bytes.value(path);
    require(bytes.size() == expected, "Invalid RHI float lookup bytes");
    return bytes;
}
void visibleEntities(const Input &input, const QVector<QMatrix4x4> &entities,
                     QVector<bool> &visible)
{
    visible.resize(entities.size());
    const auto viewProjection = input.scene.projection * input.scene.view;
    // Keep every logical identity and its global sort, including off-screen shadow casters.
    for (int i = 0; i < entities.size(); ++i)
        visible[i] = caseIntersectsClip(viewProjection * entities[i]);
}
struct ShellDraw final
{
    Mesh mesh;
    quint32 offset;
    float depth;
    quint64 identity;
    int surface;
    int entity;
};
void queueRenderFailure(const QPointer<RhiItem> &guard, const QString &message)
{
    if (guard)
        QMetaObject::invokeMethod(
                guard,
                [guard, message] {
                    if (guard)
                        Q_EMIT guard->renderFailed(message);
                },
                Qt::QueuedConnection);
}
class Renderer final : public QQuickRhiItemRenderer
{
    Q_DISABLE_COPY_MOVE(Renderer)
public:
    Renderer()
    {
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
        QPointer<RhiItem> guard(source);
        failed = [guard](const QString &message) {
            queueRenderFailure(guard, message);
        };
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
    void reportFailure(const char *message)
    {
        qCCritical(logRender) << "RHI rendering failed"
                              << "backend=" << (api ? api->backendName() : "uninitialized")
                              << "submission=" << synchronizedSubmission
                              << "target=" << (itemOutput.rt ? itemOutput.rt->pixelSize() : QSize())
                              << "error=" << message;
        failedRenderer = true;
        if (failed)
            failed(QString::fromUtf8(message));
    }
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
void Renderer::enqueueAtlas()
{
    if (expectedAtlasGeneration != atlasGeneration || expectedCoverLeases != input.coverLeases
        || expectedCoverTiles != input.coverTiles
        || expectedFallbackTiles != input.fallbackCoverTiles
        || expectedFallbackLeases != input.fallbackCoverLeases) {
        for (int i = 0; i < input.coverLeases.size(); ++i) {
            const auto tile = std::as_const(input.coverTiles)[i];
            for (int y = tile.y(); y < tile.y() + tile.height(); y += renderData::CoverTilePixels)
                for (int x = tile.x(); x < tile.x() + tile.width();
                     x += renderData::CoverTilePixels)
                    atlasQueue.expect((y / renderData::CoverTilePixels)
                                                      * (input.atlasSize.width()
                                                         / renderData::CoverTilePixels)
                                              + x / renderData::CoverTilePixels,
                                      std::as_const(input.coverLeases)[i]);
        }
        expectedAtlasGeneration = atlasGeneration;
        for (int i = 0; i < input.fallbackCoverTiles.size(); ++i) {
            const auto tile = std::as_const(input.fallbackCoverTiles)[i];
            atlasQueue.expect(
                    (tile.y() / renderData::CoverTilePixels)
                                    * (input.atlasSize.width() / renderData::CoverTilePixels)
                            + tile.x() / renderData::CoverTilePixels,
                    std::as_const(input.fallbackCoverLeases)[i]);
        }
        expectedCoverLeases = input.coverLeases;
        expectedCoverTiles = input.coverTiles;
        expectedFallbackTiles = input.fallbackCoverTiles;
        expectedFallbackLeases = input.fallbackCoverLeases;
    }
    atlasQueue.enqueue(input.atlasUploads);
}
void Renderer::resetTargetPipelines()
{
    // Drop users before the target render passes are recreated. Shadow resources are independent.
    post.reset();
    prepassPipeline.reset();
    prepassBackgroundPipeline.reset();
    opaqueInstances.reset();
    prepassInstances.reset();
    coverageInstances.reset();
    outputSource = nullptr;
    outputPass = nullptr;
    shellPipeline.reset();
    coveragePipeline.reset();
    copyPipeline.reset();
    copyBindings.reset();
    shellBindings.reset();
    outputPipeline.reset();
    backgroundPipeline.reset();
    opaquePipeline.reset();
    outputBindings.reset();
    geometryBindings.reset();
    postTargets = false;
}
void Renderer::reset()
{
    readbacks.reset();
    resetTargetPipelines();
    itemOutput.reset();
    shadowInstances.reset();
    shadowPipeline.reset();
    shadowBindings.reset();
    opaque.reset();
    composite.reset();
    shadow.reset();
    aoGeometry.reset();
    dofGeometry.reset();
    uniforms.reset();
    uniformScratch.clear();
    outputUniform.reset();
    vertices.reset();
    indices.reset();
    instanceBuffer.reset();
    instanceScratch.clear();
    cover.reset();
    environment.reset();
    dfg.reset();
    ltc1.reset();
    ltc2.reset();
    mipLinear.reset();
    shadowCompare.reset();
    linear.reset();
    nearest.reset();
    targetSize = { };
    shadowSize = 0;
    uploadGeometry = true;
    shadowDirty = true;
    shadowEntities.clear();
    coverDirty = true;
    atlasGeneration = 0;
    expectedAtlasGeneration = 0;
    expectedCoverLeases.clear();
    expectedCoverTiles.clear();
}
void Renderer::prepareCoverTexture(QSize coverSize)
{
    ++coverResourceRevision;
    cover.reset(api->newTexture(
            QRhiTexture::RGBA8, coverSize, 1,
            QRhiTexture::UsedAsTransferSource
                    | (input.filterCovers ? QRhiTexture::MipMapped : QRhiTexture::Flags())));
    require(cover->create(), "Cover texture failed");
    coverDirty = true;
    atlasGeneration = input.atlasGeneration;
    if (atlasGeneration) {
        atlasQueue.reset(coverSize, atlasGeneration);
        enqueueAtlas();
    }
}
// Preserve PSOs when only a sampled image changes; the SRB layout remains compatible.
void Renderer::replaceTextureBinding(int binding, QRhiTexture *texture, QRhiSampler *sampler)
{
    for (auto *bindings : { geometryBindings.get(), shellBindings.get() }) {
        if (!bindings)
            continue;
        QVector<QRhiShaderResourceBinding> values(bindings->cbeginBindings(),
                                                  bindings->cendBindings());
        // Both scene SRBs declare the contiguous Frame/Cover/Environment/Shadow prefix.
        require((binding == CoverBinding || binding == ShadowBinding) && values.size() > binding,
                "Missing scene texture binding");
        QRhiShaderResourceBinding::StageFlags stages = QRhiShaderResourceBinding::FragmentStage;
        if (binding == CoverBinding)
            stages |= QRhiShaderResourceBinding::VertexStage;
        values[binding] =
                QRhiShaderResourceBinding::sampledTexture(binding, stages, texture, sampler);
        bindings->setBindings(values.cbegin(), values.cend());
        require(bindings->create(), "Sampled texture binding replacement failed");
    }
}
void Renderer::prepareShadowResources()
{
    const int requested = std::max(1, input.shadowSize);
    const bool diagnostic = bool(deliver);
    const bool replaceDepth = !shadow.hardwareDepth || shadowSize != requested;
    if (shadow.rt && !replaceDepth && bool(shadow.color) == diagnostic)
        return;
    shadowPipeline.reset();
    shadowInstances.reset();
    // Capture changes only the diagnostic color layout. Preserve the sampled D32F and its
    // cached contents; enabling capture redraws once to populate the new canonical-depth map.
    shadow.rt.reset();
    shadow.pass.reset();
    shadow.color.reset();
    shadowSize = requested;
    shadow.size = { shadowSize, shadowSize };
    if (replaceDepth) {
        shadow.hardwareDepth.reset(
                api->newTexture(QRhiTexture::D32F, shadow.size, 1, QRhiTexture::RenderTarget));
        require(shadow.hardwareDepth->create(),
                "Cannot create source-compatible shadow depth texture");
    }
    QRhiTextureRenderTargetDescription description;
    description.setDepthTexture(shadow.hardwareDepth.get());
    if (diagnostic) {
        shadow.color.reset(
                api->newTexture(QRhiTexture::R32F, shadow.size, 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        require(shadow.color->create(), "Cannot create shadow diagnostic color texture");
        description.setColorAttachments({ shadow.color.get() });
    }
    shadow.rt.reset(api->newTextureRenderTarget(description));
    shadow.pass.reset(shadow.rt->newCompatibleRenderPassDescriptor());
    shadow.rt->setRenderPassDescriptor(shadow.pass.get());
    require(shadow.rt->create(), "Cannot create shadow render target");
    if (!shadowBindings) {
        shadowBindings.reset(api->newShaderResourceBindings());
        const auto stages =
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
        shadowBindings->setBindings({ QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                FrameUniformBinding, stages, uniforms.get(), sizeof(Uniform)) });
        require(shadowBindings->create(), "Shadow bindings failed");
    }
    shadowPipeline =
            createScenePipeline(api, input, shadow, shadowBindings.get(), ScenePass::Shadow);
    shadowInstances = createScenePipeline(api, input, shadow, shadowBindings.get(),
                                          ScenePass::Shadow, InstanceMode::Instanced);
    if (replaceDepth)
        replaceTextureBinding(ShadowBinding, shadow.hardwareDepth.get(), shadowCompare.get());
    ++shadowResourceRevision;
    cachedInventory = { };
    shadowDirty = shadowDirty || replaceDepth || diagnostic;
}
void Renderer::prepareShellPipelines()
{
    const auto stages =
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;

    shellBindings.reset(api->newShaderResourceBindings());
    const auto fragment = QRhiShaderResourceBinding::FragmentStage;
    shellBindings->setBindings(
            { QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, stages, uniforms.get(),
                                                                        sizeof(Uniform)),
              QRhiShaderResourceBinding::sampledTexture(CoverBinding, stages, cover.get(),
                                                        linear.get()),
              QRhiShaderResourceBinding::sampledTexture(EnvironmentBinding, fragment,
                                                        environment.get(), linear.get()),
              QRhiShaderResourceBinding::sampledTexture(
                      ShadowBinding, fragment, shadow.hardwareDepth.get(), shadowCompare.get()),
              QRhiShaderResourceBinding::sampledTexture(CaptureBinding, fragment,
                                                        opaque.color.get(), mipLinear.get()),
              QRhiShaderResourceBinding::sampledTexture(DfgBinding, fragment, dfg.get(),
                                                        linear.get()),
              QRhiShaderResourceBinding::sampledTexture(LtcFirstBinding, fragment, ltc1.get(),
                                                        linear.get()),
              QRhiShaderResourceBinding::sampledTexture(LtcSecondBinding, fragment, ltc2.get(),
                                                        linear.get()) });
    require(shellBindings->create(), "Shell bindings failed");
    shellPipeline =
            createScenePipeline(api, input, composite, shellBindings.get(), ScenePass::Shell);
    areaShader = input.areaIntensity > 0;
    opticsProbeShader = input.opticalProbe > 0;
    // The visibility pass writes the exact source front-surface depth first.
    // Equal/read-only depth allows early rejection before expensive optical shading.
    shellPipeline->setDepthOp(QRhiGraphicsPipeline::Equal);
    shellPipeline->setDepthWrite(false);
    require(shellPipeline->create(), "Shell visible-surface pipeline failed");
    visibilityProbeShader = false;
    coveragePipeline =
            createScenePipeline(api, input, composite, geometryBindings.get(), ScenePass::Opaque);
    coveragePipeline->setShaderStages(
            { { QRhiShaderStage::Vertex, shader(u":/rhi/geometry.vert.qsb"_s) },
              { QRhiShaderStage::Fragment, shader(u":/rhi/coverage.frag.qsb"_s) } });
    QRhiGraphicsPipeline::TargetBlend noColor;
    noColor.colorWrite = { };
    if (diagnosticTargets)
        coveragePipeline->setTargetBlends({ noColor, noColor, noColor });
    else
        coveragePipeline->setTargetBlends({ noColor });
    require(coveragePipeline->create(), "Shell visibility pipeline failed");
    coverageInstances = createScenePipeline(api, input, composite, geometryBindings.get(),
                                            ScenePass::Opaque, InstanceMode::Instanced);
    coverageInstances->setShaderStages(
            { { QRhiShaderStage::Vertex, shader(u":/rhi/geometry-instanced.vert.qsb"_s) },
              { QRhiShaderStage::Fragment, shader(u":/rhi/coverage.frag.qsb"_s) } });
    if (diagnosticTargets)
        coverageInstances->setTargetBlends({ noColor, noColor, noColor });
    else
        coverageInstances->setTargetBlends({ noColor });
    require(coverageInstances->create(), "Instanced visibility pipeline failed");
    copyBindings.reset(api->newShaderResourceBindings());
    copyBindings->setBindings(
            { QRhiShaderResourceBinding::sampledTexture(FrameUniformBinding, fragment,
                                                        opaque.color.get(), nearest.get()),
              QRhiShaderResourceBinding::uniformBuffer(CoverBinding, fragment, outputUniform.get()),
              QRhiShaderResourceBinding::sampledTexture(
                      EnvironmentBinding, fragment,
                      diagnosticTargets ? opaque.normal.get() : opaque.color.get(), nearest.get()),
              QRhiShaderResourceBinding::sampledTexture(ShadowBinding, fragment,
                                                        diagnosticTargets ? opaque.depth.get()
                                                                          : opaque.color.get(),
                                                        nearest.get()) });
    require(copyBindings->create(), "Opaque copy bindings failed");
    copyPipeline.reset(api->newGraphicsPipeline());
    copyPipeline->setShaderStages(
            { { QRhiShaderStage::Vertex, shader(u":/rhi/output.vert.qsb"_s) },
              { QRhiShaderStage::Fragment, shader(u":/rhi/copy.frag.qsb"_s) } });
    copyPipeline->setShaderResourceBindings(copyBindings.get());
    copyPipeline->setRenderPassDescriptor(composite.pass.get());
    copyPipeline->setSampleCount(captureSamples);
    if (diagnosticTargets)
        copyPipeline->setTargetBlends({ { }, { }, { } });
    require(copyPipeline->create(), "Opaque copy pipeline failed");
}
void Renderer::prepareOutputTarget()
{
    if (api != rhi()) {
        readbacks.finish(api);
        readbacks.reset();
        reset();
        api = rhi();
        qCInfo(logRender) << "RHI renderer initialized" << "backend=" << api->backendName()
                          << "device=" << api->driverInfo().deviceName;
    }
    itemOutput.create(api, colorTexture(), msaaColorBuffer(), resolveTexture());
    cachedInventory = { };
}
void Renderer::prepareResources(QRhiCommandBuffer *)
{
    if (input.cover.isNull() && !input.atlasGeneration)
        return;
    const auto requestedSize = itemOutput.rt->pixelSize();
    const auto requestedCover = input.atlasGeneration ? input.atlasSize : input.cover.size();
    const AllocationKey key{ requestedSize,       requestedCover,
                             input.shadowSize,    input.captureSamples,
                             input.aoSamples,     itemOutput.rt->sampleCount(),
                             input.shells,        bool(deliver),
                             input.postprocess,   input.bokeh,
                             input.smaa,          input.filterCovers,
                             input.navigationDpr, input.textResourceRevision };
    if (key != allocationKey || plannedOpeningLayer != input.openingLayerSize
        || plannedPanelLayer != input.panelLayerSize || plannedHdr != input.hdrActive
        || plannedDisplayLayer != input.displayLayerSize || !vertices) {
        for (const auto format : { QRhiTexture::RGBA16F, QRhiTexture::RGBA8, QRhiTexture::D32F })
            require(api->isTextureFormatSupported(format, QRhiTexture::RenderTarget),
                    "Selected GPU lacks a required RHI target format");
        if (deliver)
            require(api->isTextureFormatSupported(QRhiTexture::R32F, QRhiTexture::RenderTarget),
                    "Selected GPU lacks the shadow diagnostic color attachment");
        require(api->isTextureFormatSupported(QRhiTexture::RGBA32F),
                "Selected GPU lacks required float lookup textures");
        if (input.postprocess && input.aoSamples)
            require(api->isTextureFormatSupported(QRhiTexture::R16F, QRhiTexture::RenderTarget),
                    "Selected GPU lacks the half-float scalar AO attachment");
        const int maximum = api->resourceLimit(QRhi::TextureSizeMax);
        require(qMax(requestedSize.width(), requestedSize.height()) <= maximum
                        && qMax(requestedCover.width(), requestedCover.height()) <= maximum
                        && qMax(1, input.shadowSize) <= maximum,
                "Native resource dimensions exceed GPU capabilities");
        require(api->supportedSampleCounts().contains(input.captureSamples),
                "Requested resource plan uses unsupported MSAA");
        qint64 geometryBytes = ExtraGeometryVertices * renderData::VertexBytes
                + ExtraGeometryIndices * renderData::FloatBytes;
        for (const auto &batch : preparedAssets().geometry)
            geometryBytes += batch.vertices.size() * renderData::VertexBytes
                    + batch.indices.size() * renderData::FloatBytes;
        const qint64 previousPlanBytes =
                allocationPlan.value(u"estimatedBytesUpperBound"_s).toInteger();
        allocationPlan =
                resourcePlan(input, requestedSize, bool(deliver), api->ubufAligned(sizeof(Uniform)),
                             geometryBytes, itemOutput.rt->sampleCount());
        allocationPlan.insert(
                u"replacementOldPlusNewUpperBoundBytes"_s,
                previousPlanBytes
                        + allocationPlan.value(u"estimatedBytesUpperBound"_s).toInteger());
        allocationKey = key;
        plannedOpeningLayer = input.openingLayerSize;
        plannedPanelLayer = input.panelLayerSize;
        plannedDisplayLayer = input.displayLayerSize;
        plannedHdr = input.hdrActive;
        cachedInventory = { };
    }
    if (!vertices) {
        geometry.emplace();
        vertices.reset(api->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                      geometry->vertexBytes().size()));
        require(vertices->create(), "RHI vertex buffer failed");
        indices.reset(api->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                     geometry->indexBytes().size()));
        require(indices->create(), "RHI index buffer failed");
        uniformStride = api->ubufAligned(sizeof(Uniform));
        uniforms.reset(
                api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                               uniformStride
                                       * (renderData::MaximumCaseEntities
                                                  * yalahoo::rhi::renderData::CaseSurfaceCount
                                          + yalahoo::rhi::renderData::ExtraDrawUniforms)));
        require(uniforms->create(), "RHI uniform buffer failed");
        uniformScratch = QByteArray(uniforms->size(), '\0');
        instanceBuffer.reset(
                api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                               renderData::MaximumCaseEntities * renderData::InstanceBytes));
        require(instanceBuffer->create(), "RHI instance buffer failed");
        instanceScratch = QByteArray(instanceBuffer->size(), Qt::Uninitialized);
        outputUniform.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                           renderData::OutputUniformBytes));
        require(outputUniform->create(), "Output uniform buffer failed");
        linear.reset(api->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        require(linear->create(), "Linear sampler failed");
        nearest.reset(api->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        require(nearest->create(), "Depth sampler failed");
        shadowCompare.reset(api->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                            QRhiSampler::None, QRhiSampler::ClampToEdge,
                                            QRhiSampler::ClampToEdge));
        shadowCompare->setTextureCompareOp(QRhiSampler::LessOrEqual);
        require(shadowCompare->create(), "Source bilinear comparison sampler failed");
        mipLinear.reset(api->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                        QRhiSampler::Linear, QRhiSampler::ClampToEdge,
                                        QRhiSampler::ClampToEdge));
        require(mipLinear->create(), "Capture mip sampler failed");
        environment.reset(
                api->newTexture(QRhiTexture::RGBA16F,
                                { renderData::EnvironmentWidth, renderData::EnvironmentHeight }));
        require(environment->create(), "Room environment texture failed");
        for (auto *t : { &dfg, &ltc1, &ltc2 }) {
            t->reset(api->newTexture(QRhiTexture::RGBA32F,
                                     t == &dfg ? QSize(renderData::DfgSide, renderData::DfgSide)
                                               : QSize(renderData::LtcSide, renderData::LtcSide)));
            require((*t)->create(), "Shell lookup texture failed");
        }
    }
    prepareShadowResources();
    const auto coverSize = input.atlasGeneration ? input.atlasSize : input.cover.size();
    const bool replaceCover = !cover || cover->pixelSize() != coverSize
            || atlasGeneration != input.atlasGeneration
            || cover->flags().testFlag(QRhiTexture::MipMapped) != input.filterCovers;
    const bool replaceTargets = targetSize != itemOutput.rt->pixelSize()
            || captureSamples != input.captureSamples || targetsHaveShells != input.shells
            || diagnosticTargets != bool(deliver) || !opaque.rt;
    if (replaceCover && !replaceTargets) {
        require(std::max(coverSize.width(), coverSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "Cover exceeds GPU dimensions");
        auto previous = std::move(cover);
        prepareCoverTexture(coverSize);
        replaceTextureBinding(CoverBinding, cover.get(),
                              input.filterCovers ? mipLinear.get() : linear.get());
    }
    if (replaceTargets) {
        resetTargetPipelines();
        targetSize = itemOutput.rt->pixelSize();
        captureSamples = input.captureSamples;
        targetsHaveShells = input.shells;
        diagnosticTargets = bool(deliver);
        require(api->supportedSampleCounts().contains(captureSamples),
                "Requested capture MSAA is unsupported");
        ++generation;
        require(std::max(targetSize.width(), targetSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "RHI target exceeds GPU dimensions");
        require(shadowSize <= api->resourceLimit(QRhi::TextureSizeMax),
                "Shadow target exceeds GPU dimensions");
        opaque.create(api, targetSize, diagnosticTargets, captureSamples, input.shells);
        if (input.shells)
            composite.create(api, targetSize, diagnosticTargets, captureSamples, false,
                             QRhiTexture::RGBA16F, &opaque);
        else
            composite.reset();
        require(std::max(coverSize.width(), coverSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "Atlas exceeds GPU dimensions");
        if (replaceCover)
            prepareCoverTexture(coverSize);
        geometryBindings.reset(api->newShaderResourceBindings());
        const auto stages =
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
        geometryBindings->setBindings(
                { QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                          0, stages, uniforms.get(), sizeof(Uniform)),
                  QRhiShaderResourceBinding::sampledTexture(CoverBinding, stages, cover.get(),
                                                            input.filterCovers ? mipLinear.get()
                                                                               : linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          EnvironmentBinding, QRhiShaderResourceBinding::FragmentStage,
                          environment.get(), linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          ShadowBinding, QRhiShaderResourceBinding::FragmentStage,
                          shadow.hardwareDepth.get(), shadowCompare.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          DfgBinding, QRhiShaderResourceBinding::FragmentStage, dfg.get(),
                          linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          LtcFirstBinding, QRhiShaderResourceBinding::FragmentStage, ltc1.get(),
                          linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          LtcSecondBinding, QRhiShaderResourceBinding::FragmentStage, ltc2.get(),
                          linear.get()) });
        require(geometryBindings->create(), "Geometry bindings failed");
        opaquePipeline =
                createScenePipeline(api, input, opaque, geometryBindings.get(), ScenePass::Opaque);
        opaqueInstances = createScenePipeline(api, input, opaque, geometryBindings.get(),
                                              ScenePass::Opaque, InstanceMode::Instanced);
        backgroundPipeline =
                createScenePipeline(api, input, opaque, geometryBindings.get(), ScenePass::Opaque);
        backgroundPipeline->setDepthTest(false);
        backgroundPipeline->setDepthWrite(false);
        backgroundPipeline->setCullMode(QRhiGraphicsPipeline::None);
        require(backgroundPipeline->create(), "Background pipeline failed");
        if (input.shells)
            prepareShellPipelines();
    }
    if (input.shells
        && (areaShader != (input.areaIntensity > 0)
            || opticsProbeShader != (input.opticalProbe > 0))) {
        shellPipeline =
                createScenePipeline(api, input, composite, shellBindings.get(), ScenePass::Shell);
        shellPipeline->setDepthOp(QRhiGraphicsPipeline::Equal);
        shellPipeline->setDepthWrite(false);
        require(shellPipeline->create(), "Shell area specialization failed");
        areaShader = input.areaIntensity > 0;
        opticsProbeShader = input.opticalProbe > 0;
    }
#ifdef YALAHOO_RHI_AUDIT
    if (input.shells && visibilityProbeShader != (input.opticalProbe == VisibilityProbe)) {
        visibilityProbeShader = input.opticalProbe == VisibilityProbe;
        for (int instanced = 0; instanced < 2; ++instanced) {
            auto *p = instanced ? coverageInstances.get() : coveragePipeline.get();
            p->setShaderStages(
                    { { QRhiShaderStage::Vertex,
                        shader(instanced ? u":/rhi/geometry-instanced.vert.qsb"_s
                                         : u":/rhi/geometry.vert.qsb"_s) },
                      { QRhiShaderStage::Fragment,
                        shader(visibilityProbeShader ? u":/rhi/VisibilityProbe.frag.qsb"_s
                                                     : u":/rhi/coverage.frag.qsb"_s) } });
            QRhiGraphicsPipeline::TargetBlend color, disabled;
            disabled.colorWrite = { };
            if (!visibilityProbeShader)
                color.colorWrite = { };
            if (diagnosticTargets)
                p->setTargetBlends({ color, disabled, disabled });
            else
                p->setTargetBlends({ color });
            require(p->create(), "Visibility diagnostic pipeline failed");
        }
    }
#endif
    const QSize aoSize = targetSize;
    const bool wantAo = input.postprocess && input.aoSamples > 0;
    const bool wantDof = input.postprocess && input.bokeh && (!wantAo || aoSize != targetSize);
    if (postTargets != input.postprocess || aoTargets != wantAo || dofTargets != wantDof
        || (wantAo && aoGeometrySize != aoSize) || (wantDof && dofGeometry.size != targetSize)) {
        post.reset();
        prepassPipeline.reset();
        prepassInstances.reset();
        outputSource = nullptr;
        prepassBackgroundPipeline.reset();
        aoGeometry.reset();
        dofGeometry.reset();
        postTargets = input.postprocess;
        aoTargets = wantAo;
        dofTargets = wantDof;
        aoGeometrySize = aoSize;
        if (wantAo)
            aoGeometry.create(api, aoSize, true, 1, false, QRhiTexture::RGBA8);
        if (wantDof)
            dofGeometry.create(api, targetSize, true, 1, false, QRhiTexture::RGBA8);
        if (wantAo || wantDof) {
            auto &target = wantAo ? aoGeometry : dofGeometry;
            prepassPipeline = createScenePipeline(api, input, target, geometryBindings.get(),
                                                  ScenePass::Opaque);
            prepassInstances = createScenePipeline(api, input, target, geometryBindings.get(),
                                                   ScenePass::Opaque, InstanceMode::Instanced);
            prepassBackgroundPipeline = createScenePipeline(
                    api, input, target, geometryBindings.get(), ScenePass::Opaque);
            prepassBackgroundPipeline->setDepthTest(false);
            prepassBackgroundPipeline->setDepthWrite(false);
            prepassBackgroundPipeline->setCullMode(QRhiGraphicsPipeline::None);
            require(prepassBackgroundPipeline->create(), "Prepass background pipeline failed");
        }
    }
    auto *resolvedColor = input.shells ? composite.color.get() : opaque.color.get();
    if (input.postprocess) {
        PostSettings settings;
        settings.size = targetSize;
        settings.samples = input.aoSamples;
        settings.radius = input.aoRadius;
        settings.nearPlane = float(input.scene.nearPlane);
        settings.farPlane = float(input.scene.farPlane);
        settings.focus = input.focus;
        settings.aperture = input.aperture;
        settings.projectionX = 1 / input.scene.projection(0, 0);
        settings.projectionY = 1 / input.scene.projection(1, 1);
        settings.bokeh = input.bokeh;
        settings.smaa = input.smaa;
        post.prepare(api, settings, resolvedColor,
                     wantAo ? aoGeometry.normal.get() : opaque.color.get(),
                     wantAo ? aoGeometry.depth.get() : opaque.color.get(),
                     wantDof ? dofGeometry.depth.get()
                             : (wantAo ? aoGeometry.depth.get() : opaque.color.get()));
        resolvedColor = post.output();
    }
    // Output format/sample changes replace the color-only render-pass descriptor.
    if (outputPipeline && outputSource == resolvedColor && outputPostRevision == post.revision()
        && outputPass == itemOutput.rt->renderPassDescriptor()
        && outputSamples == itemOutput.rt->sampleCount())
        return;
    outputPipeline.reset();
    outputBindings.reset();
    outputBindings.reset(api->newShaderResourceBindings());
    outputBindings->setBindings(
            { QRhiShaderResourceBinding::sampledTexture(FrameUniformBinding,
                                                        QRhiShaderResourceBinding::FragmentStage,
                                                        resolvedColor, linear.get()),
              QRhiShaderResourceBinding::uniformBuffer(CoverBinding,
                                                       QRhiShaderResourceBinding::FragmentStage,
                                                       outputUniform.get()) });
    require(outputBindings->create(), "Output bindings failed");
    outputPipeline.reset(api->newGraphicsPipeline());
    outputPipeline->setShaderStages(
            { { QRhiShaderStage::Vertex, shader(u":/rhi/output.vert.qsb"_s) },
              { QRhiShaderStage::Fragment, shader(u":/rhi/output.frag.qsb"_s) } });
    outputPipeline->setShaderResourceBindings(outputBindings.get());
    outputPipeline->setRenderPassDescriptor(itemOutput.rt->renderPassDescriptor());
    outputPipeline->setSampleCount(itemOutput.rt->sampleCount());
    require(outputPipeline->create(), "Output pipeline failed");
    outputSource = resolvedColor;
    outputPass = itemOutput.rt->renderPassDescriptor();
    outputSamples = itemOutput.rt->sampleCount();
    outputPostRevision = post.revision();
}
void Renderer::drawMesh(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *p,
                        QRhiShaderResourceBindings *s, Mesh mesh, quint32 offset)
{
    cb->setGraphicsPipeline(p);
    const QRhiCommandBuffer::DynamicOffset dynamic{ 0, offset };
    cb->setShaderResources(s, 1, &dynamic);
    const QRhiCommandBuffer::VertexInput v(vertices.get(), 0);
    cb->setVertexInput(0, 1, &v, indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(mesh.count, 1, mesh.start);
}
void Renderer::drawInstances(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *p,
                             QRhiShaderResourceBindings *s, Mesh mesh, quint32 offset, int count)
{
    if (!count)
        return;
    cb->setGraphicsPipeline(p);
    const QRhiCommandBuffer::DynamicOffset dynamic{ 0, offset };
    cb->setShaderResources(s, 1, &dynamic);
    const std::array<QRhiCommandBuffer::VertexInput, 2> inputs{
        QRhiCommandBuffer::VertexInput(vertices.get(), 0),
        QRhiCommandBuffer::VertexInput(instanceBuffer.get(), 0)
    };
    cb->setVertexInput(0, 2, inputs.data(), indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(mesh.count, count, mesh.start);
}
void Renderer::drawGeometryPass(QRhiCommandBuffer *&cb, const quint32 &bgOffset,
                                const quint32 &floorOffset, const bool &batched,
                                const quint32 &batchGeometryOffset, const int &visibleCount,
                                QVector<int> &opaqueOrder, const QVector<bool> &visible,
                                QVector<quint32> &prepassOffsets, Target &target)
{
    const auto size = target.size;
    cb->beginPass(target.rt.get(), Qt::white, { 1, 0 });
    cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });
    drawMesh(cb, prepassBackgroundPipeline.get(), geometryBindings.get(),
             geometry->backgroundTriangle(), bgOffset);
    if (input.productFloor)
        drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), geometry->productFloor(),
                 floorOffset);
    if (batched) {
        Mesh combined{ geometry->lid().start,
                       geometry->lid().count + geometry->spine().count + geometry->tray().count
                               + (input.showPaper ? geometry->paper().count : 0) };
        drawInstances(cb, prepassInstances.get(), geometryBindings.get(), combined,
                      batchGeometryOffset, visibleCount);
    } else
        for (int i : opaqueOrder) {
            if (!visible[i])
                continue;
            for (auto mesh : { geometry->lid(), geometry->spine(), geometry->tray() })
                drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), mesh,
                         prepassOffsets[i]);
            if (input.showPaper)
                drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), geometry->paper(),
                         prepassOffsets[i]);
        }
    cb->endPass();
}
void Renderer::drawOpaquePass(QRhiCommandBuffer *&cb, const bool &recordCapture,
                              QJsonArray &opticalEvents, auto &opaqueMeshDraw,
                              const quint32 &bgOffset, const quint32 &floorOffset,
                              const bool &batched, auto &opaqueInstanceDraw,
                              const quint32 &batchGeometryOffset, const quint32 &batchPaperOffset,
                              QVector<int> &opaqueOrder, const QVector<bool> &visible,
                              QVector<quint32> &opaqueOffsets, Target &target)
{
    cb->beginPass(target.rt.get(), Qt::black, { 1, 0 });
    cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
    if (recordCapture)
        opticalEvents.append(u"opaque"_s);
    opaqueMeshDraw(backgroundPipeline.get(), geometry->backgroundTriangle(), bgOffset);
    if (input.productFloor)
        opaqueMeshDraw(opaquePipeline.get(), geometry->productFloor(), floorOffset);
    else if (input.shadowDiagnostic)
        opaqueMeshDraw(opaquePipeline.get(), geometry->floor(), floorOffset);
    if ((!input.shadowDiagnostic || input.shells) && batched) {
        if (input.geometryDiagnostic)
            for (auto mesh : { geometry->lid(), geometry->spine(), geometry->tray() })
                opaqueInstanceDraw(mesh, batchGeometryOffset);
        if (input.showPaper)
            opaqueInstanceDraw(geometry->paper(),
                               input.geometryDiagnostic ? batchGeometryOffset : batchPaperOffset);
    } else if (!input.shadowDiagnostic || input.shells)
        for (int i : opaqueOrder) {
            if (!visible[i])
                continue;
            if (input.geometryDiagnostic)
                for (auto mesh : { geometry->lid(), geometry->spine(), geometry->tray() })
                    opaqueMeshDraw(opaquePipeline.get(), mesh, opaqueOffsets[i]);
            if (input.showPaper)
                opaqueMeshDraw(opaquePipeline.get(), geometry->paper(), opaqueOffsets[i]);
        }
}
void Renderer::writeInstanceData(QByteArray &instances, int &instanceCursor,
                                 const QVector<QMatrix4x4> &entities, int i)
{
    auto *destination = instances.data() + instanceCursor++ * renderData::InstanceBytes;
    std::memcpy(destination, entities[i].constData(), renderData::MatrixBytes);
    float uv[VectorScalars] = { 0, 0, 1, 1 };
    if (input.atlasGeneration)
        copy(uv, entityCoverUv[i]);
    std::memcpy(destination + InstanceUvOffsetBytes, uv, renderData::VectorBytes);
    float light[VectorScalars];
    copy(light, frameUniforms.originLight(i));
    std::memcpy(destination + InstanceLightOffsetBytes, light, renderData::VectorBytes);
    copy(light, frameUniforms.secondaryLight(i));
    std::memcpy(destination + InstanceSecondaryLightOffsetBytes, light, renderData::VectorBytes);
}
void Renderer::drawShellVisibility(QRhiCommandBuffer *cb, const QVector<ShellDraw> &shells,
                                   const QVector<bool> &visible, bool batched,
                                   quint32 batchGeometryOffset, int visibleCount)
{
    if (batched) {
        Mesh combined{ geometry->lid().start,
                       geometry->lid().count + geometry->spine().count + geometry->tray().count };
        drawInstances(cb, coverageInstances.get(), geometryBindings.get(), combined,
                      batchGeometryOffset, visibleCount);
    } else
        for (auto it = shells.crbegin(); it != shells.crend(); ++it)
            if (visible[it->entity])
                drawMesh(cb, coveragePipeline.get(), geometryBindings.get(), it->mesh, it->offset);
}
void Renderer::drawShellPass(QRhiCommandBuffer *cb, const QVector<ShellDraw> &shells,
                             const QVector<bool> &visible, bool batched,
                             quint32 batchGeometryOffset, int visibleCount, bool recordCapture,
                             QJsonArray &opticalEvents)
{
    // Resolve ends before generateMips; capture never contains transmission shells.
    ResourceUpdates mips(api->nextResourceUpdateBatch());
    mips->generateMips(opaque.color.get());
    cb->resourceUpdate(mips.release());
    if (recordCapture)
        opticalEvents.append(u"capture-mips"_s);
    // Load retained opaque depth and copy resolved color into the independent destination.
    // Both MRT layouts/sample counts are compatible; no sampled attachment aliases this target.
    cb->beginPass(composite.rt.get(), Qt::black, { 1, 0 });
    cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
    cb->setGraphicsPipeline(copyPipeline.get());
    cb->setShaderResources(copyBindings.get());
    cb->draw(yalahoo::rhi::renderData::FullscreenTriangleVertices);
    if (recordCapture)
        opticalEvents.append(u"composite-copy"_s);
    if (!input.geometryDiagnostic) {
        if (recordCapture)
            opticalEvents.append(u"visibility"_s);
        drawShellVisibility(cb, shells, visible, batched, batchGeometryOffset, visibleCount);
    }
    if (recordCapture && !input.geometryDiagnostic)
        opticalEvents.append(u"shell-shading"_s);
    if (!input.geometryDiagnostic && visibleCount) {
        // Preserve the global depth/identity/surface sequence. Pipeline and geometry
        // are constant across this sequence; only the uniform offset changes.
        cb->setGraphicsPipeline(shellPipeline.get());
        const QRhiCommandBuffer::VertexInput vertex(vertices.get(), 0);
        cb->setVertexInput(0, 1, &vertex, indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        for (const auto &draw : shells) {
            if (!visible[draw.entity])
                continue;
            const QRhiCommandBuffer::DynamicOffset dynamic{ 0, draw.offset };
            cb->setShaderResources(shellBindings.get(), 1, &dynamic);
            cb->drawIndexed(draw.mesh.count, 1, draw.mesh.start);
        }
    }
    cb->endPass();
}
void Renderer::drawShadowPass(QRhiCommandBuffer *cb, bool batched, quint32 batchGeometryOffset,
                              const QVector<QMatrix4x4> &entities,
                              const QVector<quint32> &shadowOffsets)
{
    if (!input.shadows)
        return;

    if (batched)
        drawInstances(cb, shadowInstances.get(), shadowBindings.get(), geometry->tray(),
                      batchGeometryOffset, entities.size());
    else
        for (int i = 0; i < entities.size(); ++i)
            drawMesh(cb, shadowPipeline.get(), shadowBindings.get(), geometry->tray(),
                     shadowOffsets[i]);
}
void Renderer::renderFrame(QRhiCommandBuffer *cb)
{
    QElapsedTimer cpu;
    if (input.profile || diagnose)
        cpu.start();
    // initialize may precede synchronize in Qt; defer resource-size changes to a fresh frame.
    if (input.cover.isNull() && !input.atlasGeneration)
        return;
    prepareResources(cb);
    ResourceUpdates updates(api->nextResourceUpdateBatch());
    if (uploadGeometry) {
        updates->uploadStaticBuffer(vertices.get(), geometry->vertexBytes());
        updates->uploadStaticBuffer(indices.get(), geometry->indexBytes());
        const auto roomHalf =
                floatTextureBytes(u":/rhi/environment/room-pmrem.rgba16f"_s,
                                  renderData::EnvironmentWidth * renderData::EnvironmentHeight
                                          * pixelStorage::HalfRgbaOrDepthStencilBytes);
        updates->uploadTexture(environment.get(),
                               QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(roomHalf)) }));
        const std::array<QRhiTexture *, yalahoo::rhi::renderData::CaseSurfaceCount> lookup{
            dfg.get(), ltc1.get(), ltc2.get()
        };
        const std::array<QString, yalahoo::rhi::renderData::CaseSurfaceCount> names{
            u":/rhi/environment/dfg.rgba32f"_s, u":/rhi/environment/ltc1.rgba32f"_s,
            u":/rhi/environment/ltc2.rgba32f"_s
        };
        for (int i = 0; i < 3; ++i) {
            const auto bytes = floatTextureBytes(names[i],
                                                 lookup[i]->pixelSize().width()
                                                         * lookup[i]->pixelSize().height()
                                                         * renderData::VectorBytes);
            updates->uploadTexture(
                    lookup[i],
                    QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                            0, 0, QRhiTextureSubresourceUploadDescription(bytes)) }));
        }
        uploadGeometry = false;
    }
    frameUploadBytes = 0;
    selectedUploadBytes = 0;
    uploadRegions = { };
    const bool recordCapture =
            deliver && input.request != capturedRequest && readbacks.hasCapacity();
    if (atlasGeneration) {
        for (const auto &u : atlasQueue.take(input.initializingAtlas)) {
            const QPoint destination(
                    (u.slot % (input.atlasSize.width() / renderData::CoverTilePixels))
                            * renderData::CoverTilePixels,
                    (u.slot / (input.atlasSize.width() / renderData::CoverTilePixels))
                            * renderData::CoverTilePixels);
            QRhiTextureSubresourceUploadDescription region(u.pixels);
            region.setSourceSize({ renderData::CoverTilePixels, renderData::CoverTilePixels });
            region.setDestinationTopLeft(destination);
            updates->uploadTexture(cover.get(), { { 0, 0, region } });
            if (input.filterCovers) {
                for (int level = 1; level <= u.mipmaps.size(); ++level) {
                    QRhiTextureSubresourceUploadDescription mip(u.mipmaps[level - 1]);
                    mip.setDestinationTopLeft(
                            { destination.x() >> level, destination.y() >> level });
                    updates->uploadTexture(cover.get(), { { 0, level, mip } });
                }
            }
            frameUploadBytes += AtlasUploadQueue::payloadBytes(u);
            if (recordCapture)
                uploadRegions.append(QJsonObject{ { u"slot"_s, u.slot },
                                                  { u"x"_s, destination.x() },
                                                  { u"y"_s, destination.y() },
                                                  { u"width"_s, renderData::CoverTilePixels },
                                                  { u"height"_s, renderData::CoverTilePixels },
                                                  { u"bytes"_s, AtlasUploadQueue::tileBytes },
                                                  { u"lease"_s, qint64(u.lease) },
                                                  { u"serial"_s, qint64(u.serial) } });
        }
    } else if (coverDirty) {
        // Filter premultiplied texels, then unpremultiply in the geometry->paper() shader. Straight
        // transparent black texels otherwise produce a dark line at the alpha-test edge.
        const auto image = input.cover.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        updates->uploadTexture(cover.get(),
                               { { 0, 0, QRhiTextureSubresourceUploadDescription(image) } });
        selectedUploadBytes = image.sizeInBytes();
        if (input.filterCovers) {
            require(input.coverMipmaps.size() == api->mipLevelsForSize(image.size()) - 1,
                    "Cover filtering requires a complete independently generated mip chain");
            for (int level = 1; level <= input.coverMipmaps.size(); ++level) {
                const auto &mip = std::as_const(input.coverMipmaps)[level - 1];
                updates->uploadTexture(
                        cover.get(),
                        { { 0, level, QRhiTextureSubresourceUploadDescription(mip) } });
                selectedUploadBytes += mip.sizeInBytes();
            }
        }
        coverDirty = false;
        selectedTotalUploadBytes += selectedUploadBytes;
    }
    totalUploadBytes += frameUploadBytes;
    // Keep renderer-owned scratch storage. Sharing the input and then replacing
    // unready regions would detach/copy both arrays on every pending-upload frame.
    if (input.atlasGeneration) {
        effectiveCoverTiles.resize(input.coverTiles.size());
        effectiveCoverLeases.resize(input.coverLeases.size());
        entityCoverUv.resize(input.coverTiles.size());
        for (int i = 0; i < input.coverTiles.size(); ++i) {
            auto tile = std::as_const(input.coverTiles)[i];
            auto lease = input.coverLeases.isEmpty() ? 0 : std::as_const(input.coverLeases)[i];
            bool ready = input.coverLeases.isEmpty() || atlasQueue.ready(tile, lease);
            if (!ready && i < input.fallbackCoverTiles.size()) {
                tile = std::as_const(input.fallbackCoverTiles)[i];
                lease = std::as_const(input.fallbackCoverLeases)[i];
                ready = atlasQueue.ready(tile, lease);
            }
            effectiveCoverTiles[i] = tile;
            if (!effectiveCoverLeases.isEmpty())
                effectiveCoverLeases[i] = lease;
            entityCoverUv[i] = ready ? QVector4D(float(tile.x()) / input.atlasSize.width(),
                                                 float(tile.y()) / input.atlasSize.height(),
                                                 float(tile.width()) / input.atlasSize.width(),
                                                 float(tile.height()) / input.atlasSize.height())
                                     : QVector4D();
        }
    } else {
        effectiveCoverTiles = input.coverTiles;
        effectiveCoverLeases = input.coverLeases;
    }
    const bool standalone = input.entities.isEmpty() && !input.poolFrame;
    if (standalone) {
        standaloneEntities.resize(1);
        standaloneEntities[0] = input.scene.caseTransform;
    }
    const auto &entities = standalone ? std::as_const(standaloneEntities) : input.entities;
    require(entities.size() <= renderData::MaximumCaseEntities,
            "RHI visible pool capacity exceeded");
    visibleEntities(input, entities, visible);
    // Disabled shadows need one clear after toggling/reallocation, not on every caster movement.
    shadowDirty = shadowDirty || cachedShadows != input.shadows
            || (input.shadows && shadowEntities != entities);
    const bool batched = entities.size() >= MinimumBatchedEntities;
    frameUniforms.prepare(input, api->clipSpaceCorrMatrix(), api->isYUpInFramebuffer(),
                          api->isClipDepthZeroToOne(), targetSize, shadowSize, entities, visible,
                          batched);
    auto &bytes = uniformScratch;
    quint32 cursor = 0;
    auto append = [&](const QMatrix4x4 &m, int kind, int entity = -1) {
        auto u = frameUniforms.uniform(input, m, kind, entity, entityCoverUv);
        const auto offset = cursor;
        require(cursor + sizeof(u) <= quint32(bytes.size()), "Draw uniforms exceed bounded buffer");
        std::memcpy(bytes.data() + cursor, &u, sizeof(u));
        cursor += uniformStride;
        return offset;
    };
    shadowOffsets.clear();
    opaqueOffsets.clear();
    prepassOffsets.clear();
    if (!batched) {
        shadowOffsets.reserve(entities.size());
        opaqueOffsets.reserve(entities.size());
        prepassOffsets.reserve(entities.size());
    }
    opaqueOrder.resize(entities.size());
    for (int i = 0; i < entities.size(); ++i)
        opaqueOrder[i] = i;
    if (input.shells) {
        depths.resize(entities.size());
        viewModels.resize(entities.size());
        for (int i = 0; i < entities.size(); ++i) {
            viewModels[i] = input.scene.view * entities[i];
            depths[i] = -(viewModels[i] * QVector4D(geometry->paper().center, 1)).z();
        }
        std::sort(opaqueOrder.begin(), opaqueOrder.end(), [&](int a, int b) {
            const float da = depths[a];
            const float db = depths[b];
            if (da != db)
                return da < db;
            const auto ia =
                    input.entityIds.isEmpty() ? quint64(a) : std::as_const(input.entityIds)[a];
            const auto ib =
                    input.entityIds.isEmpty() ? quint64(b) : std::as_const(input.entityIds)[b];
            return ia < ib;
        });
    }
    if (!batched && shadowDirty && input.shadows)
        for (const auto &m : entities)
            shadowOffsets.append(append(m, 1));
    const auto bgOffset = append({ }, -1);
    if (!batched)
        for (int i = 0; i < entities.size(); ++i)
            opaqueOffsets.append(append(entities[i], input.geometryDiagnostic ? 1 : 0, i));
    const auto floorOffset = append({ }, input.productFloor ? ProductFloorKind : ReviewFloorKind);

    shells.clear();
    if (input.shells) {
        shells.reserve(entities.size() * yalahoo::rhi::renderData::CaseSurfaceCount);
        require(entities.size() == 1 || input.entityIds.size() == entities.size(),
                "Shell instances require stable IDs");
        const std::array<Mesh, yalahoo::rhi::renderData::CaseSurfaceCount> meshes{
            geometry->lid(), geometry->spine(), geometry->tray()
        };
        for (int i : std::as_const(opaqueOrder)) {
            const auto id = input.entityIds.isEmpty() ? 0 : std::as_const(input.entityIds)[i];
            for (int surface = 0; surface < yalahoo::rhi::renderData::CaseSurfaceCount; ++surface) {
                const auto mesh = meshes[surface];
                const auto center = viewModels[i] * QVector4D(mesh.center, 1);
                shells.append({ mesh, visible[i] ? append(entities[i], LidKind + surface, i) : 0,
                                -center.z(), id, surface, i });
            }
        }
        std::sort(shells.begin(), shells.end(), [](const auto &a, const auto &b) {
            if (a.depth != b.depth)
                return a.depth > b.depth;
            if (a.identity != b.identity)
                return a.identity < b.identity;
            return a.surface < b.surface;
        });
    }
    if (input.postprocess && !batched)
        for (const auto &m : entities)
            prepassOffsets.append(append(m, 1));
    const auto batchPaperOffset = append({ }, 0);
    const auto batchGeometryOffset = append({ }, 1);
    const int visibleCount = int(std::count(visible.begin(), visible.end(), true));
    if (batched) {
        // Every field in the active prefix is overwritten before QRhi copies the upload.
        auto &instances = instanceScratch;
        int instanceCursor = 0;
        auto instance = [&](int i) {
            return writeInstanceData(instances, instanceCursor, entities, i);
        };
        for (int i : opaqueOrder)
            if (visible[i])
                instance(i);
        for (int i : opaqueOrder)
            if (!visible[i])
                instance(i);
        updates->updateDynamicBuffer(instanceBuffer.get(), 0,
                                     instanceCursor * renderData::InstanceBytes,
                                     instances.constData());
    }
    updates->updateDynamicBuffer(uniforms.get(), 0, cursor, bytes.constData());
    const float out[8] = {
        api->isYUpInFramebuffer() ? 0.0f : 1.0f,
        input.geometryDiagnostic || input.shadowDiagnostic || input.rawOutput ? 1.0f : 0.0f,
        (input.postprocess || input.shells) && input.lightingEnabled && !input.rawOutput
                        && !input.geometryDiagnostic && !input.shadowDiagnostic
                ? 1.0f
                : 0.0f,
        input.exposure,
        input.hdrHeadroom,
        0,
        0,
        0
    };
    updates->updateDynamicBuffer(outputUniform.get(), 0, sizeof(out), out);
    const bool shadowRendered = shadowDirty;
    if (shadowDirty) {
        cb->beginPass(shadow.rt.get(), Qt::white, { 1, 0 }, updates.release());
        cb->setViewport({ 0, 0, float(shadowSize), float(shadowSize) });
        drawShadowPass(cb, batched, batchGeometryOffset, entities, shadowOffsets);

        cb->endPass();
        shadowEntities = entities;
        cachedShadows = input.shadows;
        shadowDirty = false;
    } else {
        cb->resourceUpdate(updates.release());
    }
    QJsonArray captureKinds, opticalEvents, fogUniforms;
    const auto recordFog = [&](const Uniform &submitted) {
        fogUniforms.append(QJsonObject{
                { u"kind"_s, submitted.draw[0] },
                { u"shadow"_s,
                  QJsonArray{ submitted.draw[1], submitted.draw[2], submitted.draw[3],
                              submitted.optics[1], submitted.optics[2], submitted.optics[3] } },
                { u"range"_s, QJsonArray{ submitted.fog[0], submitted.fog[1], submitted.fog[2] } },
                { u"color"_s,
                  QJsonArray{ submitted.background[0], submitted.background[1],
                              submitted.background[2] } } });
    };
    const auto recordOpaque = [&](quint32 offset) {
        if (recordCapture) {
            Uniform submitted;
            std::memcpy(&submitted, bytes.constData() + offset, sizeof(submitted));
            captureKinds.append(submitted.draw[0]);
            recordFog(submitted);
        }
    };
    const auto opaqueMeshDraw = [&](QRhiGraphicsPipeline *pipeline, Mesh mesh, quint32 offset) {
        drawMesh(cb, pipeline, geometryBindings.get(), mesh, offset);
        recordOpaque(offset);
    };
    const auto opaqueInstanceDraw = [&](Mesh mesh, quint32 offset) {
        drawInstances(cb, opaqueInstances.get(), geometryBindings.get(), mesh, offset,
                      visibleCount);
        recordOpaque(offset);
    };
    auto drawOpaque = [&](Target &target) {
        return drawOpaquePass(cb, recordCapture, opticalEvents, opaqueMeshDraw, bgOffset,
                              floorOffset, batched, opaqueInstanceDraw, batchGeometryOffset,
                              batchPaperOffset, opaqueOrder, visible, opaqueOffsets, target);
    };
    drawOpaque(opaque);
    cb->endPass();
    if (recordCapture)
        opticalEvents.append(u"opaque-resolved"_s);
    if (input.shells)
        drawShellPass(cb, shells, visible, batched, batchGeometryOffset, visibleCount,
                      recordCapture, opticalEvents);

    if (input.postprocess) {
        auto geometry = [&](Target &target) {
            return drawGeometryPass(cb, bgOffset, floorOffset, batched, batchGeometryOffset,
                                    visibleCount, opaqueOrder, visible, prepassOffsets, target);
        };
        if (input.aoSamples)
            geometry(aoGeometry);
        if (dofTargets)
            geometry(dofGeometry);
        post.render(cb);
    }
    cb->beginPass(itemOutput.rt.get(), Qt::black, { 1, 0 });
    cb->setGraphicsPipeline(outputPipeline.get());
    cb->setViewport({ 0, 0, float(itemOutput.rt->pixelSize().width()),
                      float(itemOutput.rt->pixelSize().height()) });
    cb->setShaderResources();
    cb->draw(yalahoo::rhi::renderData::FullscreenTriangleVertices);
    cb->endPass();
    if (deliver && input.request != capturedRequest && readbacks.hasCapacity()) {
        capturedRequest = input.request;
        Capture capture;
        capture.request = input.request;
        capture.metadata = { { u"backend"_s, int(api->backend()) },
                             { u"device"_s, QString::fromUtf8(api->driverInfo().deviceName) },
                             { u"qt"_s, QString::fromUtf8(qVersion()) },
                             { u"generation"_s, qint64(generation) },
                             { u"resourceGeneration"_s, qint64(generation) },
                             { u"postprocessRevision"_s, qint64(post.revision()) },
                             { u"width"_s, targetSize.width() },
                             { u"height"_s, targetSize.height() },
                             { u"shadowSize"_s, shadowSize },
                             { u"frameRevision"_s, qint64(input.scene.revision) },
                             { u"guiThreadAccessInRender"_s, false },
                             { u"renderThreadDistinct"_s, distinctThread },
                             { u"passes"_s,
                               input.shells
                                       ? QJsonArray{ u"shadow"_s, u"opaque-color-normal-depth"_s,
                                                     u"capture-resolve-mips"_s, u"sorted-shell"_s,
                                                     u"review-output"_s }
                                       : QJsonArray{ u"shadow"_s, u"opaque-color-normal-depth"_s,
                                                     u"review-output"_s } },
                             { u"msaaSamples"_s, captureSamples },
                             { u"recursiveGlassCapture"_s, false },
                             { u"shellDepthWrite"_s, true },
                             { u"shellColorDepthWrite"_s, false },
                             { u"shellDepthVisibility"_s,
                               u"exact front-surface depth, Equal test before optical shading"_s },
                             { u"shellBlend"_s, false },
                             { u"shellCull"_s, u"Back"_s } };
#ifdef YALAHOO_RHI_AUDIT
        QJsonArray resolveGraph;
        struct NamedTarget final
        {
            const char *name;
            Target *target;
        };
        for (const auto &[name, target] :
             { NamedTarget{ "opaque", &opaque }, NamedTarget{ "composite", &composite } }) {
            if (!target->rt)
                continue;
            QJsonArray attachments;
            const auto &description = target->rt->description();
            for (auto it = description.cbeginColorAttachments();
                 it != description.cendColorAttachments(); ++it) {
                const auto *texture = it->texture();
                const auto *buffer = it->renderBuffer();
                const auto *resolved = it->resolveTexture();
                const auto size = texture ? texture->pixelSize() : buffer->pixelSize();
                attachments.append(QJsonObject{
                        { u"storage"_s, texture ? u"texture"_s : u"renderbuffer"_s },
                        { u"format"_s, int(texture ? texture->format() : buffer->backingFormat()) },
                        { u"samples"_s, texture ? texture->sampleCount() : buffer->sampleCount() },
                        { u"width"_s, size.width() },
                        { u"height"_s, size.height() },
                        { u"resourceId"_s,
                          qint64(texture ? texture->globalResourceId()
                                         : buffer->globalResourceId()) },
                        { u"resolveResourceId"_s,
                          resolved ? QJsonValue(qint64(resolved->globalResourceId()))
                                   : QJsonValue() },
                        { u"resolveFormat"_s, resolved ? int(resolved->format()) : 0 },
                        { u"resolveSamples"_s, resolved ? resolved->sampleCount() : 0 },
                        { u"resolveWidth"_s, resolved ? resolved->pixelSize().width() : 0 },
                        { u"resolveHeight"_s, resolved ? resolved->pixelSize().height() : 0 },
                        { u"separateResolve"_s, resolved && resolved != texture } });
            }
            resolveGraph.append(QJsonObject{ { u"name"_s, QString::fromUtf8(name) },
                                             { u"samples"_s, target->rt->sampleCount() },
                                             { u"pipelineSamples"_s,
                                               QString::fromUtf8(name) == u"opaque"_s
                                                       ? opaquePipeline->sampleCount()
                                                       : shellPipeline->sampleCount() },
                                             { u"depthSamples"_s, target->z->sampleCount() },
                                             { u"attachments"_s, attachments } });
        }
        capture.metadata[u"sampling"_s] =
                QJsonObject{ { u"coverTileLocalMipmaps"_s,
                               input.filterCovers && cover->flags().testFlag(QRhiTexture::MipMapped)
                                       && mipLinear->mipmapMode() == QRhiSampler::Linear },
                             { u"captureTrilinearClamp"_s,
                               mipLinear->magFilter() == QRhiSampler::Linear
                                       && mipLinear->minFilter() == QRhiSampler::Linear
                                       && mipLinear->mipmapMode() == QRhiSampler::Linear
                                       && mipLinear->addressU() == QRhiSampler::ClampToEdge
                                       && mipLinear->addressV() == QRhiSampler::ClampToEdge } };
        capture.metadata[u"resolveGraph"_s] = resolveGraph;
        capture.metadata[u"postprocessGraph"_s] = input.postprocess
                ? post.graph(input.shells ? composite.color.get() : opaque.color.get())
                : QJsonObject();
        const auto expectedOutput = QRhiShaderResourceBinding::sampledTexture(
                FrameUniformBinding, QRhiShaderResourceBinding::FragmentStage, outputSource,
                linear.get());
        capture.metadata[u"outputBindingMatches"_s] = outputSource
                        == (input.postprocess
                                    ? post.output()
                                    : (input.shells ? composite.color.get() : opaque.color.get()))
                && std::find(outputBindings->cbeginBindings(), outputBindings->cendBindings(),
                             expectedOutput)
                        != outputBindings->cendBindings();
#endif
        if (input.postprocess) {
            capture.metadata[u"submittedBokeh"_s] = post.submittedBokeh();
            auto passNames = capture.metadata[u"passes"_s].toArray();
            passNames.removeLast();
            for (const auto &name : post.passes())
                passNames.append(name);
            passNames.append(u"ACES-sRGB-output"_s);
            capture.metadata[u"passes"_s] = passNames;
        }
        if (input.shells) {
            const auto expectedBinding = QRhiShaderResourceBinding::sampledTexture(
                    CaptureBinding, QRhiShaderResourceBinding::FragmentStage, opaque.color.get(),
                    mipLinear.get());
            bool noBlend = true;
            for (auto blend = shellPipeline->cbeginTargetBlends();
                 blend != shellPipeline->cendTargetBlends(); ++blend)
                noBlend = noBlend && !blend->enable;
            capture.metadata[u"opticalGraph"_s] = QJsonObject{
                { u"captureBindingMatches"_s,
                  std::find(shellBindings->cbeginBindings(), shellBindings->cendBindings(),
                            expectedBinding)
                          != shellBindings->cendBindings() },
                { u"captureSeparateFromComposite"_s, opaque.color.get() != composite.color.get() },
                { u"sharedOpaqueDepth"_s,
                  opaque.rt->description().depthStencilBuffer()
                          == composite.rt->description().depthStencilBuffer() },
                { u"depthContentsPreserved"_s,
                  composite.rt->flags().testFlag(
                          QRhiTextureRenderTarget::PreserveDepthStencilContents) },
                { u"captureMips"_s, opaque.color->flags().testFlag(QRhiTexture::MipMapped) },
                { u"colorDepthTest"_s, shellPipeline->hasDepthTest() },
                { u"colorDepthWrite"_s, shellPipeline->hasDepthWrite() },
                { u"colorDepthEqual"_s, shellPipeline->depthOp() == QRhiGraphicsPipeline::Equal },
                { u"colorCullBack"_s, shellPipeline->cullMode() == QRhiGraphicsPipeline::Back },
                { u"colorBlendingDisabled"_s, noBlend },
                { u"visibilityDepthWrite"_s, coveragePipeline->hasDepthWrite() },
                { u"visibilityDepthLessEqual"_s,
                  coveragePipeline->depthOp() == QRhiGraphicsPipeline::LessOrEqual }
            };
        }
        capture.metadata[u"atlasGeneration"_s] = qint64(atlasGeneration);
        capture.metadata[u"atlasUploadBytes"_s] = frameUploadBytes;
        capture.metadata[u"selectedUploadBytes"_s] = selectedUploadBytes;
        capture.metadata[u"selectedCoverWidth"_s] = cover->pixelSize().width();
        capture.metadata[u"selectedCoverHeight"_s] = cover->pixelSize().height();
        capture.metadata[u"selectedTotalUploadBytes"_s] = selectedTotalUploadBytes;
        capture.metadata[u"atlasTotalUploadBytes"_s] = totalUploadBytes;
        capture.metadata[u"atlasRegions"_s] = uploadRegions;
        if (atlasGeneration)
            capture.metadata[u"nativeAtlasTransfers"_s] = nativeTransfers(api, cb, cover.get());
        else
            capture.metadata[u"nativeSelectedTransfers"_s] =
                    nativeTransfers(api, cb, cover.get(), false);
        capture.metadata[u"atlasPending"_s] = atlasQueue.pending();
        capture.metadata[u"atlasRejected"_s] = qint64(atlasQueue.rejected());
        capture.metadata[u"drawCount"_s] = entities.size();
        capture.metadata[u"geometryInstanced"_s] = batched;
        capture.metadata[u"visibleEntities"_s] = visibleCount;
        capture.metadata[u"uniformBytes"_s] = int(sizeof(Uniform));
        capture.metadata[u"postUniformUploadBytes"_s] =
                input.postprocess ? post.uniformUploadBytes() : 0;
        capture.metadata[u"uniformUploadBytes"_s] = qint64(cursor);
        capture.metadata[u"uniformCapacityBytes"_s] = qint64(uniforms->size());
        capture.metadata[u"coverSnapshotCopies"_s] = qint64(coverSnapshotCopies);
        capture.metadata[u"coverSnapshotShares"_s] = qint64(coverSnapshotShares);
        capture.metadata[u"coverResourceRevision"_s] = qint64(coverResourceRevision);
        QJsonArray effectiveRegions;
        for (int i = 0; i < effectiveCoverTiles.size(); ++i) {
            const auto tile = effectiveCoverTiles[i];
            effectiveRegions.append(
                    QJsonObject{ { u"x"_s, tile.x() },
                                 { u"y"_s, tile.y() },
                                 { u"width"_s, tile.width() },
                                 { u"height"_s, tile.height() },
                                 { u"lease"_s,
                                   effectiveCoverLeases.isEmpty()
                                           ? QJsonValue()
                                           : QJsonValue(qint64(effectiveCoverLeases[i])) } });
        }
        capture.metadata[u"effectiveCoverRegions"_s] = effectiveRegions;
        const std::array<QRhiTexture *, CaptureFieldCount> textures{
            opaque.color.get(),
            opaque.normal.get(),
            opaque.depth.get(),
            shadow.color.get(),
            input.shells ? composite.color.get() : opaque.color.get(),
            input.shells ? composite.normal.get() : opaque.normal.get(),
            input.shells ? composite.depth.get() : opaque.depth.get(),
            input.postprocess ? post.output()
                              : (input.shells ? composite.color.get() : opaque.color.get()),
            cover.get(),
            input.postprocess && input.aoSamples ? post.rawAo() : opaque.color.get(),
            input.postprocess && input.aoSamples ? post.blurredAo() : opaque.color.get(),
            input.postprocess && input.aoSamples ? aoGeometry.normal.get() : opaque.normal.get(),
            input.postprocess && input.aoSamples ? aoGeometry.depth.get() : opaque.depth.get(),
            input.postprocess && input.bokeh
                    ? (dofTargets ? dofGeometry.depth.get() : aoGeometry.depth.get())
                    : opaque.depth.get()
        };
        QJsonArray sorted;
        QJsonArray opticalUniforms;
        for (const auto &draw : shells) {
            sorted.append(QJsonObject{ { u"id"_s, qint64(draw.identity) },
                                       { u"surface"_s, draw.surface },
                                       { u"depth"_s, draw.depth } });
            if (!visible[draw.entity])
                continue;
            // Inspect the exact bytes sent to updateDynamicBuffer, only on capture.
            Uniform submitted;
            std::memcpy(&submitted, bytes.constData() + draw.offset, sizeof(submitted));
            const auto array = [](const float *values, int size) {
                QJsonArray result;
                for (int i = 0; i < size; ++i)
                    result.append(values[i]);
                return result;
            };
            recordFog(submitted);
            const double clarity =
                    input.scene.individualMaterials && draw.entity < input.scene.instances.size()
                    ? input.scene.instances[draw.entity].clarity
                    : input.clarity;
            opticalUniforms.append(
                    QJsonObject{ { u"surface"_s, draw.surface },
                                 { u"warmth"_s, input.warmth },
                                 { u"clarity"_s, clarity },
                                 { u"environment"_s, input.environmentIntensity },
                                 { u"transport"_s, array(submitted.material, 4) },
                                 { u"coat"_s, array(submitted.coat, 4) },
                                 { u"tint"_s, array(submitted.tint, 3) },
                                 { u"attenuation"_s, array(submitted.attenuation, 3) } });
        }
        // Inspect the same receiver bytes uploaded above, rather than restating Input defaults.
        Uniform receiver;
        std::memcpy(&receiver, bytes.constData() + floorOffset, sizeof(receiver));
        capture.metadata[u"productFloor"_s] = QJsonObject{
            { u"enabled"_s, input.productFloor },
            { u"color"_s, QJsonArray{ receiver.tint[0], receiver.tint[1], receiver.tint[2] } },
            { u"roughness"_s, receiver.material[1] },
            { u"environment"_s, receiver.settings[3] },
            { u"kind"_s, receiver.draw[0] },
            { u"opaqueIndices"_s, input.productFloor ? int(geometry->productFloor().count) : 0 },
            { u"aoIndices"_s,
              input.productFloor && input.postprocess && input.aoSamples
                      ? int(geometry->productFloor().count)
                      : 0 },
            { u"dofIndices"_s,
              input.productFloor && input.postprocess && input.bokeh
                      ? int(geometry->productFloor().count)
                      : 0 },
            { u"castsShadow"_s, false }
        };
        QJsonArray lightMatrix;
        for (float value : receiver.lightVP)
            lightMatrix.append(value);
        capture.metadata[u"shadowState"_s] = QJsonObject{
            { u"rendered"_s, shadowRendered },
            { u"resourceRevision"_s, qint64(shadowResourceRevision) },
            { u"bindingMatches"_s,
              std::find(geometryBindings->cbeginBindings(), geometryBindings->cendBindings(),
                        QRhiShaderResourceBinding::sampledTexture(
                                ShadowBinding, QRhiShaderResourceBinding::FragmentStage,
                                shadow.hardwareDepth.get(), shadowCompare.get()))
                              != geometryBindings->cendBindings()
                      && (!shellBindings
                          || std::find(shellBindings->cbeginBindings(),
                                       shellBindings->cendBindings(),
                                       QRhiShaderResourceBinding::sampledTexture(
                                               ShadowBinding,
                                               QRhiShaderResourceBinding::FragmentStage,
                                               shadow.hardwareDepth.get(), shadowCompare.get()))
                                  != shellBindings->cendBindings()) },
            { u"separateCasterUniforms"_s, shadowOffsets.size() },
            { u"casters"_s, input.shadows ? entities.size() : 0 },
            { u"batched"_s, batched },
            { u"enabled"_s, receiver.draw[1] },
            { u"size"_s, receiver.draw[2] },
            { u"radius"_s, receiver.draw[3] },
            { u"strength"_s, receiver.optics[1] },
            { u"normalBias"_s, receiver.optics[2] },
            { u"bias"_s, receiver.optics[3] },
            { u"lightVP"_s, lightMatrix },
            { u"framebufferYUp"_s, api->isYUpInFramebuffer() },
            { u"frontCull"_s, shadowPipeline->cullMode() == QRhiGraphicsPipeline::Front },
            { u"lessEqual"_s, shadowCompare->textureCompareOp() == QRhiSampler::LessOrEqual },
            { u"linearCompare"_s,
              shadowCompare->minFilter() == QRhiSampler::Linear
                      && shadowCompare->magFilter() == QRhiSampler::Linear }
        };
        capture.metadata[u"submittedFogUniforms"_s] = fogUniforms;
        capture.metadata[u"outputOptions"_s] = QJsonArray{ out[0], out[1], out[2], out[3] };
        capture.metadata[u"captureDrawKinds"_s] = captureKinds;
        capture.metadata[u"opticalEvents"_s] = opticalEvents;
        capture.metadata[u"sortedShells"_s] = sorted;
        capture.metadata[u"submittedShellUniforms"_s] = opticalUniforms;
        readbacks.enqueue(api, cb, textures, std::move(capture), input, deliver, failed, [this] {
            return resourceInventory();
        });
    }
    ++submissionStats.rendered;
    if (input.preparationToken && prepared)
        prepared(input.preparationToken, synchronizedSubmission, atlasQueue.pending() == 0);
    // A reduced-motion scene may have no subsequent state mutation to drain the upload budget.
    if (atlasQueue.pending() > 0)
        update();
    if (diagnose) {
        const auto gpu = cb->lastCompletedGpuTime();
        diagnose({ yalahoo::monotonicMilliseconds(),
                   cpu.nsecsElapsed() / yalahoo::units::NanosecondsPerMillisecond,
                   synchronizationMs, gpu > 0 ? gpu * yalahoo::units::MillisecondsPerSecond : -1,
                   frameUploadBytes, selectedUploadBytes, qint64(cursor) });
    }
    if (input.profile && profile) {
        const auto gpu = cb->lastCompletedGpuTime();
        profile(QJsonObject{
                { u"cpuSubmitMs"_s,
                  cpu.nsecsElapsed() / yalahoo::units::NanosecondsPerMillisecond },
                { u"synchronizationMs"_s, synchronizationMs },
                { u"frameRevision"_s, qint64(input.scene.revision) },
                { u"submission"_s, qint64(synchronizedSubmission) },
                { u"aoSamples"_s, input.aoSamples },
                { u"bokeh"_s, input.bokeh },
                { u"smaa"_s, input.smaa },
                { u"logicalWidth"_s, input.scene.width },
                { u"logicalHeight"_s, input.scene.height },
                { u"targetWidth"_s, targetSize.width() },
                { u"targetHeight"_s, targetSize.height() },
                { u"inputRevision"_s, qint64(input.scene.inputRevision) },
                { u"inputReceivedNs"_s, input.scene.inputReceivedNs },
                { u"navigationRevision"_s, qint64(input.scene.navigationRevision) },
                { u"navigationInputRevision"_s, qint64(input.scene.navigationInputRevision) },
                { u"navigationCommittedNs"_s, input.scene.navigationCommittedNs },
                { u"submittedNs"_s, qint64(yalahoo::monotonicNanoseconds()) },
                { u"coverSnapshotCopies"_s, qint64(coverSnapshotCopies) },
                { u"coverSnapshotShares"_s, qint64(coverSnapshotShares) },
                { u"postprocessRevision"_s, qint64(post.revision()) },
                { u"gpuFrameMs"_s,
                  gpu > 0 ? QJsonValue(gpu * yalahoo::units::MillisecondsPerSecond)
                          : QJsonValue() },
                { u"atlasPending"_s, atlasQueue.pending() },
                { u"atlasUploadBytes"_s, frameUploadBytes },
                { u"selectedUploadBytes"_s, selectedUploadBytes },
                { u"selectedCoverWidth"_s, input.cover.width() },
                { u"selectedCoverHeight"_s, input.cover.height() },
                { u"uniformUploadBytes"_s, qint64(cursor) },
                { u"resourceGeneration"_s, qint64(generation) },
                { u"shadowRendered"_s, shadowRendered },
                { u"visibleEntities"_s, visibleCount },
                { u"resources"_s, resourceInventory() } });
    }
}
// Count owned resources once by native identity. Byte estimates describe application storage;
// driver padding, compositor memory and GPU completion remain separate unknown facts.
QJsonObject Renderer::resourceInventory()
{
    if (inventoryGeneration == generation && inventoryPostRevision == post.revision()
        && inventoryCoverRevision == coverResourceRevision
        && inventoryReadbackRevision == readbacks.revision() && !cachedInventory.isEmpty())
        return cachedInventory;
    ResourceInventory inventory;
    auto target = [&](const QString &name, const Target &t) {
        inventory.texture(name + "-color-resolve"_L1, t.color.get());
        inventory.texture(name + "-normal"_L1, t.normal.get());
        inventory.texture(name + "-linear-depth"_L1, t.depth.get());
        inventory.texture(name + "-hardware-depth"_L1, t.hardwareDepth.get());
        inventory.renderBuffer(name + "-depth-stencil"_L1, t.z.get());
        for (int i = 0; i < 3; ++i) {
            inventory.texture(name + u"-msaa-%1"_s.arg(i), t.multisample[i].get());
            inventory.renderBuffer(name + u"-msaa-%1"_s.arg(i), t.multisampleBuffers[i].get());
        }
    };
    target(u"shadow"_s, shadow);
    target(u"opaque"_s, opaque);
    target(u"shell-composite"_s, composite);
    target(u"ao-geometry"_s, aoGeometry);
    target(u"dof-geometry"_s, dofGeometry);
    inventory.texture(u"cover-or-atlas"_s, cover.get());
    inventory.texture(u"environment"_s, environment.get());
    inventory.texture(u"dfg"_s, dfg.get());
    inventory.texture(u"ltc1"_s, ltc1.get());
    inventory.texture(u"ltc2"_s, ltc2.get());
    readbacks.inventory(inventory);
    inventory.buffer(u"geometry-vertices"_s, vertices.get());
    inventory.buffer(u"geometry-indices"_s, indices.get());
    inventory.buffer(u"bounded-draw-uniforms"_s, uniforms.get());
    inventory.buffer(u"bounded-instances"_s, instanceBuffer.get());
    inventory.buffer(u"output-uniform"_s, outputUniform.get());
    post.inventory(inventory);
    inventory.texture(u"qt-item-color"_s, colorTexture());
    inventory.texture(u"qt-item-resolve"_s, resolveTexture());
    inventory.renderBuffer(u"qt-item-msaa"_s, msaaColorBuffer());
    inventory.renderBuffer(u"qt-item-depth-stencil"_s, depthStencilBuffer());
    const qint64 current = inventory.bytes();
    cachedInventory = {
        { u"preallocationPlan"_s, allocationPlan },
        { u"outputHasDepthStencil"_s,
          bool(itemOutput.rt->description().depthStencilBuffer()
               || itemOutput.rt->description().depthTexture() || depthStencilBuffer()) },
        { u"outputSamples"_s, itemOutput.rt->sampleCount() },
        { u"shadowDepthResourceId"_s, qint64(shadow.hardwareDepth->globalResourceId()) },
        { u"entries"_s, inventory.entries() },
        { u"estimatedCurrentBytes"_s, current },
        { u"resizeOldPlusNewUpperBoundBytes"_s, current + previousResourceBytes },
        { u"measuredVramBytes"_s, QJsonValue() },
        { u"scope"_s,
          u"Descriptor payloads; shared depth and transient MSAA counted once. Format 0 depth/stencil uses 8-byte upper bound. Excludes driver padding, PSO/SRB storage and Qt window swapchain/compositor (not observable here). Old+new is a conservative replacement overlap, not measured residency. Qt UI attachment estimates and their full old+new plan are separate in preallocationPlan."_s }
    };
    previousResourceBytes = current;
    inventoryGeneration = generation;
    inventoryPostRevision = post.revision();
    inventoryCoverRevision = coverResourceRevision;
    inventoryReadbackRevision = readbacks.revision();
    return cachedInventory;
}
}
SceneSubmissionStats &sceneSubmissionStats()
{
    return submissionStats;
}
QQuickRhiItemRenderer *createSceneRenderer()
{
    return new Renderer;
}
}
