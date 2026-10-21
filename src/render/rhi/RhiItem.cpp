#include "SceneGeometry.h"
#include "Units.h"
#include "RenderData.h"
#include "RhiItem.h"
#include "Logging.h"
#include "ReadbackImage.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include "ResourcePlan.h"
#include <QPointer>
#include <QQuickWindow>
#include <chrono>
#include <limits>
#include "RhiApi.h"
#include "ShellMaterial.h"
#include "Postprocess.h"
#include "TransferAudit.h"
#include "GeometryAudit.h"
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <QSet>
#include <QtEndian>
#include <QFloat16>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <cstring>
#include <algorithm>
#include <cstdio>

using namespace Qt::StringLiterals;

#if defined(Q_OS_WIN)
#  include <rhi/qrhi_platform.h>
#endif

namespace yalahoo::rhi {
namespace {
constexpr float ShadowStrength = yalahoo::rhi::renderData::PaperBottom;
// Geometry and source lighting parameters retain their original float precision at GPU upload.
constexpr auto EmptyBoundsMagnitude = 1e6f;
constexpr auto ReviewFloorDepth = .05f;
constexpr auto ProductFloorHalfExtent = 100;
constexpr auto ProductFloorDepth = 4.63f;
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
// The ordinal order matches the capture texture/output tables; aliases share a readback owner.
enum CaptureField {
    OpaqueColorField,
    OpaqueNormalField,
    OpaqueDepthField,
    ShadowField,
    CompositeColorField,
    CompositeNormalField,
    CompositeDepthField,
    PostprocessedField,
    AtlasField,
    RawAoField,
    BlurredAoField,
    AoNormalField,
    AoDepthField,
    DofDepthField,
    CaptureFieldCount
};
// Draw kinds are the production frame.glsl material selectors, including the shadow sentinel.
enum DrawKind {
    ShadowKind = -1,
    PaperKind,
    PreviewPaperKind,
    ReviewFloorKind,
    LidKind,
    SpineKind,
    TrayKind,
    ProductFloorKind
};
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
constexpr int VectorScalars = 4;
constexpr int MatrixScalars = 16;
constexpr int ClipPlaneCount = 6;
constexpr int UniformBytes = 832;
constexpr int BackgroundOffsetBytes = 320;
constexpr int OpticsOffsetBytes = 544;
constexpr int NativeVpOffsetBytes = 560;
constexpr int ScatterOffsetBytes = 736;
constexpr int FogOffsetBytes = 816;
constexpr int UniformBaseCount = 8;
constexpr int NormalOffsetBytes = 12;
constexpr int UvOffsetBytes = 24;
constexpr int InstanceMatrixSecondOffsetBytes = 32;
constexpr int InstanceMatrixThirdOffsetBytes = 48;
constexpr int InstanceUvOffsetBytes = 64;
constexpr int InstanceLightOffsetBytes = 80;
constexpr int InstanceSecondaryLightOffsetBytes = 96;
constexpr int MaximumOpticalProbe = 13;
constexpr int VisibilityProbe = 10;
constexpr int MinimumBatchedEntities = 8;
constexpr int ExtraGeometryVertices = 15;
constexpr int ExtraGeometryIndices = 21;
constexpr int ShadowFilterRadius = 4;
constexpr int ReviewFloorHalfExtent = 6;
constexpr int LightDistanceWidth = 6;
constexpr int MinimumAoSamples = 16;
constexpr int MediumAoSamples = 32;
constexpr int MaximumAoSamples = 64;

constinit SceneSubmissionStats submissionStats;
template <class T>
using Owned = std::unique_ptr<T>;
// Exactly matches frame.glsl; avoid compiler-dependent QVector3D/std140 packing.
struct alignas(renderData::VectorBytes) Uniform final
{
    float mvp[MatrixScalars], model[MatrixScalars], view[MatrixScalars], lightMvp[MatrixScalars],
            lightVP[MatrixScalars];
    float background[VectorScalars], rowSettings[VectorScalars], rowColumn[VectorScalars],
            lightColumn[VectorScalars], spotPosition[VectorScalars], spotTarget[VectorScalars],
            spotColor[VectorScalars];
    float keyColor[VectorScalars], skyColor[VectorScalars], groundColor[VectorScalars],
            lightIntensities[VectorScalars], settings[VectorScalars], clip[VectorScalars],
            draw[VectorScalars], optics[VectorScalars];
    float nativeVP[MatrixScalars], camera[VectorScalars], tint[VectorScalars],
            attenuation[VectorScalars], material[VectorScalars], coat[VectorScalars],
            areaPosition[VectorScalars], areaTarget[VectorScalars], scatter[VectorScalars],
            coverUv[VectorScalars], paperSize[VectorScalars], originLight0[VectorScalars],
            originLight1[VectorScalars], fog[VectorScalars];
};
static_assert(sizeof(Uniform) == UniformBytes
              && offsetof(Uniform, background) == BackgroundOffsetBytes
              && offsetof(Uniform, optics) == OpticsOffsetBytes
              && offsetof(Uniform, nativeVP) == NativeVpOffsetBytes
              && offsetof(Uniform, scatter) == ScatterOffsetBytes
              && offsetof(Uniform, fog) == FogOffsetBytes);
void copy(float *to, const QMatrix4x4 &m)
{
    std::memcpy(to, m.constData(), renderData::MatrixBytes);
}
void copy(float *to, QVector3D v, float w = 0)
{
    to[0] = v.x();
    to[1] = v.y();
    to[2] = v.z();
    to[3] = w;
}
void copy(float *to, QVector4D v)
{
    to[0] = v.x();
    to[1] = v.y();
    to[2] = v.z();
    to[3] = v.w();
}
struct Mesh final
{
    quint32 start = 0, count = 0;
    QVector3D center{ };
};
struct Target final
{
    Q_DISABLE_COPY_MOVE(Target)
    Target() = default;
    bool mrt = false;
    bool prepass = false;
    QSize size;
    Owned<QRhiTexture> color, normal, depth;
    std::array<Owned<QRhiTexture>, yalahoo::rhi::renderData::CaseSurfaceCount> multisample;
    std::array<Owned<QRhiRenderBuffer>, yalahoo::rhi::renderData::CaseSurfaceCount>
            multisampleBuffers;
    std::shared_ptr<QRhiRenderBuffer> z;
    Owned<QRhiTexture> hardwareDepth;
    Owned<QRhiRenderPassDescriptor> pass;
    Owned<QRhiTextureRenderTarget> rt;
    void reset()
    {
        rt.reset();
        pass.reset();
        z.reset();
        hardwareDepth.reset();
        for (auto &t : multisample)
            t.reset();
        for (auto &buffer : multisampleBuffers)
            buffer.reset();
        depth.reset();
        normal.reset();
        color.reset();
    }
    // Dependencies are destroyed in reverse declaration order: target before attachments.
};
struct Pending final
{
    Q_DISABLE_COPY_MOVE(Pending)
    Pending() = default;
    std::array<QRhiReadbackResult, CaptureFieldCount> results;
    Capture capture;
    std::function<void(Capture)> deliver;
    std::function<void(QString)> failed;
    int remaining = 0;
    std::array<int, CaptureFieldCount> readbackOwner;
    int drainFrames = 0;
    bool yUp = false;
    int opticalProbe = 0;
    bool captureShadowDepth = false;
    std::array<QSize, CaptureFieldCount> logicalReadbackSizes;
};
QByteArray floatTextureBytes(const QString &path, qsizetype expected)
{
    const auto bytes = preparedAssets().bytes.value(path);
    require(bytes.size() == expected, "Invalid RHI float lookup bytes");
    return bytes;
}
QVector<bool> visibleEntities(const Input &input, const QVector<QMatrix4x4> &entities)
{
    QVector<bool> visible(entities.size(), true);
    // Conservative homogeneous clip test of the authored complete case bounds.
    // Keep every logical identity and its global sort; skip only provably off-screen draws.
    for (int i = 0; i < entities.size(); ++i) {
        const auto matrix = input.scene.projection * input.scene.view * entities[i];
        std::array<bool, ClipPlaneCount> outside{ true, true, true, true, true, true };
        for (float x :
             { -yalahoo::sceneGeometry::CaseHalfWidth, yalahoo::sceneGeometry::CaseHalfWidth })
            for (float y : { yalahoo::sceneGeometry::CaseBottom, yalahoo::sceneGeometry::CaseTop })
                for (float z : { -yalahoo::sceneGeometry::CaseHalfDepth,
                                 yalahoo::sceneGeometry::CaseHalfDepth }) {
                    const auto v = matrix * QVector4D(x, y, z, 1);
                    outside[0] = outside[0] && v.x() < -v.w();
                    outside[1] = outside[1] && v.x() > v.w();
                    outside[2] = outside[2] && v.y() < -v.w();
                    outside[3] = outside[3] && v.y() > v.w();
                    outside[4] = outside[4] && v.z() < -v.w();
                    outside[5] = outside[5] && v.z() > v.w();
                }
        visible[i] = std::none_of(outside.begin(), outside.end(), [](bool v) {
            return v;
        });
    }
    return visible;
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
        if (api && !pending.empty())
            api->finish();
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
        failed = [guard](QString message) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [guard, message] {
                            if (guard)
                                Q_EMIT guard->renderFailed(message);
                        },
                        Qt::QueuedConnection);
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
        if (failedRenderer || input.sceneOccluded)
            return;
        try {
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
        if (input.sceneOccluded) {
            cb->beginPass(renderTarget(), Qt::transparent, { 1, 0 });
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
    void prepareResources(QRhiCommandBuffer *cb);
    void renderFrame(QRhiCommandBuffer *cb);
    QJsonObject resourceInventory();
    void reportFailure(const char *message)
    {
        qCCritical(logRender) << "RHI rendering failed"
                              << "backend=" << (api ? api->backendName() : "uninitialized")
                              << "submission=" << synchronizedSubmission << "target="
                              << (renderTarget() ? renderTarget()->pixelSize() : QSize())
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
    quint64 coverResourceRevision = 0;
    QJsonObject cachedInventory, allocationPlan;
    struct AllocationKey final
    {
        QSize size, cover;
        int shadow = 0, samples = 0, aoSamples = 0;
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
    void enqueueAtlas();
    void makeTarget(Target &target, QSize size, bool mrt, int samples = 1, bool mips = false,
                    QRhiTexture::Format format = QRhiTexture::RGBA16F,
                    std::shared_ptr<QRhiRenderBuffer> sharedDepth = { });
    Owned<QRhiGraphicsPipeline> pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                         bool shadow, bool shell = false, bool instanced = false);
    Uniform uniform(const QMatrix4x4 &model, int kind, int entity = -1) const;
    Uniform uniformTemplate(int kind) const;
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
    std::array<Owned<QRhiTexture>, CaptureFieldCount> alignedReadbacks;
    int readbackPackAlignment = 0;
    quint64 alignedReadbackRevision = 0;
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
    Mesh lid, spine, tray, paper, floor, productFloor, backgroundTriangle;
    QByteArray vertexBytes, indexBytes;
    QSize targetSize;
    int shadowSize = 0, uniformStride = 0, captureSamples = 0;
    bool targetsHaveShells = false;
    bool areaShader = false, opticsProbeShader = false, visibilityProbeShader = false;
    bool diagnosticTargets = false;
    std::array<Uniform, UniformBaseCount> uniformBases;
    QVector<QVector4D> entityLight0, entityLight1;
    QVector<QMatrix4x4> lightEntities;
    QByteArray uniformScratch;
    QImage detachedCover;
    QVector4D cachedRowSettings, cachedPrintSettings;
    QVector3D cachedRowColumn, cachedLightColumn;
    QMatrix4x4 nativeVP, nativeLightVP, canonicalLightVP;
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
    std::vector<std::shared_ptr<Pending>> pending;
};
void Renderer::enqueueAtlas()
{
    if (expectedAtlasGeneration != atlasGeneration || expectedCoverLeases != input.coverLeases
        || expectedCoverTiles != input.coverTiles
        || expectedFallbackTiles != input.fallbackCoverTiles
        || expectedFallbackLeases != input.fallbackCoverLeases) {
        for (int i = 0; i < input.coverLeases.size(); ++i) {
            const auto tile = input.coverTiles[i];
            for (int y = tile.y(); y < tile.y() + tile.height(); y += renderData::CoverTilePixels)
                for (int x = tile.x(); x < tile.x() + tile.width();
                     x += renderData::CoverTilePixels)
                    atlasQueue.expect((y / renderData::CoverTilePixels)
                                                      * (input.atlasSize.width()
                                                         / renderData::CoverTilePixels)
                                              + x / renderData::CoverTilePixels,
                                      input.coverLeases[i]);
        }
        expectedAtlasGeneration = atlasGeneration;
        for (int i = 0; i < input.fallbackCoverTiles.size(); ++i) {
            const auto tile = input.fallbackCoverTiles[i];
            atlasQueue.expect(
                    (tile.y() / renderData::CoverTilePixels)
                                    * (input.atlasSize.width() / renderData::CoverTilePixels)
                            + tile.x() / renderData::CoverTilePixels,
                    input.fallbackCoverLeases[i]);
        }
        expectedCoverLeases = input.coverLeases;
        expectedCoverTiles = input.coverTiles;
        expectedFallbackTiles = input.fallbackCoverTiles;
        expectedFallbackLeases = input.fallbackCoverLeases;
    }
    atlasQueue.enqueue(input.atlasUploads);
}
void Renderer::reset()
{
    for (auto &texture : alignedReadbacks)
        texture.reset();
    readbackPackAlignment = 0;
    alignedReadbackRevision = 0;
    post.reset();
    prepassPipeline.reset();
    prepassBackgroundPipeline.reset();
    opaqueInstances.reset();
    shadowInstances.reset();
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
    shadowPipeline.reset();
    outputBindings.reset();
    geometryBindings.reset();
    shadowBindings.reset();
    opaque.reset();
    composite.reset();
    shadow.reset();
    aoGeometry.reset();
    dofGeometry.reset();
    postTargets = false;
    uniforms.reset();
    uniformScratch.clear();
    outputUniform.reset();
    vertices.reset();
    indices.reset();
    instanceBuffer.reset();
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
void Renderer::makeTarget(Target &target, QSize size, bool mrt, int samples, bool mips,
                          QRhiTexture::Format format, std::shared_ptr<QRhiRenderBuffer> sharedDepth)
{
    target.reset();
    target.mrt = mrt;
    target.prepass = mrt && format == QRhiTexture::RGBA8;
    target.size = size;
    auto texture = [&](QRhiTexture::Format pixelFormat, bool mipmapped = false) {
        Owned<QRhiTexture> t(api->newTexture(
                pixelFormat, size, 1,
                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource
                        | (mipmapped ? QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips
                                     : QRhiTexture::Flags())));
        require(t->create(), "Cannot create RHI float target");
        return t;
    };
    if (!target.prepass)
        target.color = texture(format, mips);
    if (format == QRhiTexture::R32F) {
        target.hardwareDepth.reset(
                api->newTexture(QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
        require(target.hardwareDepth->create(),
                "Cannot create source-compatible shadow depth texture");
    } else if (sharedDepth) {
        target.z = sharedDepth;
    } else {
        target.z.reset(api->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, samples));
        require(target.z->create(), "Cannot create RHI depth attachment");
    }
    QRhiTextureRenderTargetDescription desc;
    if (mrt) {
        target.normal = texture(QRhiTexture::RGBA8);
        target.depth = texture(QRhiTexture::RGBA8);
        if (target.prepass)
            desc.setColorAttachments({ target.normal.get(), target.depth.get() });
        else
            desc.setColorAttachments(
                    { target.color.get(), target.normal.get(), target.depth.get() });
    } else
        desc.setColorAttachments({ target.color.get() });
    if (samples > 1) {
        QVector<QRhiColorAttachment> attachments;
        const std::array<QRhiTexture *, yalahoo::rhi::renderData::CaseSurfaceCount> resolves{
            target.color.get(), target.normal.get(), target.depth.get()
        };
        for (int i = 0; i < (mrt ? yalahoo::rhi::renderData::CaseSurfaceCount : 1); ++i) {
            QRhiColorAttachment attachment;
            const auto *context = QOpenGLContext::currentContext();
            const bool textureStorageAvailable = api->backend() != QRhi::OpenGLES2
                    || (context && context->getProcAddress("glTexStorage2DMultisample"));
            if (textureStorageAvailable) {
                target.multisample[i].reset(api->newTexture(resolves[i]->format(), size, samples,
                                                            QRhiTexture::RenderTarget));
                require(target.multisample[i]->create(), "Cannot create RHI MSAA attachment");
                attachment.setTexture(target.multisample[i].get());
            } else {
                require(api->isFeatureSupported(QRhi::MultisampleRenderBuffer),
                        "Backend cannot allocate the requested MSAA attachment");
                target.multisampleBuffers[i].reset(api->newRenderBuffer(
                        QRhiRenderBuffer::Color, size, samples, { }, resolves[i]->format()));
                require(target.multisampleBuffers[i]->create(), "Cannot create RHI MSAA buffer");
                require(target.multisampleBuffers[i]->backingFormat() == resolves[i]->format(),
                        "MSAA buffer changed the requested color precision");
                attachment.setRenderBuffer(target.multisampleBuffers[i].get());
            }
            attachment.setResolveTexture(resolves[i]);
            attachments.append(attachment);
        }
        desc.setColorAttachments(attachments.cbegin(), attachments.cend());
    }
    if (target.hardwareDepth)
        desc.setDepthTexture(target.hardwareDepth.get());
    else
        desc.setDepthStencilBuffer(target.z.get());
    target.rt.reset(api->newTextureRenderTarget(
            desc,
            sharedDepth ? QRhiTextureRenderTarget::PreserveDepthStencilContents
                        : QRhiTextureRenderTarget::Flags()));
    target.pass.reset(target.rt->newCompatibleRenderPassDescriptor());
    target.rt->setRenderPassDescriptor(target.pass.get());
    require(target.rt->create(), "Cannot create RHI render target");
}
Owned<QRhiGraphicsPipeline> Renderer::pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                               bool isShadow, bool isShell, bool instanced)
{
    Owned<QRhiGraphicsPipeline> p(api->newGraphicsPipeline());
    QString fragment = isShadow ? u":/rhi/shadow.frag.qsb"_s
            : target.prepass    ? u":/rhi/prepass.frag.qsb"_s
                                : u":/rhi/opaque.frag.qsb"_s;
    if (isShell) {
        fragment = input.areaIntensity > 0 ? u":/rhi/shell"_s : u":/rhi/shell-basic"_s;
        if (api->backend() != QRhi::OpenGLES2 || api->isFeatureSupported(QRhi::Compute))
            fragment += "-early"_L1;
        fragment += ".frag.qsb"_L1;
#ifdef YALAHOO_RHI_AUDIT
        if (input.opticalProbe > 0)
            fragment = u":/rhi/optics-probe.frag.qsb"_s;
#endif
    }
    p->setShaderStages({ { QRhiShaderStage::Vertex,
                           shader(isShadow ? (instanced ? u":/rhi/shadow-instanced.vert.qsb"_s
                                                        : u":/rhi/shadow.vert.qsb"_s)
                                           : (instanced ? u":/rhi/geometry-instanced.vert.qsb"_s
                                                        : u":/rhi/geometry.vert.qsb"_s)) },
                         { QRhiShaderStage::Fragment, shader(fragment) } });
    QRhiVertexInputLayout layout;
    layout.setBindings({ { renderData::VertexBytes } });
    layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                           { 0, 1, QRhiVertexInputAttribute::Float3, NormalOffsetBytes },
                           { 0, 2, QRhiVertexInputAttribute::Float2, UvOffsetBytes } });
    if (instanced) {
        layout.setBindings({ { renderData::VertexBytes },
                             { renderData::InstanceBytes, QRhiVertexInputBinding::PerInstance } });
        layout.setAttributes(
                { { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                  { 0, 1, QRhiVertexInputAttribute::Float3, NormalOffsetBytes },
                  { 0, 2, QRhiVertexInputAttribute::Float2, UvOffsetBytes },
                  { 1, 3, QRhiVertexInputAttribute::Float4, 0 },
                  { 1, 4, QRhiVertexInputAttribute::Float4, renderData::VectorBytes },
                  { 1, 5, QRhiVertexInputAttribute::Float4, InstanceMatrixSecondOffsetBytes },
                  { 1, 6, QRhiVertexInputAttribute::Float4, InstanceMatrixThirdOffsetBytes },
                  { 1, 7, QRhiVertexInputAttribute::Float4, InstanceUvOffsetBytes },
                  { 1, 8, QRhiVertexInputAttribute::Float4, InstanceLightOffsetBytes },
                  { 1, 9, QRhiVertexInputAttribute::Float4, InstanceSecondaryLightOffsetBytes } });
    }
    p->setVertexInputLayout(layout);
    p->setShaderResourceBindings(bindings);
    p->setRenderPassDescriptor(target.pass.get());
    p->setSampleCount(target.rt->sampleCount());
    p->setDepthTest(true);
    p->setDepthWrite(true);
    p->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    p->setFrontFace(QRhiGraphicsPipeline::CCW);
    // Three's shadow pass renders back faces for source FrontSide materials.
    p->setCullMode(isShadow ? QRhiGraphicsPipeline::Front : QRhiGraphicsPipeline::Back);
    if (!isShadow) {
        if (target.prepass)
            p->setTargetBlends({ { }, { } });
        else if (target.mrt)
            p->setTargetBlends({ { }, { }, { } });
        else
            p->setTargetBlends({ { } });
    }
    require(p->create(), "RHI graphics pipeline creation failed");
    return p;
}
void Renderer::prepareResources(QRhiCommandBuffer *)
{
    if (api != rhi()) {
        if (api && !pending.empty())
            api->finish();
        pending.clear();
        reset();
        api = rhi();
        qCInfo(logRender) << "RHI renderer initialized" << "backend=" << api->backendName()
                          << "device=" << api->driverInfo().deviceName;
    }
    if (input.cover.isNull() && !input.atlasGeneration)
        return;
    const auto requestedSize = renderTarget()->pixelSize();
    const auto requestedCover = input.atlasGeneration ? input.atlasSize : input.cover.size();
    const AllocationKey key{ requestedSize,
                             requestedCover,
                             input.shadowSize,
                             input.captureSamples,
                             input.aoSamples,
                             input.shells,
                             bool(deliver),
                             input.postprocess,
                             input.bokeh,
                             input.smaa,
                             input.filterCovers,
                             input.navigationDpr,
                             input.textResourceRevision };
    if (key != allocationKey || plannedOpeningLayer != input.openingLayerSize
        || plannedPanelLayer != input.panelLayerSize || plannedHdr != input.hdrActive
        || plannedDisplayLayer != input.displayLayerSize || !vertices) {
        for (const auto format :
             { QRhiTexture::RGBA16F, QRhiTexture::RGBA8, QRhiTexture::R32F, QRhiTexture::D32F })
            require(api->isTextureFormatSupported(format, QRhiTexture::RenderTarget),
                    "Selected GPU lacks a required RHI target format");
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
        allocationPlan = resourcePlan(input, requestedSize, bool(deliver),
                                      api->ubufAligned(sizeof(Uniform)), geometryBytes);
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
        vertexBytes.clear();
        indexBytes.clear();
        auto append = [&](const assets::Batch &batch) {
            const auto base = quint32(vertexBytes.size() / renderData::VertexBytes);
            Mesh mesh{ quint32(indexBytes.size() / renderData::FloatBytes),
                       quint32(batch.indices.size()) };
            QVector3D minimum(EmptyBoundsMagnitude, EmptyBoundsMagnitude, EmptyBoundsMagnitude),
                    maximum(-EmptyBoundsMagnitude, -EmptyBoundsMagnitude, -EmptyBoundsMagnitude);
            for (const auto &v : batch.vertices)
                for (int axis = 0; axis < yalahoo::units::VectorAxes; ++axis) {
                    minimum[axis] = std::min(minimum[axis], v[axis]);
                    maximum[axis] = std::max(maximum[axis], v[axis]);
                }
            mesh.center = (minimum + maximum) * .5f;
            vertexBytes.append(reinterpret_cast<const char *>(batch.vertices.constData()),
                               batch.vertices.size() * renderData::VertexBytes);
            for (auto index : batch.indices) {
                index += base;
                indexBytes.append(reinterpret_cast<const char *>(&index), renderData::FloatBytes);
            }
            return mesh;
        };
        const auto &batches = preparedAssets().geometry;
        lid = append(batches[0]);
        spine = append(batches[1]);
        tray = append(batches[2]);
        assets::Batch quad;
        quad.vertices = {
            { { -yalahoo::rhi::renderData::PaperLeftMagnitude,
                yalahoo::rhi::renderData::PaperBottom, yalahoo::rhi::renderData::PaperFront, 0, 0,
                1, 0, 1 } },
            { { yalahoo::rhi::renderData::PaperRight, yalahoo::rhi::renderData::PaperBottom,
                yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 1, 1 } },
            { { yalahoo::rhi::renderData::PaperRight, yalahoo::rhi::renderData::PaperTop,
                yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 1, 0 } },
            { { -yalahoo::rhi::renderData::PaperLeftMagnitude, yalahoo::rhi::renderData::PaperTop,
                yalahoo::rhi::renderData::PaperFront, 0, 0, 1, 0, 0 } }
        };
        quad.indices = { 0, 1, 2, 0, 2, 3 };
        paper = append(quad);
        quad.vertices = {
            { { -ReviewFloorHalfExtent, -ReviewFloorDepth, ReviewFloorHalfExtent, 0, 1, 0, 0, 0 } },
            { { ReviewFloorHalfExtent, -ReviewFloorDepth, ReviewFloorHalfExtent, 0, 1, 0, 1, 0 } },
            { { ReviewFloorHalfExtent, -ReviewFloorDepth, -ReviewFloorHalfExtent, 0, 1, 0, 1, 1 } },
            { { -ReviewFloorHalfExtent, -ReviewFloorDepth, -ReviewFloorHalfExtent, 0, 1, 0, 0, 1 } }
        };
        floor = append(quad);
        // PlaneGeometry(200, 200), rotation.x=-PI/2, position.y=-4.63.
        // Baked once into the shared immutable geometry; never a shadow caster.
        quad.vertices = { { { -ProductFloorHalfExtent, -ProductFloorDepth, ProductFloorHalfExtent,
                              0, 1, 0, 0, 0 } },
                          { { ProductFloorHalfExtent, -ProductFloorDepth, ProductFloorHalfExtent, 0,
                              1, 0, 1, 0 } },
                          { { ProductFloorHalfExtent, -ProductFloorDepth, -ProductFloorHalfExtent,
                              0, 1, 0, 1, 1 } },
                          { { -ProductFloorHalfExtent, -ProductFloorDepth, -ProductFloorHalfExtent,
                              0, 1, 0, 0, 1 } } };
        productFloor = append(quad);
        quad.vertices = { { { -1, -1, 0, 0, 0, 1, 0, 0 } },
                          { { 3, -1, 0, 0, 0, 1, 2, 0 } },
                          { { -1, 3, 0, 0, 0, 1, 0, 2 } } };
        quad.indices = { 0, 1, 2 };
        backgroundTriangle = append(quad);
        vertices.reset(api->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                      vertexBytes.size()));
        require(vertices->create(), "RHI vertex buffer failed");
        indices.reset(
                api->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, indexBytes.size()));
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
    const auto coverSize = input.atlasGeneration ? input.atlasSize : input.cover.size();
    const bool replaceCover = !cover || cover->pixelSize() != coverSize
            || atlasGeneration != input.atlasGeneration
            || cover->flags().testFlag(QRhiTexture::MipMapped) != input.filterCovers;
    const bool replaceTargets = targetSize != renderTarget()->pixelSize()
            || shadowSize != std::max(1, input.shadowSize) || captureSamples != input.captureSamples
            || targetsHaveShells != input.shells || diagnosticTargets != bool(deliver)
            || !opaque.rt;
    if (replaceCover && !replaceTargets) {
        require(std::max(coverSize.width(), coverSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "Cover exceeds GPU dimensions");
        auto previous = std::move(cover);
        cover.reset(api->newTexture(
                QRhiTexture::RGBA8, coverSize, 1,
                QRhiTexture::UsedAsTransferSource
                        | (input.filterCovers ? QRhiTexture::MipMapped : QRhiTexture::Flags())));
        require(cover->create(), "Cover texture replacement failed");
        for (auto *bindings : { geometryBindings.get(), shellBindings.get() }) {
            if (!bindings)
                continue;
            QVector<QRhiShaderResourceBinding> values(bindings->cbeginBindings(),
                                                      bindings->cendBindings());
            values[1] = QRhiShaderResourceBinding::sampledTexture(
                    CoverBinding,
                    QRhiShaderResourceBinding::VertexStage
                            | QRhiShaderResourceBinding::FragmentStage,
                    cover.get(), input.filterCovers ? mipLinear.get() : linear.get());
            bindings->setBindings(values.cbegin(), values.cend());
            require(bindings->create(), "Cover binding replacement failed");
        }
        ++coverResourceRevision;
        coverDirty = true;
        atlasGeneration = input.atlasGeneration;
        if (atlasGeneration) {
            atlasQueue.reset(coverSize, atlasGeneration);
            enqueueAtlas();
        }
    }
    if (replaceTargets) {
        post.reset();
        prepassPipeline.reset();
        opaqueInstances.reset();
        shadowInstances.reset();
        prepassInstances.reset();
        coverageInstances.reset();
        prepassBackgroundPipeline.reset();
        postTargets = false;
        outputSource = nullptr;
        shellPipeline.reset();
        coveragePipeline.reset();
        copyPipeline.reset();
        copyBindings.reset();
        shellBindings.reset();
        outputPipeline.reset();
        backgroundPipeline.reset();
        opaquePipeline.reset();
        shadowPipeline.reset();
        outputBindings.reset();
        geometryBindings.reset();
        shadowBindings.reset();
        targetSize = renderTarget()->pixelSize();
        shadowSize = std::max(1, input.shadowSize);
        captureSamples = input.captureSamples;
        targetsHaveShells = input.shells;
        diagnosticTargets = bool(deliver);
        require(api->supportedSampleCounts().contains(captureSamples),
                "Requested capture MSAA is unsupported");
        ++generation;
        shadowDirty = true;
        require(std::max(targetSize.width(), targetSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "RHI target exceeds GPU dimensions");
        require(shadowSize <= api->resourceLimit(QRhi::TextureSizeMax),
                "Shadow target exceeds GPU dimensions");
        makeTarget(opaque, targetSize, diagnosticTargets, captureSamples, input.shells);
        if (input.shells)
            makeTarget(composite, targetSize, diagnosticTargets, captureSamples, false,
                       QRhiTexture::RGBA16F, opaque.z);
        else
            composite.reset();
        makeTarget(shadow, { shadowSize, shadowSize }, false, 1, false, QRhiTexture::R32F);
        require(std::max(coverSize.width(), coverSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "Atlas exceeds GPU dimensions");
        if (replaceCover) {
            ++coverResourceRevision;
            cover.reset(api->newTexture(QRhiTexture::RGBA8, coverSize, 1,
                                        QRhiTexture::UsedAsTransferSource
                                                | (input.filterCovers ? QRhiTexture::MipMapped
                                                                      : QRhiTexture::Flags())));
            require(cover->create(), "Cover texture failed");
            coverDirty = true;
            atlasGeneration = input.atlasGeneration;
            if (atlasGeneration) {
                atlasQueue.reset(coverSize, atlasGeneration);
                enqueueAtlas();
            }
        }
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
        shadowBindings.reset(api->newShaderResourceBindings());
        shadowBindings->setBindings({ QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0, stages, uniforms.get(), sizeof(Uniform)) });
        require(shadowBindings->create(), "Shadow bindings failed");
        opaquePipeline = pipeline(opaque, geometryBindings.get(), false);
        shadowPipeline = pipeline(shadow, shadowBindings.get(), true);
        opaqueInstances = pipeline(opaque, geometryBindings.get(), false, false, true);
        shadowInstances = pipeline(shadow, shadowBindings.get(), true, false, true);
        backgroundPipeline = pipeline(opaque, geometryBindings.get(), false);
        backgroundPipeline->setDepthTest(false);
        backgroundPipeline->setDepthWrite(false);
        backgroundPipeline->setCullMode(QRhiGraphicsPipeline::None);
        require(backgroundPipeline->create(), "Background pipeline failed");
        if (input.shells) {
            shellBindings.reset(api->newShaderResourceBindings());
            const auto fragment = QRhiShaderResourceBinding::FragmentStage;
            shellBindings->setBindings(
                    { QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                              0, stages, uniforms.get(), sizeof(Uniform)),
                      QRhiShaderResourceBinding::sampledTexture(CoverBinding, stages, cover.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(EnvironmentBinding, fragment,
                                                                environment.get(), linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(ShadowBinding, fragment,
                                                                shadow.hardwareDepth.get(),
                                                                shadowCompare.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              CaptureBinding, fragment, opaque.color.get(), mipLinear.get()),
                      QRhiShaderResourceBinding::sampledTexture(DfgBinding, fragment, dfg.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(LtcFirstBinding, fragment,
                                                                ltc1.get(), linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(LtcSecondBinding, fragment,
                                                                ltc2.get(), linear.get()) });
            require(shellBindings->create(), "Shell bindings failed");
            shellPipeline = pipeline(composite, shellBindings.get(), false, true);
            areaShader = input.areaIntensity > 0;
            opticsProbeShader = input.opticalProbe > 0;
            // The visibility pass writes the exact source front-surface depth first.
            // Equal/read-only depth allows early rejection before expensive optical shading.
            shellPipeline->setDepthOp(QRhiGraphicsPipeline::Equal);
            shellPipeline->setDepthWrite(false);
            require(shellPipeline->create(), "Shell visible-surface pipeline failed");
            visibilityProbeShader = false;
            coveragePipeline = pipeline(composite, geometryBindings.get(), false);
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
            coverageInstances = pipeline(composite, geometryBindings.get(), false, false, true);
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
                      QRhiShaderResourceBinding::uniformBuffer(CoverBinding, fragment,
                                                               outputUniform.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              EnvironmentBinding, fragment,
                              diagnosticTargets ? opaque.normal.get() : opaque.color.get(),
                              nearest.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              ShadowBinding, fragment,
                              diagnosticTargets ? opaque.depth.get() : opaque.color.get(),
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
    }
    if (input.shells
        && (areaShader != (input.areaIntensity > 0)
            || opticsProbeShader != (input.opticalProbe > 0))) {
        shellPipeline = pipeline(composite, shellBindings.get(), false, true);
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
            makeTarget(aoGeometry, aoSize, true, 1, false, QRhiTexture::RGBA8);
        if (wantDof)
            makeTarget(dofGeometry, targetSize, true, 1, false, QRhiTexture::RGBA8);
        if (wantAo || wantDof) {
            auto &target = wantAo ? aoGeometry : dofGeometry;
            prepassPipeline = pipeline(target, geometryBindings.get(), false);
            prepassInstances = pipeline(target, geometryBindings.get(), false, false, true);
            prepassBackgroundPipeline = pipeline(target, geometryBindings.get(), false);
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
    // QQuickRhiItem may replace its render-pass descriptor when only its output changes.
    if (outputPipeline && outputSource == resolvedColor && outputPostRevision == post.revision()
        && outputPass == renderTarget()->renderPassDescriptor()
        && outputSamples == renderTarget()->sampleCount())
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
    outputPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    outputPipeline->setSampleCount(renderTarget()->sampleCount());
    require(outputPipeline->create(), "Output pipeline failed");
    outputSource = resolvedColor;
    outputPass = renderTarget()->renderPassDescriptor();
    outputSamples = renderTarget()->sampleCount();
    outputPostRevision = post.revision();
}
Uniform Renderer::uniformTemplate(int kind) const
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
         QVector4D(float(input.scene.nearPlane), float(input.scene.farPlane),
                   api->isYUpInFramebuffer() ? 1 : 0, api->isClipDepthZeroToOne() ? 1 : 0));
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
Uniform Renderer::uniform(const QMatrix4x4 &model, int kind, int entity) const
{
    auto u = uniformBases[kind + 1];
    // mvp remains reserved in the std140 ABI; both geometry paths reuse world position.
    copy(u.model, model);
    copy(u.lightMvp, nativeLightVP * model);
    if (input.atlasGeneration && entity >= 0) {
        const auto tile = effectiveCoverTiles[entity];
        copy(u.coverUv,
             QVector4D(float(tile.x()) / input.atlasSize.width(),
                       float(tile.y()) / input.atlasSize.height(),
                       float(tile.width()) / input.atlasSize.width(),
                       float(tile.height()) / input.atlasSize.height()));
        if (!effectiveCoverLeases.isEmpty()
            && !atlasQueue.ready(tile, effectiveCoverLeases[entity]))
            copy(u.coverUv, QVector4D());
    }
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
void Renderer::renderFrame(QRhiCommandBuffer *cb)
{
    QElapsedTimer cpu;
    if (input.profile || diagnose)
        cpu.start();
    // initialize may precede synchronize in Qt; defer resource-size changes to a fresh frame.
    if (input.cover.isNull() && !input.atlasGeneration)
        return;
    prepareResources(cb);
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [](const auto &p) {
                                     return p->remaining == 0;
                                 }),
                  pending.end());
    ResourceUpdates updates(api->nextResourceUpdateBatch());
    if (uploadGeometry) {
        updates->uploadStaticBuffer(vertices.get(), vertexBytes);
        updates->uploadStaticBuffer(indices.get(), indexBytes);
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
        // Filter premultiplied texels, then unpremultiply in the paper shader. Straight
        // transparent black texels otherwise produce a dark line at the alpha-test edge.
        const auto image = input.cover.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        updates->uploadTexture(cover.get(),
                               { { 0, 0, QRhiTextureSubresourceUploadDescription(image) } });
        selectedUploadBytes = image.sizeInBytes();
        if (input.filterCovers) {
            require(input.coverMipmaps.size() == api->mipLevelsForSize(image.size()) - 1,
                    "Cover filtering requires a complete independently generated mip chain");
            for (int level = 1; level <= input.coverMipmaps.size(); ++level) {
                const auto &mip = input.coverMipmaps[level - 1];
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
    effectiveCoverTiles = input.coverTiles;
    effectiveCoverLeases = input.coverLeases;
    for (int i = 0; i < input.fallbackCoverTiles.size(); ++i)
        if (!atlasQueue.ready(effectiveCoverTiles[i], effectiveCoverLeases[i])) {
            effectiveCoverTiles[i] = input.fallbackCoverTiles[i];
            effectiveCoverLeases[i] = input.fallbackCoverLeases[i];
        }
    QVector<QMatrix4x4> entities = input.entities;
    if (entities.isEmpty() && !input.poolFrame)
        entities.append(input.scene.caseTransform);
    const auto visible = visibleEntities(input, entities);
    require(entities.size() <= renderData::MaximumCaseEntities,
            "RHI visible pool capacity exceeded");
    shadowDirty = shadowDirty || shadowEntities != entities || cachedShadows != input.shadows;
    const bool batched = entities.size() >= MinimumBatchedEntities;
    nativeVP = api->clipSpaceCorrMatrix() * input.scene.projection * input.scene.view;
    canonicalLightVP = lightProjection() * lightView();
    nativeLightVP = api->clipSpaceCorrMatrix() * canonicalLightVP;
    for (int kind = ShadowKind; kind <= ProductFloorKind; ++kind)
        uniformBases[kind + 1] = uniformTemplate(kind);
    const QVector4D printSettings(input.printAmbient, input.rowDetail, input.rowFill, 0);
    if (lightEntities != entities || cachedRowSettings != input.rowSettings
        || cachedPrintSettings != printSettings || cachedRowColumn != input.rowColumn
        || cachedLightColumn != input.lightColumn) {
        entityLight0.clear();
        entityLight1.clear();
        for (const auto &model : entities) {
            const auto origin = model.column(yalahoo::units::VectorAxes).toVector3D();
            const float row = (origin.z() - input.rowColumn.z()
                               - input.rowSettings.w() * yalahoo::sceneGeometry::RowSpacing)
                    / (.5f * input.rowSettings.z() * yalahoo::sceneGeometry::RowSpacing);
            const float lane = (origin.x() - input.rowColumn.x()) / RibbonLaneWidth;
            const float lane2 = lane * lane;
            const float field = std::exp(-GaussianHalfMaximumExponent * row * row)
                    * std::exp(-lane2 * lane2 * lane2);
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
    auto &bytes = uniformScratch;
    quint32 cursor = 0;
    auto append = [&](const QMatrix4x4 &m, int kind, int entity = -1) {
        auto u = uniform(m, kind, entity);
        const auto offset = cursor;
        require(cursor + sizeof(u) <= quint32(bytes.size()), "Draw uniforms exceed bounded buffer");
        std::memcpy(bytes.data() + cursor, &u, sizeof(u));
        cursor += uniformStride;
        return offset;
    };
    QVector<quint32> shadowOffsets, opaqueOffsets, prepassOffsets;
    QVector<int> opaqueOrder;
    opaqueOrder.reserve(entities.size());
    for (int i = 0; i < entities.size(); ++i)
        opaqueOrder.append(i);
    if (input.shells) {
        QVector<float> depths;
        depths.reserve(entities.size());
        for (const auto &model : entities)
            depths.append(-(input.scene.view * model * QVector4D(paper.center, 1)).z());
        std::sort(opaqueOrder.begin(), opaqueOrder.end(), [&](int a, int b) {
            const float da = depths[a];
            const float db = depths[b];
            if (da != db)
                return da < db;
            const auto ia = input.entityIds.isEmpty() ? quint64(a) : input.entityIds[a];
            const auto ib = input.entityIds.isEmpty() ? quint64(b) : input.entityIds[b];
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
    struct ShellDraw final
    {
        Mesh mesh;
        quint32 offset;
        float depth;
        quint64 identity;
        int surface;
        int entity;
    };
    QVector<ShellDraw> shells;
    if (input.shells) {
        shells.reserve(entities.size() * yalahoo::rhi::renderData::CaseSurfaceCount);
        require(entities.size() == 1 || input.entityIds.size() == entities.size(),
                "Shell instances require stable IDs");
        for (int i : opaqueOrder) {
            const std::array<Mesh, yalahoo::rhi::renderData::CaseSurfaceCount> meshes{ lid, spine,
                                                                                       tray };
            const auto id = input.entityIds.isEmpty() ? 0 : input.entityIds[i];
            for (int surface = 0; surface < yalahoo::rhi::renderData::CaseSurfaceCount; ++surface) {
                const auto mesh = meshes[surface];
                const auto center = input.scene.view * entities[i] * QVector4D(mesh.center, 1);
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
        QByteArray instances(entities.size() * renderData::InstanceBytes, '\0');
        int instanceCursor = 0;
        auto instance = [&](int i) {
            auto *destination = instances.data() + instanceCursor++ * renderData::InstanceBytes;
            std::memcpy(destination, entities[i].constData(), renderData::MatrixBytes);
            float uv[VectorScalars] = { 0, 0, 1, 1 };
            if (input.atlasGeneration) {
                const auto tile = effectiveCoverTiles[i];
                copy(uv,
                     QVector4D(float(tile.x()) / input.atlasSize.width(),
                               float(tile.y()) / input.atlasSize.height(),
                               float(tile.width()) / input.atlasSize.width(),
                               float(tile.height()) / input.atlasSize.height()));
                if (!effectiveCoverLeases.isEmpty()
                    && !atlasQueue.ready(tile, effectiveCoverLeases[i]))
                    copy(uv, QVector4D());
            }
            std::memcpy(destination + InstanceUvOffsetBytes, uv, renderData::VectorBytes);
            float light[VectorScalars];
            copy(light, entityLight0[i]);
            std::memcpy(destination + InstanceLightOffsetBytes, light, renderData::VectorBytes);
            copy(light, entityLight1[i]);
            std::memcpy(destination + InstanceSecondaryLightOffsetBytes, light,
                        renderData::VectorBytes);
        };
        for (int i : opaqueOrder)
            if (visible[i])
                instance(i);
        for (int i : opaqueOrder)
            if (!visible[i])
                instance(i);
        updates->updateDynamicBuffer(instanceBuffer.get(), 0, instances.size(),
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
        if (input.shadows) {
            if (batched)
                drawInstances(cb, shadowInstances.get(), shadowBindings.get(), tray,
                              batchGeometryOffset, entities.size());
            else
                for (int i = 0; i < entities.size(); ++i)
                    drawMesh(cb, shadowPipeline.get(), shadowBindings.get(), tray,
                             shadowOffsets[i]);
        }
        cb->endPass();
        shadowEntities = entities;
        cachedShadows = input.shadows;
        shadowDirty = false;
    } else {
        cb->resourceUpdate(updates.release());
    }
    const bool recordCapture = deliver && input.request != capturedRequest && pending.size() < 2;
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
        cb->beginPass(target.rt.get(), Qt::black, { 1, 0 });
        cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
        if (recordCapture)
            opticalEvents.append(u"opaque"_s);
        opaqueMeshDraw(backgroundPipeline.get(), backgroundTriangle, bgOffset);
        if (input.productFloor)
            opaqueMeshDraw(opaquePipeline.get(), productFloor, floorOffset);
        else if (input.shadowDiagnostic)
            opaqueMeshDraw(opaquePipeline.get(), floor, floorOffset);
        if ((!input.shadowDiagnostic || input.shells) && batched) {
            if (input.geometryDiagnostic)
                for (auto mesh : { lid, spine, tray })
                    opaqueInstanceDraw(mesh, batchGeometryOffset);
            if (input.showPaper)
                opaqueInstanceDraw(
                        paper, input.geometryDiagnostic ? batchGeometryOffset : batchPaperOffset);
        } else if (!input.shadowDiagnostic || input.shells)
            for (int i : opaqueOrder) {
                if (!visible[i])
                    continue;
                if (input.geometryDiagnostic)
                    for (auto mesh : { lid, spine, tray })
                        opaqueMeshDraw(opaquePipeline.get(), mesh, opaqueOffsets[i]);
                if (input.showPaper)
                    opaqueMeshDraw(opaquePipeline.get(), paper, opaqueOffsets[i]);
            }
    };
    drawOpaque(opaque);
    cb->endPass();
    if (recordCapture)
        opticalEvents.append(u"opaque-resolved"_s);
    if (input.shells) {
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
            if (batched) {
                Mesh combined{ lid.start, lid.count + spine.count + tray.count };
                drawInstances(cb, coverageInstances.get(), geometryBindings.get(), combined,
                              batchGeometryOffset, visibleCount);
            } else
                for (auto it = shells.crbegin(); it != shells.crend(); ++it)
                    if (visible[it->entity])
                        drawMesh(cb, coveragePipeline.get(), geometryBindings.get(), it->mesh,
                                 it->offset);
        }
        if (recordCapture && !input.geometryDiagnostic)
            opticalEvents.append(u"shell-shading"_s);
        if (!input.geometryDiagnostic)
            for (const auto &draw : shells)
                if (visible[draw.entity])
                    drawMesh(cb, shellPipeline.get(), shellBindings.get(), draw.mesh, draw.offset);
        cb->endPass();
    }
    if (input.postprocess) {
        auto geometry = [&](Target &target) {
            const auto size = target.size;
            cb->beginPass(target.rt.get(), Qt::white, { 1, 0 });
            cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });
            drawMesh(cb, prepassBackgroundPipeline.get(), geometryBindings.get(),
                     backgroundTriangle, bgOffset);
            if (input.productFloor)
                drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), productFloor,
                         floorOffset);
            if (batched) {
                Mesh combined{ lid.start,
                               lid.count + spine.count + tray.count
                                       + (input.showPaper ? paper.count : 0) };
                drawInstances(cb, prepassInstances.get(), geometryBindings.get(), combined,
                              batchGeometryOffset, visibleCount);
            } else
                for (int i : opaqueOrder) {
                    if (!visible[i])
                        continue;
                    for (auto mesh : { lid, spine, tray })
                        drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), mesh,
                                 prepassOffsets[i]);
                    if (input.showPaper)
                        drawMesh(cb, prepassPipeline.get(), geometryBindings.get(), paper,
                                 prepassOffsets[i]);
                }
            cb->endPass();
        };
        if (input.aoSamples)
            geometry(aoGeometry);
        if (dofTargets)
            geometry(dofGeometry);
        post.render(cb);
    }
    cb->beginPass(renderTarget(), Qt::black, { 1, 0 });
    cb->setGraphicsPipeline(outputPipeline.get());
    cb->setViewport({ 0, 0, float(renderTarget()->pixelSize().width()),
                      float(renderTarget()->pixelSize().height()) });
    cb->setShaderResources();
    cb->draw(yalahoo::rhi::renderData::FullscreenTriangleVertices);
    cb->endPass();
    if (deliver && input.request != capturedRequest && pending.size() < 2) {
        capturedRequest = input.request;
        auto p = std::make_shared<Pending>();
        p->drainFrames = api->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1;
        p->capture.request = input.request;
        p->opticalProbe = input.opticalProbe;
        p->captureShadowDepth = input.captureShadowDepth;
        p->deliver = deliver;
        p->failed = failed;
        p->yUp = api->isYUpInFramebuffer();
        p->capture.metadata = {
            { u"backend"_s, int(api->backend()) },
            { u"device"_s, QString::fromUtf8(api->driverInfo().deviceName) },
            { u"qt"_s, QString::fromUtf8(qVersion()) },
            { u"generation"_s, qint64(generation) },
            { u"resourceGeneration"_s, qint64(generation) },
            { u"width"_s, targetSize.width() },
            { u"height"_s, targetSize.height() },
            { u"shadowSize"_s, shadowSize },
            { u"frameRevision"_s, qint64(input.scene.revision) },
            { u"guiThreadAccessInRender"_s, false },
            { u"renderThreadDistinct"_s, distinctThread },
            { u"passes"_s,
              input.shells ? QJsonArray{ u"shadow"_s, u"opaque-color-normal-depth"_s,
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
            { u"shellCull"_s, u"Back"_s }
        };
#ifdef YALAHOO_RHI_AUDIT
        QJsonArray resolveGraph;
        for (const auto &[name, target] :
             { std::pair{ "opaque", &opaque }, std::pair{ "composite", &composite } }) {
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
        p->capture.metadata[u"sampling"_s] =
                QJsonObject{ { u"coverTileLocalMipmaps"_s,
                               input.filterCovers && cover->flags().testFlag(QRhiTexture::MipMapped)
                                       && mipLinear->mipmapMode() == QRhiSampler::Linear },
                             { u"captureTrilinearClamp"_s,
                               mipLinear->magFilter() == QRhiSampler::Linear
                                       && mipLinear->minFilter() == QRhiSampler::Linear
                                       && mipLinear->mipmapMode() == QRhiSampler::Linear
                                       && mipLinear->addressU() == QRhiSampler::ClampToEdge
                                       && mipLinear->addressV() == QRhiSampler::ClampToEdge } };
        p->capture.metadata[u"resolveGraph"_s] = resolveGraph;
        p->capture.metadata[u"postprocessGraph"_s] = input.postprocess
                ? post.graph(input.shells ? composite.color.get() : opaque.color.get())
                : QJsonObject();
        const auto expectedOutput = QRhiShaderResourceBinding::sampledTexture(
                FrameUniformBinding, QRhiShaderResourceBinding::FragmentStage, outputSource,
                linear.get());
        p->capture.metadata[u"outputBindingMatches"_s] = outputSource
                        == (input.postprocess
                                    ? post.output()
                                    : (input.shells ? composite.color.get() : opaque.color.get()))
                && std::find(outputBindings->cbeginBindings(), outputBindings->cendBindings(),
                             expectedOutput)
                        != outputBindings->cendBindings();
#endif
        ResourceUpdates reads(api->nextResourceUpdateBatch());
        if (input.postprocess) {
            p->capture.metadata[u"submittedBokeh"_s] = post.submittedBokeh();
            auto passNames = p->capture.metadata[u"passes"_s].toArray();
            passNames.removeLast();
            for (const auto &name : post.passes())
                passNames.append(name);
            passNames.append(u"ACES-sRGB-output"_s);
            p->capture.metadata[u"passes"_s] = passNames;
        }
        if (input.shells) {
            const auto expectedBinding = QRhiShaderResourceBinding::sampledTexture(
                    CaptureBinding, QRhiShaderResourceBinding::FragmentStage, opaque.color.get(),
                    mipLinear.get());
            bool noBlend = true;
            for (auto blend = shellPipeline->cbeginTargetBlends();
                 blend != shellPipeline->cendTargetBlends(); ++blend)
                noBlend = noBlend && !blend->enable;
            p->capture.metadata[u"opticalGraph"_s] = QJsonObject{
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
        p->capture.metadata[u"atlasGeneration"_s] = qint64(atlasGeneration);
        p->capture.metadata[u"atlasUploadBytes"_s] = frameUploadBytes;
        p->capture.metadata[u"selectedUploadBytes"_s] = selectedUploadBytes;
        p->capture.metadata[u"selectedCoverWidth"_s] = cover->pixelSize().width();
        p->capture.metadata[u"selectedCoverHeight"_s] = cover->pixelSize().height();
        p->capture.metadata[u"selectedTotalUploadBytes"_s] = selectedTotalUploadBytes;
        p->capture.metadata[u"atlasTotalUploadBytes"_s] = totalUploadBytes;
        p->capture.metadata[u"atlasRegions"_s] = uploadRegions;
        if (atlasGeneration)
            p->capture.metadata[u"nativeAtlasTransfers"_s] = nativeTransfers(api, cb, cover.get());
        else
            p->capture.metadata[u"nativeSelectedTransfers"_s] =
                    nativeTransfers(api, cb, cover.get(), false);
        p->capture.metadata[u"atlasPending"_s] = atlasQueue.pending();
        p->capture.metadata[u"atlasRejected"_s] = qint64(atlasQueue.rejected());
        p->capture.metadata[u"drawCount"_s] = entities.size();
        p->capture.metadata[u"geometryInstanced"_s] = batched;
        p->capture.metadata[u"visibleEntities"_s] = visibleCount;
        p->capture.metadata[u"uniformBytes"_s] = int(sizeof(Uniform));
        p->capture.metadata[u"postUniformUploadBytes"_s] =
                input.postprocess ? post.uniformUploadBytes() : 0;
        p->capture.metadata[u"uniformUploadBytes"_s] = qint64(cursor);
        p->capture.metadata[u"uniformCapacityBytes"_s] = qint64(uniforms->size());
        p->capture.metadata[u"coverSnapshotCopies"_s] = qint64(coverSnapshotCopies);
        p->capture.metadata[u"coverSnapshotShares"_s] = qint64(coverSnapshotShares);
        p->capture.metadata[u"coverResourceRevision"_s] = qint64(coverResourceRevision);
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
        p->capture.metadata[u"effectiveCoverRegions"_s] = effectiveRegions;
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
        p->capture.metadata[u"productFloor"_s] = QJsonObject{
            { u"enabled"_s, input.productFloor },
            { u"color"_s, QJsonArray{ receiver.tint[0], receiver.tint[1], receiver.tint[2] } },
            { u"roughness"_s, receiver.material[1] },
            { u"environment"_s, receiver.settings[3] },
            { u"kind"_s, receiver.draw[0] },
            { u"opaqueIndices"_s, input.productFloor ? int(productFloor.count) : 0 },
            { u"aoIndices"_s,
              input.productFloor && input.postprocess && input.aoSamples ? int(productFloor.count)
                                                                         : 0 },
            { u"dofIndices"_s,
              input.productFloor && input.postprocess && input.bokeh ? int(productFloor.count)
                                                                     : 0 },
            { u"castsShadow"_s, false }
        };
        QJsonArray lightMatrix;
        for (float value : receiver.lightVP)
            lightMatrix.append(value);
        p->capture.metadata[u"shadowState"_s] = QJsonObject{
            { u"rendered"_s, shadowRendered },
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
        p->capture.metadata[u"submittedFogUniforms"_s] = fogUniforms;
        p->capture.metadata[u"outputOptions"_s] = QJsonArray{ out[0], out[1], out[2], out[3] };
        p->capture.metadata[u"captureDrawKinds"_s] = captureKinds;
        p->capture.metadata[u"opticalEvents"_s] = opticalEvents;
        p->capture.metadata[u"sortedShells"_s] = sorted;
        p->capture.metadata[u"submittedShellUniforms"_s] = opticalUniforms;
        if (api->backend() == QRhi::OpenGLES2 && readbackPackAlignment == 0) {
            auto *context = QOpenGLContext::currentContext();
            require(context, "No current GL context for readback alignment");
            context->functions()->glGetIntegerv(GL_PACK_ALIGNMENT, &readbackPackAlignment);
            require(readbackPackAlignment == 1 || readbackPackAlignment == 2
                            || readbackPackAlignment == 4 || readbackPackAlignment == 8,
                    "Invalid GL pack alignment");
        }
        p->capture.metadata[u"readbackPackAlignment"_s] = readbackPackAlignment;
        QJsonArray alignedCopies;
        p->readbackOwner.fill(-1);
        for (int i = 0; i < CaptureFieldCount; ++i) {
            if ((i == AtlasField && !input.captureAtlas)
                || (i == DofDepthField && !input.captureFloorDepth))
                continue;
            int owner = i;
            for (int prior = 0; prior < i; ++prior)
                if (p->readbackOwner[prior] >= 0 && textures[prior] == textures[i]) {
                    owner = p->readbackOwner[prior];
                    break;
                }
            p->readbackOwner[i] = owner;
            if (owner != i)
                continue;
            ++p->remaining;
            p->results[i].completed = [weak = std::weak_ptr<Pending>(p), i] {
                if (auto state = weak.lock()) {
                    try {
                        const auto image =
                                readbackImage(state->results[i], state->yUp, i == AtlasField,
                                              state->logicalReadbackSizes[i]);
                        if (i == ShadowField && state->captureShadowDepth) {
                            require(state->results[i].format == QRhiTexture::R32F,
                                    "Shadow math capture must retain R32F precision");
                            state->capture.shadowDepth = state->results[i].data;
                        }
                        if (i == CompositeColorField && state->opticalProbe > 0) {
                            const auto &readback = state->results[i];
                            require(readback.format == QRhiTexture::RGBA16F
                                            || readback.format == QRhiTexture::RGBA32F,
                                    "Optical math capture must retain floating-point RGBA");
                            const int components = readback.format == QRhiTexture::RGBA16F ? 2 : 4;
                            const int x = readback.pixelSize.width() / 2;
                            const int y = readback.pixelSize.height() / 2;
                            const qsizetype offset = (qsizetype(y) * readback.pixelSize.width() + x)
                                    * renderData::FloatBytes * components;
                            QJsonArray values;
                            for (int channel = 0; channel < 3; ++channel) {
                                float value;
                                if (components == 2) {
                                    qfloat16 half;
                                    std::memcpy(&half,
                                                readback.data.constData() + offset + channel * 2,
                                                2);
                                    value = float(half);
                                } else {
                                    std::memcpy(&value,
                                                readback.data.constData() + offset
                                                        + channel * renderData::FloatBytes,
                                                4);
                                }
                                require(std::isfinite(value), "Nonfinite optical math probe");
                                values.append(value);
                            }
                            state->capture.metadata[u"opticalProbeSample"_s] = values;
                        }
                        const std::array<QImage *, CaptureFieldCount> outputs{
                            &state->capture.color,          &state->capture.normal,
                            &state->capture.depth,          &state->capture.shadow,
                            &state->capture.composite,      &state->capture.compositeNormal,
                            &state->capture.compositeDepth, &state->capture.postprocessed,
                            &state->capture.atlas,          &state->capture.rawAo,
                            &state->capture.blurredAo,      &state->capture.aoNormal,
                            &state->capture.aoDepth,        &state->capture.dofDepth
                        };
                        for (int field = 0; field < CaptureFieldCount; ++field)
                            if (state->readbackOwner[field] == i)
                                *outputs[field] = image;
                    } catch (const std::runtime_error &error) {
                        const auto message = u"Readback field %1: %2"_s.arg(i).arg(
                                QString::fromUtf8(error.what()));
                        qWarning("%s", qPrintable(message));
                        state->capture.metadata[u"readbackError"_s] = message;
                        if (state->failed)
                            state->failed(message);
                    }
                    if (--state->remaining == 0)
                        state->deliver(std::move(state->capture));
                }
            };
            auto *readTexture = textures[i];
            const auto logical = readTexture->pixelSize();
            if (api->backend() == QRhi::OpenGLES2 && readTexture->format() == QRhiTexture::R16F
                && logical.width() * 2 % readbackPackAlignment != 0) {
                const int rowBytes = (logical.width() * 2 + readbackPackAlignment - 1)
                        / readbackPackAlignment * readbackPackAlignment;
                const QSize padded(rowBytes / 2, logical.height());
                auto &texture = alignedReadbacks[i];
                if (!texture || texture->pixelSize() != padded) {
                    texture.reset(api->newTexture(QRhiTexture::R16F, padded, 1,
                                                  QRhiTexture::UsedAsTransferSource));
                    require(texture->create(), "Aligned scalar readback texture failed");
                    ++alignedReadbackRevision;
                    cachedInventory = { };
                }
                QRhiTextureCopyDescription copy;
                copy.setPixelSize(logical);
                reads->copyTexture(texture.get(), readTexture, copy);
                readTexture = texture.get();
                p->logicalReadbackSizes[i] = logical;
                alignedCopies.append(QJsonObject{ { u"field"_s, i },
                                                  { u"logicalWidth"_s, logical.width() },
                                                  { u"storageWidth"_s, padded.width() },
                                                  { u"height"_s, logical.height() } });
            }
            reads->readBackTexture(QRhiReadbackDescription(readTexture), &p->results[i]);
        }
        p->capture.metadata[u"alignedScalarCopies"_s] = alignedCopies;
        p->capture.metadata[u"alignedReadbackRevision"_s] = qint64(alignedReadbackRevision);
        p->capture.metadata[u"resources"_s] = resourceInventory();
        p->capture.metadata[u"uniqueReadbackTextures"_s] = p->remaining;
        p->capture.metadata[u"readbackFieldSlots"_s] =
                (input.captureAtlas ? 13 : 12) + int(input.captureFloorDepth);
        pending.push_back(p);
        cb->resourceUpdate(reads.release());
    }
    // Static scenes still need real frames to retire asynchronous backend readbacks.
    // Bound continuation by the backend limit; never resubmit the scene or poll the GUI.
    bool drain = false;
    for (const auto &capture : pending) {
        if (capture->remaining > 0 && capture->drainFrames > 0) {
            --capture->drainFrames;
            drain = true;
        }
    }
    if (drain)
        update();
    ++submissionStats.rendered;
    if (input.preparationToken && prepared)
        prepared(input.preparationToken, synchronizedSubmission, atlasQueue.pending() == 0);
    // A reduced-motion scene may have no subsequent state mutation to drain the upload budget.
    if (atlasQueue.pending() > 0)
        update();
    if (diagnose) {
        const auto gpu = cb->lastCompletedGpuTime();
        diagnose({ std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count(),
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
                { u"submittedNs"_s,
                  qint64(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count()) },
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
                { u"visibleEntities"_s, visibleCount },
                { u"resources"_s, resourceInventory() } });
    }
}
// Count owned resources once by native identity. Byte estimates describe application storage;
// driver padding, compositor memory and GPU completion remain separate unknown facts.
QJsonObject Renderer::resourceInventory()
{
    if (inventoryGeneration == generation && inventoryPostRevision == post.revision()
        && inventoryCoverRevision == coverResourceRevision && !cachedInventory.isEmpty())
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
    for (int i = 0; i < int(alignedReadbacks.size()); ++i)
        inventory.texture(u"capture-aligned-scalar-%1"_s.arg(i), alignedReadbacks[i].get());
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
        { u"entries"_s, inventory.entries() },
        { u"estimatedCurrentBytes"_s, current },
        { u"resizeOldPlusNewUpperBoundBytes"_s, current + previousResourceBytes },
        { u"measuredVramBytes"_s, QJsonValue() },
        { u"scope"_s,
          u"Descriptor payloads; shared depth counted once. Format 0 depth/stencil uses 8-byte upper bound. Excludes driver padding, PSO/SRB storage and Qt window swapchain/compositor (not observable here). Old+new is a conservative replacement overlap, not measured residency. Qt UI attachment estimates and their full old+new plan are separate in preallocationPlan."_s }
    };
    previousResourceBytes = current;
    inventoryGeneration = generation;
    inventoryPostRevision = post.revision();
    inventoryCoverRevision = coverResourceRevision;
    return cachedInventory;
}
}
SceneSubmissionStats &sceneSubmissionStats()
{
    return submissionStats;
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
    for (const auto &tile : input.fallbackCoverTiles)
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
        for (const auto &tile : input.coverTiles)
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
        QSet<quint64> ids;
        for (auto id : input.entityIds) {
            require(!ids.contains(id), "Duplicate shell instance ID");
            ids.insert(id);
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
    return new Renderer;
}
}
