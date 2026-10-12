#include "RhiItem.h"
#include "RhiApi.h"
#include "ShellMaterial.h"
#include "GeometryAudit.h"
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <QSet>
#include <QtEndian>
#include <cstring>
#include <algorithm>

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
            areaTarget[4], scatter[4];
};
static_assert(sizeof(Uniform) == 752 && offsetof(Uniform, background) == 320
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
    Owned<QRhiTexture> color, normal, depth;
    std::array<Owned<QRhiTexture>, 3> multisample;
    Owned<QRhiRenderBuffer> z;
    Owned<QRhiRenderPassDescriptor> pass;
    Owned<QRhiTextureRenderTarget> rt;
    void reset()
    {
        rt.reset();
        pass.reset();
        z.reset();
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
    std::array<QRhiReadbackResult, 7> results;
    Capture capture;
    std::function<void(Capture)> deliver;
    int remaining = 7;
    bool yUp = false;
};
QByteArray floatTextureBytes(const char *path, qsizetype expected)
{
    auto bytes = assets::readFile(QString::fromLatin1(path));
    require(bytes.size() == expected, "Invalid RHI float lookup bytes");
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    for (qsizetype i = 0; i < bytes.size(); i += 4) {
        const auto word = qFromLittleEndian<quint32>(bytes.constData() + i);
        std::memcpy(bytes.data() + i, &word, 4);
    }
#endif
    return bytes;
}
QImage imageOf(const QRhiReadbackResult &r, bool yUp)
{
    require(r.data.size() == r.pixelSize.width() * r.pixelSize.height() * 16,
            "Invalid RGBA32F readback");
    QImage image(r.pixelSize, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        const int sourceY = yUp ? image.height() - 1 - y : y;
        auto *dest = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x)
            for (int c = 0; c < 4; ++c) {
                float value;
                std::memcpy(&value,
                            r.data.constData() + ((sourceY * image.width() + x) * 4 + c) * 4, 4);
                require(std::isfinite(value), "Nonfinite RHI pixel");
                dest[x * 4 + c] = uchar(std::clamp(int(std::lround(value * 255)), 0, 255));
            }
    }
    return image;
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
        auto *source = static_cast<RhiItem *>(item);
        coverDirty = coverDirty || coverSourceKey != source->input().cover.cacheKey();
        coverSourceKey = source->input().cover.cacheKey();
        input = source->input();
        deliver = source->captured;
        distinctThread = QThread::currentThread() != source->thread();
        // No GUI pointer retained; image pixels detach before rendering or resource updates.
        input.cover = input.cover.copy();
    }
    void initialize(QRhiCommandBuffer *) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    void reset();
    void makeTarget(Target &target, QSize size, bool mrt, int samples = 1, bool mips = false);
    Owned<QRhiGraphicsPipeline> pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                         bool shadow, bool shell = false);
    Uniform uniform(const QMatrix4x4 &model, int kind) const;
    void drawMesh(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *pipeline,
                  QRhiShaderResourceBindings *bindings, Mesh mesh, quint32 offset);
    QRhi *api = nullptr;
    Input input;
    std::function<void(Capture)> deliver;
    Target shadow, opaque, composite;
    Owned<QRhiBuffer> vertices, indices, uniforms, outputUniform;
    Owned<QRhiTexture> cover, environment, dfg, ltc1, ltc2;
    Owned<QRhiSampler> linear, nearest, mipLinear;
    Owned<QRhiShaderResourceBindings> geometryBindings, shadowBindings, outputBindings;
    Owned<QRhiGraphicsPipeline> shadowPipeline, opaquePipeline, backgroundPipeline, outputPipeline;
    Owned<QRhiShaderResourceBindings> shellBindings;
    Owned<QRhiGraphicsPipeline> shellPipeline;
    Mesh lid, spine, tray, paper, floor, backgroundTriangle;
    QByteArray vertexBytes, indexBytes;
    QSize targetSize;
    int shadowSize = 0, uniformStride = 0, captureSamples = 0;
    bool targetsHaveShells = false;
    bool uploadGeometry = true;
    bool coverDirty = true;
    bool distinctThread = false;
    quint64 capturedRequest = 0, generation = 0;
    qint64 coverSourceKey = 0;
    std::vector<std::shared_ptr<Pending>> pending;
};
void Renderer::reset()
{
    shellPipeline.reset();
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
    uniforms.reset();
    outputUniform.reset();
    vertices.reset();
    indices.reset();
    cover.reset();
    environment.reset();
    dfg.reset();
    ltc1.reset();
    ltc2.reset();
    mipLinear.reset();
    linear.reset();
    nearest.reset();
    targetSize = { };
    shadowSize = 0;
    uploadGeometry = true;
    coverDirty = true;
}
void Renderer::makeTarget(Target &target, QSize size, bool mrt, int samples, bool mips)
{
    target.reset();
    auto texture = [&](bool mipmapped = false) {
        Owned<QRhiTexture> t(api->newTexture(
                QRhiTexture::RGBA32F, size, 1,
                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource
                        | (mipmapped ? QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips
                                     : QRhiTexture::Flags())));
        require(t->create(), "Cannot create RHI float target");
        return t;
    };
    target.color = texture(mips);
    target.z.reset(api->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, samples));
    require(target.z->create(), "Cannot create RHI depth attachment");
    QRhiTextureRenderTargetDescription desc;
    if (mrt) {
        target.normal = texture();
        target.depth = texture();
        desc.setColorAttachments({ target.color.get(), target.normal.get(), target.depth.get() });
    } else
        desc.setColorAttachments({ target.color.get() });
    if (samples > 1) {
        QVector<QRhiColorAttachment> attachments;
        const std::array<QRhiTexture *, 3> resolves{ target.color.get(), target.normal.get(),
                                                     target.depth.get() };
        for (int i = 0; i < (mrt ? 3 : 1); ++i) {
            target.multisample[i].reset(api->newTexture(QRhiTexture::RGBA32F, size, samples,
                                                        QRhiTexture::RenderTarget));
            require(target.multisample[i]->create(), "Cannot create RHI MSAA attachment");
            QRhiColorAttachment attachment(target.multisample[i].get());
            attachment.setResolveTexture(resolves[i]);
            attachments.append(attachment);
        }
        desc.setColorAttachments(attachments.cbegin(), attachments.cend());
    }
    desc.setDepthStencilBuffer(target.z.get());
    target.rt.reset(api->newTextureRenderTarget(desc));
    target.pass.reset(target.rt->newCompatibleRenderPassDescriptor());
    target.rt->setRenderPassDescriptor(target.pass.get());
    require(target.rt->create(), "Cannot create RHI render target");
}
Owned<QRhiGraphicsPipeline> Renderer::pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                               bool isShadow, bool isShell)
{
    Owned<QRhiGraphicsPipeline> p(api->newGraphicsPipeline());
    p->setShaderStages({ { QRhiShaderStage::Vertex,
                           shader(isShadow ? ":/rhi/shadow.vert.qsb" : ":/rhi/geometry.vert.qsb") },
                         { QRhiShaderStage::Fragment,
                           shader(isShadow          ? ":/rhi/shadow.frag.qsb"
                                          : isShell ? ":/rhi/shell.frag.qsb"
                                                    : ":/rhi/opaque.frag.qsb") } });
    QRhiVertexInputLayout layout;
    layout.setBindings({ { 32 } });
    layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                           { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
                           { 0, 2, QRhiVertexInputAttribute::Float2, 24 } });
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
    if (!isShadow)
        p->setTargetBlends({ { }, { }, { } });
    require(p->create(), "RHI graphics pipeline creation failed");
    return p;
}
void Renderer::initialize(QRhiCommandBuffer *)
{
    if (api != rhi()) {
        if (api && !pending.empty())
            api->finish();
        pending.clear();
        reset();
        api = rhi();
    }
    if (input.cover.isNull())
        return;
    require(api->isTextureFormatSupported(QRhiTexture::RGBA32F, QRhiTexture::RenderTarget),
            "Selected GPU lacks required RGBA32F targets");
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
        const auto batches = assets::readGlb(assets::readFile(":/rhi/models/music-case-v040.glb"));
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
                                      uniformStride * 128));
        require(uniforms->create(), "RHI uniform buffer failed");
        outputUniform.reset(api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
        require(outputUniform->create(), "Output uniform buffer failed");
        linear.reset(api->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        require(linear->create(), "Linear sampler failed");
        nearest.reset(api->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        require(nearest->create(), "Depth sampler failed");
        mipLinear.reset(api->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                        QRhiSampler::Linear, QRhiSampler::ClampToEdge,
                                        QRhiSampler::ClampToEdge));
        require(mipLinear->create(), "Capture mip sampler failed");
        environment.reset(api->newTexture(QRhiTexture::RGBA32F, { 768, 1024 }));
        require(environment->create(), "Room environment texture failed");
        for (auto *t : { &dfg, &ltc1, &ltc2 }) {
            t->reset(api->newTexture(QRhiTexture::RGBA32F,
                                     t == &dfg ? QSize(16, 16) : QSize(64, 64)));
            require((*t)->create(), "Shell lookup texture failed");
        }
    }
    if (targetSize != renderTarget()->pixelSize() || shadowSize != std::max(1, input.shadowSize)
        || !cover || cover->pixelSize() != input.cover.size()
        || captureSamples != input.captureSamples || targetsHaveShells != input.shells) {
        shellPipeline.reset();
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
        require(api->supportedSampleCounts().contains(captureSamples),
                "Requested capture MSAA is unsupported");
        ++generation;
        require(qint64(targetSize.width()) * targetSize.height() <= 8294400,
                "RHI target exceeds RenderQuality pixel limit");
        require(std::max(targetSize.width(), targetSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "RHI target exceeds GPU dimensions");
        require(shadowSize <= api->resourceLimit(QRhi::TextureSizeMax),
                "Shadow target exceeds GPU dimensions");
        makeTarget(opaque, targetSize, true, captureSamples, input.shells);
        if (input.shells)
            makeTarget(composite, targetSize, true, captureSamples);
        else
            composite.reset();
        makeTarget(shadow, { shadowSize, shadowSize }, false);
        cover.reset(api->newTexture(QRhiTexture::RGBA8, input.cover.size()));
        require(cover->create(), "Cover texture failed");
        coverDirty = true;
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
                          3, QRhiShaderResourceBinding::FragmentStage, shadow.color.get(),
                          nearest.get()) });
        require(geometryBindings->create(), "Geometry bindings failed");
        shadowBindings.reset(api->newShaderResourceBindings());
        shadowBindings->setBindings({ QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0, stages, uniforms.get(), sizeof(Uniform)) });
        require(shadowBindings->create(), "Shadow bindings failed");
        opaquePipeline = pipeline(opaque, geometryBindings.get(), false);
        shadowPipeline = pipeline(shadow, shadowBindings.get(), true);
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
                      QRhiShaderResourceBinding::sampledTexture(3, fragment, shadow.color.get(),
                                                                nearest.get()),
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
        }
    }
    // QQuickRhiItem may replace its render-pass descriptor when only its output changes.
    outputPipeline.reset();
    outputBindings.reset();
    outputBindings.reset(api->newShaderResourceBindings());
    outputBindings->setBindings(
            { QRhiShaderResourceBinding::sampledTexture(
                      0, QRhiShaderResourceBinding::FragmentStage,
                      input.shells ? composite.color.get() : opaque.color.get(), linear.get()),
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
}
Uniform Renderer::uniform(const QMatrix4x4 &model, int kind) const
{
    Uniform u{ };
    copy(u.mvp, api->clipSpaceCorrMatrix() * input.scene.projection * input.scene.view * model);
    copy(u.model, model);
    copy(u.view, input.scene.view);
    copy(u.lightMvp, api->clipSpaceCorrMatrix() * lightProjection() * lightView() * model);
    copy(u.lightVP, lightProjection() * lightView());
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
    copy(u.nativeVP, api->clipSpaceCorrMatrix() * input.scene.projection * input.scene.view);
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
void Renderer::render(QRhiCommandBuffer *cb)
{
    // initialize may precede synchronize in Qt; defer resource-size changes to a fresh frame.
    if (!api || input.cover.isNull())
        return;
    if (!cover || shadowSize != std::max(1, input.shadowSize)
        || cover->pixelSize() != input.cover.size() || captureSamples != input.captureSamples
        || targetsHaveShells != input.shells) {
        initialize(cb);
    }
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [](const auto &p) {
                                     return p->remaining == 0;
                                 }),
                  pending.end());
    auto *updates = api->nextResourceUpdateBatch();
    if (uploadGeometry) {
        updates->uploadStaticBuffer(vertices.get(), vertexBytes);
        updates->uploadStaticBuffer(indices.get(), indexBytes);
        const auto room =
                floatTextureBytes(":/rhi/environment/room-pmrem.rgba32f", 768 * 1024 * 16);
        updates->uploadTexture(environment.get(),
                               QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(room)) }));
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
    if (coverDirty) {
        // Filter premultiplied texels, then unpremultiply in the paper shader. Straight
        // transparent black texels otherwise produce a dark line at the alpha-test edge.
        const auto image = input.cover.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        const QByteArray bytes(reinterpret_cast<const char *>(image.constBits()),
                               image.sizeInBytes());
        updates->uploadTexture(cover.get(),
                               QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(bytes)) }));
        coverDirty = false;
    }
    // Upload bookkeeping is deliberately not an R10C tile/budget claim.
    QVector<QMatrix4x4> entities = input.entities;
    if (entities.isEmpty())
        entities.append(input.scene.caseTransform);
    require(entities.size() * 5 + 2 <= 128, "RHI review draw uniform capacity exceeded");
    QByteArray bytes((entities.size() * 5 + 2) * uniformStride, '\0');
    quint32 cursor = 0;
    auto append = [&](const QMatrix4x4 &m, int kind) {
        auto u = uniform(m, kind);
        const auto offset = cursor;
        std::memcpy(bytes.data() + cursor, &u, sizeof(u));
        cursor += uniformStride;
        return offset;
    };
    QVector<quint32> shadowOffsets, opaqueOffsets;
    QVector<int> opaqueOrder;
    for (int i = 0; i < entities.size(); ++i)
        opaqueOrder.append(i);
    if (input.shells)
        std::sort(opaqueOrder.begin(), opaqueOrder.end(), [&](int a, int b) {
            const float da = -(input.scene.view * entities[a] * QVector4D(paper.center, 1)).z();
            const float db = -(input.scene.view * entities[b] * QVector4D(paper.center, 1)).z();
            if (da != db)
                return da < db;
            const auto ia = input.entityIds.isEmpty() ? quint64(a) : input.entityIds[a];
            const auto ib = input.entityIds.isEmpty() ? quint64(b) : input.entityIds[b];
            return ia < ib;
        });
    for (const auto &m : entities)
        shadowOffsets.append(append(m, 1));
    const auto bgOffset = append({ }, -1);
    for (const auto &m : entities)
        opaqueOffsets.append(append(m, input.geometryDiagnostic ? 1 : 0));
    const auto floorOffset = append({ }, 2);
    struct ShellDraw
    {
        Mesh mesh;
        quint32 offset;
        float depth;
        quint64 identity;
        int surface;
    };
    QVector<ShellDraw> shells;
    if (input.shells) {
        require(entities.size() == 1 || input.entityIds.size() == entities.size(),
                "Shell instances require stable IDs");
        for (int i : opaqueOrder) {
            const std::array<Mesh, 3> meshes{ lid, spine, tray };
            const auto id = input.entityIds.isEmpty() ? 0 : input.entityIds[i];
            for (int surface = 0; surface < 3; ++surface) {
                const auto mesh = meshes[surface];
                const auto center = input.scene.view * entities[i] * QVector4D(mesh.center, 1);
                shells.append({ mesh, append(entities[i], 3 + surface), -center.z(), id, surface });
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
    updates->updateDynamicBuffer(uniforms.get(), 0, bytes.size(), bytes.constData());
    const float out[4] = { api->isYUpInFramebuffer() ? 0.0f : 1.0f,
                           input.geometryDiagnostic || input.shadowDiagnostic || input.rawOutput
                                   ? 1.0f
                                   : 0.0f,
                           input.shells && input.lightingEnabled && !input.rawOutput
                                           && !input.geometryDiagnostic && !input.shadowDiagnostic
                                   ? 1.0f
                                   : 0.0f,
                           input.exposure };
    updates->updateDynamicBuffer(outputUniform.get(), 0, 16, out);
    cb->beginPass(shadow.rt.get(), Qt::white, { 1, 0 }, updates);
    cb->setViewport({ 0, 0, float(shadowSize), float(shadowSize) });
    if (input.shadows)
        for (int i = 0; i < entities.size(); ++i)
            drawMesh(cb, shadowPipeline.get(), shadowBindings.get(), tray, shadowOffsets[i]);
    cb->endPass();
    auto drawOpaque = [&](Target &target) {
        cb->beginPass(target.rt.get(), Qt::black, { 1, 0 });
        cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
        drawMesh(cb, backgroundPipeline.get(), geometryBindings.get(), backgroundTriangle,
                 bgOffset);
        if (input.shadowDiagnostic)
            drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), floor, floorOffset);
        if (!input.shadowDiagnostic || input.shells)
            for (int i : opaqueOrder) {
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
        // Reconstruct opaque depth in the separate ping-pong destination, then write shells.
        // Both MRT layouts/sample counts are compatible; no sampled attachment aliases this target.
        drawOpaque(composite);
        if (!input.geometryDiagnostic)
            for (const auto &draw : shells)
                drawMesh(cb, shellPipeline.get(), shellBindings.get(), draw.mesh, draw.offset);
        cb->endPass();
    }
    cb->beginPass(renderTarget(), Qt::black, { 1, 0 });
    cb->setGraphicsPipeline(outputPipeline.get());
    cb->setViewport({ 0, 0, float(renderTarget()->pixelSize().width()),
                      float(renderTarget()->pixelSize().height()) });
    cb->setShaderResources();
    cb->draw(3);
    cb->endPass();
    if (deliver && input.request != capturedRequest) {
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
            { "shellBlend", false },
            { "shellCull", "Back" }
        };
        auto *reads = api->nextResourceUpdateBatch();
        const std::array<QRhiTexture *, 7> textures{
            opaque.color.get(),
            opaque.normal.get(),
            opaque.depth.get(),
            shadow.color.get(),
            input.shells ? composite.color.get() : opaque.color.get(),
            input.shells ? composite.normal.get() : opaque.normal.get(),
            input.shells ? composite.depth.get() : opaque.depth.get()
        };
        QJsonArray sorted;
        for (const auto &draw : shells)
            sorted.append(QJsonObject{ { "id", qint64(draw.identity) },
                                       { "surface", draw.surface },
                                       { "depth", draw.depth } });
        p->capture.metadata["sortedShells"] = sorted;
        for (int i = 0; i < 7; ++i) {
            p->results[i].completed = [weak = std::weak_ptr<Pending>(p), i] {
                if (auto state = weak.lock()) {
                    auto img = imageOf(state->results[i], state->yUp);
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
                    else
                        state->capture.compositeDepth = img;
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
}
}
RhiItem::RhiItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    setAlphaBlending(false);
}
void RhiItem::submit(Input input, QSize target)
{
    require(!input.cover.isNull(), "Missing cover at RHI synchronization boundary");
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
