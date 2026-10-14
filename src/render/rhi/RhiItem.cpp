#include "RhiItem.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include "ResourcePlan.h"
#include <QPointer>
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
#include <cstring>
#include <algorithm>
#include <cstdio>
#if defined(Q_OS_WIN)
#  include <rhi/qrhi_platform.h>
#endif

namespace yalahoo::rhi {
namespace {
template <class T>
using Owned = std::unique_ptr<T>;
// Exactly matches frame.glsl; avoid compiler-dependent QVector3D/std140 packing.
struct alignas(16) Uniform
{
    float mvp[16], model[16], view[16], lightMvp[16], lightVP[16];
    float background[4], rowSettings[4], rowColumn[4], lightColumn[4], spotPosition[4],
            spotTarget[4], spotColor[4];
    float keyColor[4], skyColor[4], groundColor[4], lightIntensities[4], settings[4], clip[4],
            draw[4], optics[4];
    float nativeVP[16], camera[4], tint[4], attenuation[4], material[4], coat[4], areaPosition[4],
            areaTarget[4], scatter[4], coverUv[4], paperSize[4], originLight0[4], originLight1[4];
};
static_assert(sizeof(Uniform) == 816 && offsetof(Uniform, background) == 320
              && offsetof(Uniform, optics) == 544 && offsetof(Uniform, nativeVP) == 560
              && offsetof(Uniform, scatter) == 736);
void copy(float *to, const QMatrix4x4 &m)
{
    std::memcpy(to, m.constData(), 64);
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
struct Mesh
{
    quint32 start = 0, count = 0;
    QVector3D center;
};
struct Target
{
    bool mrt = false;
    bool prepass = false;
    QSize size;
    Owned<QRhiTexture> color, normal, depth;
    std::array<Owned<QRhiTexture>, 3> multisample;
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
        depth.reset();
        normal.reset();
        color.reset();
    }
    // Dependencies are destroyed in reverse declaration order: target before attachments.
};
struct Pending
{
    std::array<QRhiReadbackResult, 13> results;
    Capture capture;
    std::function<void(Capture)> deliver;
    int remaining = 13;
    bool yUp = false;
};
QByteArray floatTextureBytes(const char *path, qsizetype expected)
{
    const auto bytes = preparedAssets().bytes.value(QString::fromLatin1(path));
    require(bytes.size() == expected, "Invalid RHI float lookup bytes");
    return bytes;
}
QImage imageOf(const QRhiReadbackResult &r, bool yUp, bool uploaded = false)
{
    if (r.format == QRhiTexture::R16F) {
        QImage image(r.pixelSize, QImage::Format_RGBA8888);
        require(r.data.size() == image.width() * image.height() * 2, "Invalid R16F readback");
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                qfloat16 half;
                const int sourceY = yUp ? image.height() - 1 - y : y;
                std::memcpy(&half, r.data.constData() + (sourceY * image.width() + x) * 2, 2);
                const float value = float(half);
                require(std::isfinite(value), "Nonfinite scalar AO pixel");
                const int gray = std::clamp(int(std::lround(value * 255)), 0, 255);
                // Expand the invariant channels for the same diagnostic PNG contract.
                image.setPixelColor(x, y, QColor(gray, gray, gray, 255));
            }
        return image;
    }
    if (r.format == QRhiTexture::R32F) {
        QImage image(r.pixelSize, QImage::Format_RGBA8888);
        require(r.data.size() == image.width() * image.height() * 4, "Invalid R32F readback");
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                float value;
                const int sourceY = yUp ? image.height() - 1 - y : y;
                std::memcpy(&value, r.data.constData() + (sourceY * image.width() + x) * 4, 4);
                require(std::isfinite(value), "Nonfinite shadow pixel");
                const int gray = std::clamp(int(std::lround(value * 255)), 0, 255);
                image.setPixelColor(x, y, QColor(gray, gray, gray));
            }
        return image;
    }
    if (r.data.size() == r.pixelSize.width() * r.pixelSize.height() * 4) {
        // RGBA8 texture upload/readback retains decoded PNG row order on every backend.
        auto image = QImage(reinterpret_cast<const uchar *>(r.data.constData()),
                            r.pixelSize.width(), r.pixelSize.height(), QImage::Format_RGBA8888)
                             .copy();
        return yUp && !uploaded ? image.flipped(Qt::Vertical) : image;
    }
    const auto componentBytes = r.data.size() / (r.pixelSize.width() * r.pixelSize.height() * 4);
    require(componentBytes == 2 || componentBytes == 4, "Invalid RGBA32F readback");
    QImage image(r.pixelSize, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        const int sourceY = yUp ? image.height() - 1 - y : y;
        auto *dest = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x)
            for (int c = 0; c < 4; ++c) {
                float value;
                const auto *source = r.data.constData()
                        + ((sourceY * image.width() + x) * 4 + c) * componentBytes;
                if (componentBytes == 4)
                    std::memcpy(&value, source, 4);
                else {
                    qfloat16 half;
                    std::memcpy(&half, source, 2);
                    value = float(half);
                }
                if (!std::isfinite(value))
                    std::fprintf(stderr, "Nonfinite RHI pixel (%d,%d,%d), format %d\n", x, y, c,
                                 int(r.format));
                require(std::isfinite(value), "Nonfinite RHI pixel");
                dest[x * 4 + c] = uchar(std::clamp(int(std::lround(value * 255)), 0, 255));
            }
    }
    return image;
}
QVector<bool> visibleEntities(const Input &input, const QVector<QMatrix4x4> &entities)
{
    QVector<bool> visible(entities.size(), true);
    // Conservative homogeneous clip test of the authored complete case bounds.
    // Keep every logical identity and its global sort; skip only provably off-screen draws.
    for (int i = 0; i < entities.size(); ++i) {
        const auto matrix = input.scene.projection * input.scene.view * entities[i];
        std::array<bool, 6> outside{ true, true, true, true, true, true };
        for (float x : { -2.225f, 2.225f })
            for (float y : { .175f, 3.525f })
                for (float z : { -.07f, .07f }) {
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
public:
    ~Renderer() override
    {
        if (api && !pending.empty())
            api->finish();
    }
    void synchronize(QQuickRhiItem *item) override
    {
        QElapsedTimer sync;
        sync.start();
        auto *source = static_cast<RhiItem *>(item);
        if (source->submission() == synchronizedSubmission)
            return;
        synchronizedSubmission = source->submission();
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
        QPointer<RhiItem> guard(source);
        failed = [guard](QString message) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [guard, message] {
                            if (guard)
                                emit guard->renderFailed(message);
                        },
                        Qt::QueuedConnection);
        };
        distinctThread = QThread::currentThread() != source->thread();
        // Keep one owned cover snapshot per revision. The queue owns accepted tile pixels;
        // rejected/replayed uploads need no deep copy while the GUI is blocked in synchronize.
        input.cover = detachedCover;
        if (input.atlasGeneration && atlasGeneration == input.atlasGeneration)
            enqueueAtlas();
        synchronizationMs = sync.nsecsElapsed() / 1e6;
    }
    void initialize(QRhiCommandBuffer *cb) override
    {
        if (failedRenderer)
            return;
        try {
            prepareResources(cb);
        } catch (const std::exception &error) {
            reportFailure(error.what());
        }
    }
    void render(QRhiCommandBuffer *cb) override
    {
        if (failedRenderer)
            return;
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
        std::fprintf(stderr, "RHI rendering failed: %s\n", message);
        failedRenderer = true;
        if (failed)
            failed(QString::fromUtf8(message));
    }
    std::function<void(QString)> failed;
    bool failedRenderer = false;
    qint64 previousResourceBytes = 0;
    quint64 inventoryGeneration = 0, inventoryPostRevision = 0, inventoryCoverRevision = 0;
    quint64 coverResourceRevision = 0;
    QJsonObject cachedInventory, allocationPlan;
    struct AllocationKey
    {
        QSize size, cover;
        int shadow = 0, samples = 0, aoSamples = 0;
        float aoResolution = 0;
        bool shells = false, diagnostic = false, postprocess = false, bokeh = false, smaa = false;
        double navigationDpr = 0;
        quint64 textResourceRevision = 0;
        bool operator==(const AllocationKey &) const = default;
    };
    AllocationKey allocationKey;
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
    Mesh lid, spine, tray, paper, floor, backgroundTriangle;
    QByteArray vertexBytes, indexBytes;
    QSize targetSize;
    int shadowSize = 0, uniformStride = 0, captureSamples = 0;
    bool targetsHaveShells = false;
    bool areaShader = false;
    bool diagnosticTargets = false;
    std::array<Uniform, 7> uniformBases;
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
            for (int y = tile.y(); y < tile.y() + tile.height(); y += 256)
                for (int x = tile.x(); x < tile.x() + tile.width(); x += 256)
                    atlasQueue.expect((y / 256) * (input.atlasSize.width() / 256) + x / 256,
                                      input.coverLeases[i]);
        }
        expectedAtlasGeneration = atlasGeneration;
        for (int i = 0; i < input.fallbackCoverTiles.size(); ++i) {
            const auto tile = input.fallbackCoverTiles[i];
            atlasQueue.expect((tile.y() / 256) * (input.atlasSize.width() / 256) + tile.x() / 256,
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
        const std::array<QRhiTexture *, 3> resolves{ target.color.get(), target.normal.get(),
                                                     target.depth.get() };
        for (int i = 0; i < (mrt ? 3 : 1); ++i) {
            target.multisample[i].reset(api->newTexture(resolves[i]->format(), size, samples,
                                                        QRhiTexture::RenderTarget));
            require(target.multisample[i]->create(), "Cannot create RHI MSAA attachment");
            QRhiColorAttachment attachment(target.multisample[i].get());
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
    QByteArray fragment = isShadow ? ":/rhi/shadow.frag.qsb"
            : target.prepass       ? ":/rhi/prepass.frag.qsb"
                                   : ":/rhi/opaque.frag.qsb";
    if (isShell) {
        fragment = input.areaIntensity > 0 ? ":/rhi/shell" : ":/rhi/shell-basic";
        if (api->backend() != QRhi::OpenGLES2 || api->isFeatureSupported(QRhi::Compute))
            fragment += "-early";
        fragment += ".frag.qsb";
    }
    p->setShaderStages({ { QRhiShaderStage::Vertex,
                           shader(isShadow ? (instanced ? ":/rhi/shadow-instanced.vert.qsb"
                                                        : ":/rhi/shadow.vert.qsb")
                                           : (instanced ? ":/rhi/geometry-instanced.vert.qsb"
                                                        : ":/rhi/geometry.vert.qsb")) },
                         { QRhiShaderStage::Fragment, shader(fragment.constData()) } });
    QRhiVertexInputLayout layout;
    layout.setBindings({ { 32 } });
    layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                           { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
                           { 0, 2, QRhiVertexInputAttribute::Float2, 24 } });
    if (instanced) {
        layout.setBindings({ { 32 }, { 112, QRhiVertexInputBinding::PerInstance } });
        layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                               { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
                               { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
                               { 1, 3, QRhiVertexInputAttribute::Float4, 0 },
                               { 1, 4, QRhiVertexInputAttribute::Float4, 16 },
                               { 1, 5, QRhiVertexInputAttribute::Float4, 32 },
                               { 1, 6, QRhiVertexInputAttribute::Float4, 48 },
                               { 1, 7, QRhiVertexInputAttribute::Float4, 64 },
                               { 1, 8, QRhiVertexInputAttribute::Float4, 80 },
                               { 1, 9, QRhiVertexInputAttribute::Float4, 96 } });
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
                             input.aoResolution,
                             input.shells,
                             bool(deliver),
                             input.postprocess,
                             input.bokeh,
                             input.smaa,
                             input.navigationDpr,
                             input.textResourceRevision };
    if (key != allocationKey || !vertices) {
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
        require(qint64(requestedSize.width()) * requestedSize.height() <= 8294400
                        && qMax(requestedSize.width(), requestedSize.height()) <= maximum
                        && qMax(requestedCover.width(), requestedCover.height()) <= maximum
                        && qMax(1, input.shadowSize) <= maximum,
                "Requested resource plan exceeds GPU dimensions or target pixel protection");
        require(api->supportedSampleCounts().contains(input.captureSamples),
                "Requested resource plan uses unsupported MSAA");
        qint64 geometryBytes = 11 * 32 + 15 * 4;
        for (const auto &batch : preparedAssets().geometry)
            geometryBytes += batch.vertices.size() * 32 + batch.indices.size() * 4;
        allocationPlan = resourcePlan(input, requestedSize, bool(deliver),
                                      api->ubufAligned(sizeof(Uniform)), geometryBytes);
        allocationKey = key;
    }
    if (!vertices) {
        vertexBytes.clear();
        indexBytes.clear();
        auto append = [&](const assets::Batch &batch) {
            const auto base = quint32(vertexBytes.size() / 32);
            Mesh mesh{ quint32(indexBytes.size() / 4), quint32(batch.indices.size()) };
            QVector3D minimum(1e6f, 1e6f, 1e6f), maximum(-1e6f, -1e6f, -1e6f);
            for (const auto &v : batch.vertices)
                for (int axis = 0; axis < 3; ++axis) {
                    minimum[axis] = std::min(minimum[axis], v[axis]);
                    maximum[axis] = std::max(maximum[axis], v[axis]);
                }
            mesh.center = (minimum + maximum) * .5f;
            vertexBytes.append(reinterpret_cast<const char *>(batch.vertices.constData()),
                               batch.vertices.size() * 32);
            for (auto index : batch.indices) {
                index += base;
                indexBytes.append(reinterpret_cast<const char *>(&index), 4);
            }
            return mesh;
        };
        const auto &batches = preparedAssets().geometry;
        lid = append(batches[0]);
        spine = append(batches[1]);
        tray = append(batches[2]);
        assets::Batch quad;
        quad.vertices = { { { -1.36f, .32f, .027f, 0, 0, 1, 0, 1 } },
                          { { 1.7f, .32f, .027f, 0, 0, 1, 1, 1 } },
                          { { 1.7f, 3.38f, .027f, 0, 0, 1, 1, 0 } },
                          { { -1.36f, 3.38f, .027f, 0, 0, 1, 0, 0 } } };
        quad.indices = { 0, 1, 2, 0, 2, 3 };
        paper = append(quad);
        quad.vertices = { { { -6, -.05f, 6, 0, 1, 0, 0, 0 } },
                          { { 6, -.05f, 6, 0, 1, 0, 1, 0 } },
                          { { 6, -.05f, -6, 0, 1, 0, 1, 1 } },
                          { { -6, -.05f, -6, 0, 1, 0, 0, 1 } } };
        floor = append(quad);
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
        uniforms.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                      uniformStride * (436 * 3 + 6)));
        require(uniforms->create(), "RHI uniform buffer failed");
        uniformScratch = QByteArray(uniforms->size(), '\0');
        instanceBuffer.reset(
                api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 436 * 112));
        require(instanceBuffer->create(), "RHI instance buffer failed");
        outputUniform.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
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
        environment.reset(api->newTexture(QRhiTexture::RGBA16F, { 768, 1024 }));
        require(environment->create(), "Room environment texture failed");
        for (auto *t : { &dfg, &ltc1, &ltc2 }) {
            t->reset(api->newTexture(QRhiTexture::RGBA32F,
                                     t == &dfg ? QSize(16, 16) : QSize(64, 64)));
            require((*t)->create(), "Shell lookup texture failed");
        }
    }
    const auto coverSize = input.atlasGeneration ? input.atlasSize : input.cover.size();
    const bool replaceCover =
            !cover || cover->pixelSize() != coverSize || atlasGeneration != input.atlasGeneration;
    const bool replaceTargets = targetSize != renderTarget()->pixelSize()
            || shadowSize != std::max(1, input.shadowSize) || captureSamples != input.captureSamples
            || targetsHaveShells != input.shells || diagnosticTargets != bool(deliver)
            || !opaque.rt;
    if (replaceCover && !replaceTargets) {
        require(std::max(coverSize.width(), coverSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "Cover exceeds GPU dimensions");
        auto previous = std::move(cover);
        cover.reset(api->newTexture(QRhiTexture::RGBA8, coverSize, 1,
                                    QRhiTexture::UsedAsTransferSource));
        require(cover->create(), "Cover texture replacement failed");
        for (auto *bindings : { geometryBindings.get(), shellBindings.get() }) {
            if (!bindings)
                continue;
            QVector<QRhiShaderResourceBinding> values(bindings->cbeginBindings(),
                                                      bindings->cendBindings());
            values[1] = QRhiShaderResourceBinding::sampledTexture(
                    1,
                    QRhiShaderResourceBinding::VertexStage
                            | QRhiShaderResourceBinding::FragmentStage,
                    cover.get(), linear.get());
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
        require(qint64(targetSize.width()) * targetSize.height() <= 8294400,
                "RHI target exceeds RenderQuality pixel limit");
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
                                        QRhiTexture::UsedAsTransferSource));
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
                  QRhiShaderResourceBinding::sampledTexture(1, stages, cover.get(), linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          2, QRhiShaderResourceBinding::FragmentStage, environment.get(),
                          linear.get()),
                  QRhiShaderResourceBinding::sampledTexture(
                          3, QRhiShaderResourceBinding::FragmentStage, shadow.hardwareDepth.get(),
                          shadowCompare.get()) });
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
                      QRhiShaderResourceBinding::sampledTexture(1, stages, cover.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(2, fragment, environment.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              3, fragment, shadow.hardwareDepth.get(), shadowCompare.get()),
                      QRhiShaderResourceBinding::sampledTexture(4, fragment, opaque.color.get(),
                                                                mipLinear.get()),
                      QRhiShaderResourceBinding::sampledTexture(5, fragment, dfg.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(6, fragment, ltc1.get(),
                                                                linear.get()),
                      QRhiShaderResourceBinding::sampledTexture(7, fragment, ltc2.get(),
                                                                linear.get()) });
            require(shellBindings->create(), "Shell bindings failed");
            shellPipeline = pipeline(composite, shellBindings.get(), false, true);
            areaShader = input.areaIntensity > 0;
            // The visibility pass writes the exact source front-surface depth first.
            // Equal/read-only depth allows early rejection before expensive optical shading.
            shellPipeline->setDepthOp(QRhiGraphicsPipeline::Equal);
            shellPipeline->setDepthWrite(false);
            require(shellPipeline->create(), "Shell visible-surface pipeline failed");
            coveragePipeline = pipeline(composite, geometryBindings.get(), false);
            coveragePipeline->setShaderStages(
                    { { QRhiShaderStage::Vertex, shader(":/rhi/geometry.vert.qsb") },
                      { QRhiShaderStage::Fragment, shader(":/rhi/coverage.frag.qsb") } });
            QRhiGraphicsPipeline::TargetBlend noColor;
            noColor.colorWrite = { };
            if (diagnosticTargets)
                coveragePipeline->setTargetBlends({ noColor, noColor, noColor });
            else
                coveragePipeline->setTargetBlends({ noColor });
            require(coveragePipeline->create(), "Shell visibility pipeline failed");
            coverageInstances = pipeline(composite, geometryBindings.get(), false, false, true);
            coverageInstances->setShaderStages(
                    { { QRhiShaderStage::Vertex, shader(":/rhi/geometry-instanced.vert.qsb") },
                      { QRhiShaderStage::Fragment, shader(":/rhi/coverage.frag.qsb") } });
            if (diagnosticTargets)
                coverageInstances->setTargetBlends({ noColor, noColor, noColor });
            else
                coverageInstances->setTargetBlends({ noColor });
            require(coverageInstances->create(), "Instanced visibility pipeline failed");
            copyBindings.reset(api->newShaderResourceBindings());
            copyBindings->setBindings(
                    { QRhiShaderResourceBinding::sampledTexture(0, fragment, opaque.color.get(),
                                                                nearest.get()),
                      QRhiShaderResourceBinding::uniformBuffer(1, fragment, outputUniform.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              2, fragment,
                              diagnosticTargets ? opaque.normal.get() : opaque.color.get(),
                              nearest.get()),
                      QRhiShaderResourceBinding::sampledTexture(
                              3, fragment,
                              diagnosticTargets ? opaque.depth.get() : opaque.color.get(),
                              nearest.get()) });
            require(copyBindings->create(), "Opaque copy bindings failed");
            copyPipeline.reset(api->newGraphicsPipeline());
            copyPipeline->setShaderStages(
                    { { QRhiShaderStage::Vertex, shader(":/rhi/output.vert.qsb") },
                      { QRhiShaderStage::Fragment, shader(":/rhi/copy.frag.qsb") } });
            copyPipeline->setShaderResourceBindings(copyBindings.get());
            copyPipeline->setRenderPassDescriptor(composite.pass.get());
            copyPipeline->setSampleCount(captureSamples);
            if (diagnosticTargets)
                copyPipeline->setTargetBlends({ { }, { }, { } });
            require(copyPipeline->create(), "Opaque copy pipeline failed");
        }
    }
    if (input.shells && areaShader != (input.areaIntensity > 0)) {
        shellPipeline = pipeline(composite, shellBindings.get(), false, true);
        shellPipeline->setDepthOp(QRhiGraphicsPipeline::Equal);
        shellPipeline->setDepthWrite(false);
        require(shellPipeline->create(), "Shell area specialization failed");
        areaShader = input.areaIntensity > 0;
    }
    const QSize aoSize(std::max(1, int(targetSize.width() * input.aoResolution)),
                       std::max(1, int(targetSize.height() * input.aoResolution)));
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
        settings.resolution = input.aoResolution;
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
            { QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                        resolvedColor, linear.get()),
              QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage,
                                                       outputUniform.get()) });
    require(outputBindings->create(), "Output bindings failed");
    outputPipeline.reset(api->newGraphicsPipeline());
    outputPipeline->setShaderStages(
            { { QRhiShaderStage::Vertex, shader(":/rhi/output.vert.qsb") },
              { QRhiShaderStage::Fragment, shader(":/rhi/output.frag.qsb") } });
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
    copy(u.draw, QVector4D(float(kind), input.shadows ? 1 : 0, float(shadowSize), 4));
    copy(u.optics, QVector4D(input.spotIntensity, .32f, .035f, -.0003f));
    copy(u.nativeVP, nativeVP);
    copy(u.camera, input.scene.camera.position.qt(), input.exposure);
    const auto material = shellMaterial(std::clamp(kind - 3, 0, 2), input.warmth, input.clarity,
                                        input.environmentIntensity);
    copy(u.tint, material.tint);
    copy(u.attenuation, material.attenuation);
    // Fixed R07 shared-theme calibration; no per-scene fitting.
    copy(u.material,
         QVector4D(material.transport.x(), std::clamp(material.transport.y() * 1.01f, 0.0f, 1.0f),
                   material.transport.z() * .91809f, material.transport.w() * 1.0201f));
    auto coat = material.coat;
    coat.setW(coat.w() * 1.01f);
    copy(u.coat, coat);
    copy(u.areaPosition, input.areaPosition, input.areaIntensity);
    copy(u.areaTarget, input.areaTarget, input.lightingEnabled ? 1 : 0);
    copy(u.scatter, input.scatterColor, input.scatterStrength);
    copy(u.coverUv, QVector4D(0, 0, 1, 1));
    const QSize pixels = input.atlasGeneration ? QSize(256, 256) : input.cover.size();
    copy(u.paperSize,
         QVector4D(float(pixels.width()), float(pixels.height()), input.coverPainted ? 1 : 0, 0));
    return u;
}
Uniform Renderer::uniform(const QMatrix4x4 &model, int kind, int entity) const
{
    auto u = uniformBases[kind + 1];
    copy(u.mvp, nativeVP * model);
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
    cpu.start();
    // initialize may precede synchronize in Qt; defer resource-size changes to a fresh frame.
    if (!api || (input.cover.isNull() && !input.atlasGeneration))
        return;
    prepareResources(cb);
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [](const auto &p) {
                                     return p->remaining == 0;
                                 }),
                  pending.end());
    auto *updates = api->nextResourceUpdateBatch();
    if (uploadGeometry) {
        updates->uploadStaticBuffer(vertices.get(), vertexBytes);
        updates->uploadStaticBuffer(indices.get(), indexBytes);
        const auto roomHalf =
                floatTextureBytes(":/rhi/environment/room-pmrem.rgba16f", 768 * 1024 * 8);
        updates->uploadTexture(environment.get(),
                               QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(roomHalf)) }));
        const std::array<QRhiTexture *, 3> lookup{ dfg.get(), ltc1.get(), ltc2.get() };
        const std::array<const char *, 3> names{ ":/rhi/environment/dfg.rgba32f",
                                                 ":/rhi/environment/ltc1.rgba32f",
                                                 ":/rhi/environment/ltc2.rgba32f" };
        for (int i = 0; i < 3; ++i) {
            const auto bytes = floatTextureBytes(names[i],
                                                 lookup[i]->pixelSize().width()
                                                         * lookup[i]->pixelSize().height() * 16);
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
            const QPoint destination((u.slot % (input.atlasSize.width() / 256)) * 256,
                                     (u.slot / (input.atlasSize.width() / 256)) * 256);
            QRhiTextureSubresourceUploadDescription region(u.pixels);
            region.setSourceSize({ 256, 256 });
            region.setDestinationTopLeft(destination);
            updates->uploadTexture(cover.get(), { { 0, 0, region } });
            frameUploadBytes += AtlasUploadQueue::tileBytes;
            uploadRegions.append(QJsonObject{ { "slot", u.slot },
                                              { "x", destination.x() },
                                              { "y", destination.y() },
                                              { "width", 256 },
                                              { "height", 256 },
                                              { "bytes", AtlasUploadQueue::tileBytes },
                                              { "lease", qint64(u.lease) },
                                              { "serial", qint64(u.serial) } });
        }
    } else if (coverDirty) {
        // Filter premultiplied texels, then unpremultiply in the paper shader. Straight
        // transparent black texels otherwise produce a dark line at the alpha-test edge.
        const auto image = input.cover.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        updates->uploadTexture(cover.get(),
                               { { 0, 0, QRhiTextureSubresourceUploadDescription(image) } });
        coverDirty = false;
        selectedUploadBytes = image.sizeInBytes();
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
    require(entities.size() <= 436, "RHI visible pool capacity exceeded");
    const bool batched = entities.size() >= 8;
    nativeVP = api->clipSpaceCorrMatrix() * input.scene.projection * input.scene.view;
    canonicalLightVP = lightProjection() * lightView();
    nativeLightVP = api->clipSpaceCorrMatrix() * canonicalLightVP;
    for (int kind = -1; kind <= 5; ++kind)
        uniformBases[kind + 1] = uniformTemplate(kind);
    const QVector4D printSettings(input.printAmbient, input.rowDetail, input.rowFill, 0);
    if (lightEntities != entities || cachedRowSettings != input.rowSettings
        || cachedPrintSettings != printSettings || cachedRowColumn != input.rowColumn
        || cachedLightColumn != input.lightColumn) {
        entityLight0.clear();
        entityLight1.clear();
        for (const auto &model : entities) {
            const auto origin = model.column(3).toVector3D();
            const float row = (origin.z() - input.rowColumn.z() - input.rowSettings.w() * .62f)
                    / (.5f * input.rowSettings.z() * .62f);
            const float lane = (origin.x() - input.rowColumn.x()) / 27;
            const float lane2 = lane * lane;
            const float field =
                    std::exp(-.6931471805599453f * row * row) * std::exp(-lane2 * lane2 * lane2);
            const auto distance = (origin - input.lightColumn) / QVector3D(6, 3.35f, 5.5f);
            const float printLight = std::exp(-QVector3D::dotProduct(distance, distance));
            const auto mix = [](float a, float b, float t) {
                return a * (1 - t) + b * t;
            };
            const float baseline = mix(input.printAmbient, 1, printLight);
            const float ambient = std::max(.08f, input.printAmbient * .5f);
            const float fill = input.rowSettings.x() > 1.5f && input.rowSettings.x() < 2.5f
                    ? input.rowFill
                    : 0;
            const float fieldExposure = mix(mix(ambient, baseline, fill), 1,
                                            std::clamp(input.rowSettings.y() * field, 0.0f, 1.0f));
            const float shelf = input.rowSettings.x() < 1.5f
                    ? mix(ambient, input.printAmbient, .25f)
                    : fieldExposure;
            const float printExposure =
                    input.rowSettings.x() < .5f ? baseline : mix(shelf, baseline, input.rowDetail);
            const float laneDistance = std::abs(origin.x() - input.lightColumn.x());
            const float core = std::exp(-std::pow(laneDistance / 3.2f, 4.0f));
            const float spill = std::exp(-std::pow(laneDistance / 7.2f, 4.0f));
            const float z = origin.z() - input.lightColumn.z();
            const float rowLight = std::exp(-std::pow(z / 5.5f, 2.0f));
            const float hot = std::exp(-std::pow(z / 1.1f, 2.0f));
            const float neighbor = std::exp(-std::pow(z / 2.4f, 2.0f));
            entityLight0.append({ field, printExposure, .58f * core + .42f * spill,
                                  .58f * core * mix(.12f, 1, rowLight) + .42f * spill * neighbor });
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
    if (!batched)
        for (const auto &m : entities)
            shadowOffsets.append(append(m, 1));
    const auto bgOffset = append({ }, -1);
    if (!batched)
        for (int i = 0; i < entities.size(); ++i)
            opaqueOffsets.append(append(entities[i], input.geometryDiagnostic ? 1 : 0, i));
    const auto floorOffset = append({ }, 2);
    struct ShellDraw
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
        shells.reserve(entities.size() * 3);
        require(entities.size() == 1 || input.entityIds.size() == entities.size(),
                "Shell instances require stable IDs");
        for (int i : opaqueOrder) {
            const std::array<Mesh, 3> meshes{ lid, spine, tray };
            const auto id = input.entityIds.isEmpty() ? 0 : input.entityIds[i];
            for (int surface = 0; surface < 3; ++surface) {
                const auto mesh = meshes[surface];
                const auto center = input.scene.view * entities[i] * QVector4D(mesh.center, 1);
                shells.append({ mesh, visible[i] ? append(entities[i], 3 + surface, i) : 0,
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
        QByteArray instances(entities.size() * 112, '\0');
        int instanceCursor = 0;
        auto instance = [&](int i) {
            auto *destination = instances.data() + instanceCursor++ * 112;
            std::memcpy(destination, entities[i].constData(), 64);
            float uv[4] = { 0, 0, 1, 1 };
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
            std::memcpy(destination + 64, uv, 16);
            float light[4];
            copy(light, entityLight0[i]);
            std::memcpy(destination + 80, light, 16);
            copy(light, entityLight1[i]);
            std::memcpy(destination + 96, light, 16);
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
    const float out[4] = {
        api->isYUpInFramebuffer() ? 0.0f : 1.0f,
        input.geometryDiagnostic || input.shadowDiagnostic || input.rawOutput ? 1.0f : 0.0f,
        (input.postprocess || input.shells) && input.lightingEnabled && !input.rawOutput
                        && !input.geometryDiagnostic && !input.shadowDiagnostic
                ? 1.0f
                : 0.0f,
        input.exposure
    };
    updates->updateDynamicBuffer(outputUniform.get(), 0, 16, out);
    shadowDirty = shadowDirty || shadowEntities != entities || cachedShadows != input.shadows;
    if (shadowDirty) {
        cb->beginPass(shadow.rt.get(), Qt::white, { 1, 0 }, updates);
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
        cb->resourceUpdate(updates);
    }
    auto drawOpaque = [&](Target &target) {
        cb->beginPass(target.rt.get(), Qt::black, { 1, 0 });
        cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
        drawMesh(cb, backgroundPipeline.get(), geometryBindings.get(), backgroundTriangle,
                 bgOffset);
        if (input.shadowDiagnostic)
            drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), floor, floorOffset);
        if ((!input.shadowDiagnostic || input.shells) && batched) {
            if (input.geometryDiagnostic)
                for (auto mesh : { lid, spine, tray })
                    drawInstances(cb, opaqueInstances.get(), geometryBindings.get(), mesh,
                                  batchGeometryOffset, visibleCount);
            if (input.showPaper)
                drawInstances(cb, opaqueInstances.get(), geometryBindings.get(), paper,
                              input.geometryDiagnostic ? batchGeometryOffset : batchPaperOffset,
                              visibleCount);
        } else if (!input.shadowDiagnostic || input.shells)
            for (int i : opaqueOrder) {
                if (!visible[i])
                    continue;
                if (input.geometryDiagnostic)
                    for (auto mesh : { lid, spine, tray })
                        drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), mesh,
                                 opaqueOffsets[i]);
                if (input.showPaper)
                    drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), paper,
                             opaqueOffsets[i]);
            }
    };
    drawOpaque(opaque);
    cb->endPass();
    if (input.shells) {
        // Resolve ends before generateMips; capture never contains transmission shells.
        auto *mips = api->nextResourceUpdateBatch();
        mips->generateMips(opaque.color.get());
        cb->resourceUpdate(mips);
        // Load retained opaque depth and copy resolved color into the independent destination.
        // Both MRT layouts/sample counts are compatible; no sampled attachment aliases this target.
        cb->beginPass(composite.rt.get(), Qt::black, { 1, 0 });
        cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
        cb->setGraphicsPipeline(copyPipeline.get());
        cb->setShaderResources(copyBindings.get());
        cb->draw(3);
        if (!input.geometryDiagnostic) {
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
    cb->draw(3);
    cb->endPass();
    if (deliver && input.request != capturedRequest && pending.size() < 2) {
        capturedRequest = input.request;
        auto p = std::make_shared<Pending>();
        p->capture.request = input.request;
        p->deliver = deliver;
        p->yUp = api->isYUpInFramebuffer();
        p->capture.metadata = {
            { "backend", int(api->backend()) },
            { "device", QString::fromUtf8(api->driverInfo().deviceName) },
            { "qt", qVersion() },
            { "generation", qint64(generation) },
            { "resourceGeneration", qint64(generation) },
            { "width", targetSize.width() },
            { "height", targetSize.height() },
            { "shadowSize", shadowSize },
            { "frameRevision", qint64(input.scene.revision) },
            { "guiThreadAccessInRender", false },
            { "renderThreadDistinct", distinctThread },
            { "passes",
              input.shells ? QJsonArray{ "shadow", "opaque-color-normal-depth",
                                         "capture-resolve-mips", "sorted-shell", "review-output" }
                           : QJsonArray{ "shadow", "opaque-color-normal-depth", "review-output" } },
            { "msaaSamples", captureSamples },
            { "recursiveGlassCapture", false },
            { "shellDepthWrite", true },
            { "shellColorDepthWrite", false },
            { "shellDepthVisibility",
              "exact front-surface depth, Equal test before optical shading" },
            { "shellBlend", false },
            { "shellCull", "Back" }
        };
        auto *reads = api->nextResourceUpdateBatch();
        if (input.postprocess) {
            auto passNames = p->capture.metadata["passes"].toArray();
            passNames.removeLast();
            for (const auto &name : post.passes())
                passNames.append(name);
            passNames.append("ACES-sRGB-output");
            p->capture.metadata["passes"] = passNames;
        }
        p->capture.metadata["atlasGeneration"] = qint64(atlasGeneration);
        p->capture.metadata["atlasUploadBytes"] = frameUploadBytes;
        p->capture.metadata["selectedUploadBytes"] = selectedUploadBytes;
        p->capture.metadata["selectedTotalUploadBytes"] = selectedTotalUploadBytes;
        p->capture.metadata["atlasTotalUploadBytes"] = totalUploadBytes;
        p->capture.metadata["atlasRegions"] = uploadRegions;
        if (atlasGeneration)
            p->capture.metadata["nativeAtlasTransfers"] = nativeTransfers(api, cb, cover.get());
        else
            p->capture.metadata["nativeSelectedTransfers"] =
                    nativeTransfers(api, cb, cover.get(), false);
        p->capture.metadata["atlasPending"] = atlasQueue.pending();
        p->capture.metadata["atlasRejected"] = qint64(atlasQueue.rejected());
        p->capture.metadata["drawCount"] = entities.size();
        p->capture.metadata["geometryInstanced"] = batched;
        p->capture.metadata["visibleEntities"] = visibleCount;
        p->capture.metadata["uniformBytes"] = int(sizeof(Uniform));
        p->capture.metadata["uniformUploadBytes"] = qint64(cursor);
        p->capture.metadata["uniformCapacityBytes"] = qint64(uniforms->size());
        p->capture.metadata["coverSnapshotCopies"] = qint64(coverSnapshotCopies);
        p->capture.metadata["coverSnapshotShares"] = qint64(coverSnapshotShares);
        p->capture.metadata["coverResourceRevision"] = qint64(coverResourceRevision);
        const std::array<QRhiTexture *, 13> textures{
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
            input.postprocess && input.aoSamples ? aoGeometry.depth.get() : opaque.depth.get()
        };
        QJsonArray sorted;
        for (const auto &draw : shells)
            sorted.append(QJsonObject{ { "id", qint64(draw.identity) },
                                       { "surface", draw.surface },
                                       { "depth", draw.depth } });
        p->capture.metadata["sortedShells"] = sorted;
        for (int i = 0; i < 13; ++i) {
            if (i == 8 && !input.captureAtlas) {
                --p->remaining;
                continue;
            }
            p->results[i].completed = [weak = std::weak_ptr<Pending>(p), i] {
                if (auto state = weak.lock()) {
                    auto img = imageOf(state->results[i], state->yUp, i == 8);
                    if (i == 0)
                        state->capture.color = img;
                    else if (i == 1)
                        state->capture.normal = img;
                    else if (i == 2)
                        state->capture.depth = img;
                    else if (i == 3)
                        state->capture.shadow = img;
                    else if (i == 4)
                        state->capture.composite = img;
                    else if (i == 5)
                        state->capture.compositeNormal = img;
                    else if (i == 6)
                        state->capture.compositeDepth = img;
                    else if (i == 7)
                        state->capture.postprocessed = img;
                    else if (i == 8)
                        state->capture.atlas = img;
                    else if (i == 9)
                        state->capture.rawAo = img;
                    else if (i == 10)
                        state->capture.blurredAo = img;
                    else if (i == 11)
                        state->capture.aoNormal = img;
                    else
                        state->capture.aoDepth = img;
                    if (--state->remaining == 0)
                        state->deliver(std::move(state->capture));
                }
            };
            reads->readBackTexture(QRhiReadbackDescription(textures[i]), &p->results[i]);
        }
        pending.push_back(p);
        cb->resourceUpdate(reads);
        update();
    }
    if (input.profile && profile) {
        const auto gpu = cb->lastCompletedGpuTime();
        profile(QJsonObject{
                { "cpuSubmitMs", cpu.nsecsElapsed() / 1e6 },
                { "synchronizationMs", synchronizationMs },
                { "frameRevision", qint64(input.scene.revision) },
                { "submission", qint64(synchronizedSubmission) },
                { "aoSamples", input.aoSamples },
                { "bokeh", input.bokeh },
                { "smaa", input.smaa },
                { "logicalWidth", input.scene.width },
                { "logicalHeight", input.scene.height },
                { "targetWidth", targetSize.width() },
                { "targetHeight", targetSize.height() },
                { "inputRevision", qint64(input.scene.inputRevision) },
                { "inputReceivedNs", input.scene.inputReceivedNs },
                { "navigationRevision", qint64(input.scene.navigationRevision) },
                { "navigationInputRevision", qint64(input.scene.navigationInputRevision) },
                { "navigationCommittedNs", input.scene.navigationCommittedNs },
                { "submittedNs",
                  qint64(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count()) },
                { "coverSnapshotCopies", qint64(coverSnapshotCopies) },
                { "coverSnapshotShares", qint64(coverSnapshotShares) },
                { "postprocessRevision", qint64(post.revision()) },
                { "gpuFrameMs", gpu > 0 ? QJsonValue(gpu * 1000) : QJsonValue() },
                { "atlasUploadBytes", frameUploadBytes },
                { "selectedUploadBytes", selectedUploadBytes },
                { "selectedCoverWidth", input.cover.width() },
                { "selectedCoverHeight", input.cover.height() },
                { "uniformUploadBytes", qint64(cursor) },
                { "resourceGeneration", qint64(generation) },
                { "visibleEntities", visibleCount },
                { "resources", resourceInventory() } });
    }
}
QJsonObject Renderer::resourceInventory()
{
    if (inventoryGeneration == generation && inventoryPostRevision == post.revision()
        && inventoryCoverRevision == coverResourceRevision && !cachedInventory.isEmpty())
        return cachedInventory;
    ResourceInventory inventory;
    auto target = [&](const QString &name, const Target &t) {
        inventory.texture(name + "-color-resolve", t.color.get());
        inventory.texture(name + "-normal", t.normal.get());
        inventory.texture(name + "-linear-depth", t.depth.get());
        inventory.texture(name + "-hardware-depth", t.hardwareDepth.get());
        inventory.renderBuffer(name + "-depth-stencil", t.z.get());
        for (int i = 0; i < 3; ++i)
            inventory.texture(name + QString("-msaa-%1").arg(i), t.multisample[i].get());
    };
    target("shadow", shadow);
    target("opaque", opaque);
    target("shell-composite", composite);
    target("ao-geometry", aoGeometry);
    target("dof-geometry", dofGeometry);
    inventory.texture("cover-or-atlas", cover.get());
    inventory.texture("environment", environment.get());
    inventory.texture("dfg", dfg.get());
    inventory.texture("ltc1", ltc1.get());
    inventory.texture("ltc2", ltc2.get());
    inventory.buffer("geometry-vertices", vertices.get());
    inventory.buffer("geometry-indices", indices.get());
    inventory.buffer("bounded-draw-uniforms", uniforms.get());
    inventory.buffer("bounded-instances", instanceBuffer.get());
    inventory.buffer("output-uniform", outputUniform.get());
    post.inventory(inventory);
    inventory.texture("qt-item-color", colorTexture());
    inventory.texture("qt-item-resolve", resolveTexture());
    inventory.renderBuffer("qt-item-msaa", msaaColorBuffer());
    inventory.renderBuffer("qt-item-depth-stencil", depthStencilBuffer());
    const qint64 current = inventory.bytes();
    cachedInventory = {
        { "preallocationPlan", allocationPlan },
        { "entries", inventory.entries() },
        { "estimatedCurrentBytes", current },
        { "resizeOldPlusNewUpperBoundBytes", current + previousResourceBytes },
        { "measuredVramBytes", QJsonValue() },
        { "scope",
          "Descriptor payloads; shared depth counted once. Format 0 depth/stencil uses 8-byte upper bound. Excludes driver padding, PSO/SRB storage and Qt window swapchain/compositor (not observable here). Old+new is a conservative replacement overlap, not measured residency." }
    };
    previousResourceBytes = current;
    inventoryGeneration = generation;
    inventoryPostRevision = post.revision();
    inventoryCoverRevision = coverResourceRevision;
    return cachedInventory;
}

}
RhiItem::RhiItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    preparedAssets();
    setAlphaBlending(false);
}
QJsonArray adapterInventory()
{
    QJsonArray result;
#if defined(Q_OS_WIN)
    QRhiD3D11InitParams params;
    const auto adapters = QRhi::enumerateAdapters(QRhi::D3D11, &params);
    int index = 0;
    for (const auto &adapter : adapters) {
        const auto info = adapter->info();
        result.append(QJsonObject{ { "index", index++ },
                                   { "name", QString::fromUtf8(info.deviceName) },
                                   { "deviceId", qint64(info.deviceId) },
                                   { "vendorId", qint64(info.vendorId) },
                                   { "deviceType", int(info.deviceType) } });
    }
#endif
    return result;
}
void RhiItem::submit(Input input, QSize target)
{
    require(input.fallbackCoverTiles.isEmpty()
                    || (input.fallbackCoverTiles.size() == input.entities.size()
                        && input.fallbackCoverLeases.size() == input.entities.size()),
            "Atlas fallback/entity mismatch");
    for (const auto &tile : input.fallbackCoverTiles)
        require(QRect(QPoint(), input.atlasSize).contains(tile) && tile.size() == QSize(256, 256)
                        && tile.x() % 256 == 0 && tile.y() % 256 == 0,
                "Atlas fallback outside page");
    require(input.entities.size() <= 436, "RHI visible pool capacity exceeded");
    require(!input.cover.isNull() || input.atlasGeneration,
            "Missing cover at RHI synchronization boundary");
    require(input.aoSamples == 0 || input.aoSamples == 16 || input.aoSamples == 32
                    || input.aoSamples == 64,
            "Invalid RHI AO kernel size");
    require(input.aoResolution == .5f || input.aoResolution == .75f || input.aoResolution == 1,
            "Invalid RHI AO resolution");
    if (input.atlasGeneration) {
        require(m_input.atlasGeneration != input.atlasGeneration
                        || m_input.atlasSize == input.atlasSize,
                "Changing atlas dimensions requires a new generation");
        require(input.coverTiles.size() == input.entities.size(), "Atlas tile/entity mismatch");
        require(input.coverLeases.isEmpty() || input.coverLeases.size() == input.entities.size(),
                "Atlas lease/entity mismatch");
        require(input.atlasSize.width() > 0 && input.atlasSize.height() > 0
                        && input.atlasSize.width() % 256 == 0
                        && input.atlasSize.height() % 256 == 0,
                "Atlas size must use whole tiles");
        for (const auto &tile : input.coverTiles)
            require(QRect(QPoint(), input.atlasSize).contains(tile)
                            && (tile.size() == QSize(256, 256) || tile.size() == QSize(1024, 1024))
                            && tile.x() % 256 == 0 && tile.y() % 256 == 0,
                    "Atlas tile outside page");
    }
    require(target.width() > 0 && target.height() > 0, "Invalid RHI target size");
    require(input.captureSamples > 0, "Invalid RHI sample count");
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
