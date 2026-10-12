#include "RhiItem.h"
#include "RhiApi.h"
#include "GeometryAudit.h"
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
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
};
static_assert(sizeof(Uniform) == 560 && offsetof(Uniform, background) == 320
              && offsetof(Uniform, optics) == 544);
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
};
struct Target
{
    Owned<QRhiTexture> color, normal, depth;
    Owned<QRhiRenderBuffer> z;
    Owned<QRhiRenderPassDescriptor> pass;
    Owned<QRhiTextureRenderTarget> rt;
    void reset()
    {
        rt.reset();
        pass.reset();
        z.reset();
        depth.reset();
        normal.reset();
        color.reset();
    }
    // Dependencies are destroyed in reverse declaration order: target before attachments.
};
struct Pending
{
    std::array<QRhiReadbackResult, 4> results;
    Capture capture;
    std::function<void(Capture)> deliver;
    int remaining = 4;
    bool yUp = false;
};
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
    void makeTarget(Target &target, QSize size, bool mrt);
    Owned<QRhiGraphicsPipeline> pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                         bool shadow);
    Uniform uniform(const QMatrix4x4 &model, int kind) const;
    void drawMesh(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *pipeline,
                  QRhiShaderResourceBindings *bindings, Mesh mesh, quint32 offset);
    QRhi *api = nullptr;
    Input input;
    std::function<void(Capture)> deliver;
    Target shadow, opaque;
    Owned<QRhiBuffer> vertices, indices, uniforms, outputUniform;
    Owned<QRhiTexture> cover, environment;
    Owned<QRhiSampler> linear, nearest;
    Owned<QRhiShaderResourceBindings> geometryBindings, shadowBindings, outputBindings;
    Owned<QRhiGraphicsPipeline> shadowPipeline, opaquePipeline, backgroundPipeline, outputPipeline;
    Mesh lid, spine, tray, paper, floor, backgroundTriangle;
    QByteArray vertexBytes, indexBytes;
    QSize targetSize;
    int shadowSize = 0, uniformStride = 0;
    bool uploadGeometry = true;
    bool coverDirty = true;
    bool distinctThread = false;
    quint64 capturedRequest = 0, generation = 0;
    qint64 coverSourceKey = 0;
    std::vector<std::shared_ptr<Pending>> pending;
};
void Renderer::reset()
{
    outputPipeline.reset();
    backgroundPipeline.reset();
    opaquePipeline.reset();
    shadowPipeline.reset();
    outputBindings.reset();
    geometryBindings.reset();
    shadowBindings.reset();
    opaque.reset();
    shadow.reset();
    uniforms.reset();
    outputUniform.reset();
    vertices.reset();
    indices.reset();
    cover.reset();
    environment.reset();
    linear.reset();
    nearest.reset();
    targetSize = { };
    shadowSize = 0;
    uploadGeometry = true;
    coverDirty = true;
}
void Renderer::makeTarget(Target &target, QSize size, bool mrt)
{
    target.reset();
    auto texture = [&] {
        Owned<QRhiTexture> t(
                api->newTexture(QRhiTexture::RGBA32F, size, 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        require(t->create(), "Cannot create RHI float target");
        return t;
    };
    target.color = texture();
    target.z.reset(api->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    require(target.z->create(), "Cannot create RHI depth attachment");
    QRhiTextureRenderTargetDescription desc;
    if (mrt) {
        target.normal = texture();
        target.depth = texture();
        desc.setColorAttachments({ target.color.get(), target.normal.get(), target.depth.get() });
    } else
        desc.setColorAttachments({ target.color.get() });
    desc.setDepthStencilBuffer(target.z.get());
    target.rt.reset(api->newTextureRenderTarget(desc));
    target.pass.reset(target.rt->newCompatibleRenderPassDescriptor());
    target.rt->setRenderPassDescriptor(target.pass.get());
    require(target.rt->create(), "Cannot create RHI render target");
}
Owned<QRhiGraphicsPipeline> Renderer::pipeline(Target &target, QRhiShaderResourceBindings *bindings,
                                               bool isShadow)
{
    Owned<QRhiGraphicsPipeline> p(api->newGraphicsPipeline());
    p->setShaderStages(
            { { QRhiShaderStage::Vertex,
                shader(isShadow ? ":/rhi/shadow.vert.qsb" : ":/rhi/geometry.vert.qsb") },
              { QRhiShaderStage::Fragment,
                shader(isShadow ? ":/rhi/shadow.frag.qsb" : ":/rhi/opaque.frag.qsb") } });
    QRhiVertexInputLayout layout;
    layout.setBindings({ { 32 } });
    layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                           { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
                           { 0, 2, QRhiVertexInputAttribute::Float2, 24 } });
    p->setVertexInputLayout(layout);
    p->setShaderResourceBindings(bindings);
    p->setRenderPassDescriptor(target.pass.get());
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
        environment.reset(api->newTexture(QRhiTexture::RGBA32F, { 768, 1024 }));
        require(environment->create(), "Room environment texture failed");
    }
    if (targetSize != renderTarget()->pixelSize() || shadowSize != std::max(1, input.shadowSize)
        || !cover || cover->pixelSize() != input.cover.size()) {
        outputPipeline.reset();
        backgroundPipeline.reset();
        opaquePipeline.reset();
        shadowPipeline.reset();
        outputBindings.reset();
        geometryBindings.reset();
        shadowBindings.reset();
        targetSize = renderTarget()->pixelSize();
        shadowSize = std::max(1, input.shadowSize);
        ++generation;
        require(qint64(targetSize.width()) * targetSize.height() <= 8294400,
                "RHI target exceeds RenderQuality pixel limit");
        require(std::max(targetSize.width(), targetSize.height())
                        <= api->resourceLimit(QRhi::TextureSizeMax),
                "RHI target exceeds GPU dimensions");
        require(shadowSize <= api->resourceLimit(QRhi::TextureSizeMax),
                "Shadow target exceeds GPU dimensions");
        makeTarget(opaque, targetSize, true);
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
                  QRhiShaderResourceBinding::sampledTexture(
                          1, QRhiShaderResourceBinding::FragmentStage, cover.get(), linear.get()),
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
    }
    // QQuickRhiItem may replace its render-pass descriptor when only its output changes.
    outputPipeline.reset();
    outputBindings.reset();
    outputBindings.reset(api->newShaderResourceBindings());
    outputBindings->setBindings(
            { QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                        opaque.color.get(), linear.get()),
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
        || cover->pixelSize() != input.cover.size()) {
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
        const auto room = assets::readFile(":/rhi/environment/room-pmrem.rgba32f");
        require(room.size() == 768 * 1024 * 16, "Invalid room environment bytes");
        updates->uploadTexture(environment.get(),
                               QRhiTextureUploadDescription({ QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(room)) }));
        uploadGeometry = false;
    }
    if (coverDirty) {
        updates->uploadTexture(cover.get(), input.cover.convertToFormat(QImage::Format_RGBA8888));
        coverDirty = false;
    }
    // Upload bookkeeping is deliberately not an R10C tile/budget claim.
    QVector<QMatrix4x4> entities = input.entities;
    if (entities.isEmpty())
        entities.append(input.scene.caseTransform);
    require(entities.size() * 5 + 2 <= 128, "R10A uniform capacity exceeded");
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
    for (const auto &m : entities)
        shadowOffsets.append(append(m, 1));
    const auto bgOffset = append({ }, -1);
    for (const auto &m : entities)
        opaqueOffsets.append(append(m, input.geometryDiagnostic ? 1 : 0));
    const auto floorOffset = append({ }, 2);
    updates->updateDynamicBuffer(uniforms.get(), 0, bytes.size(), bytes.constData());
    const float out[4] = { api->isYUpInFramebuffer() ? 0.0f : 1.0f,
                           input.geometryDiagnostic || input.shadowDiagnostic ? 1.0f : 0.0f, 0, 0 };
    updates->updateDynamicBuffer(outputUniform.get(), 0, 16, out);
    cb->beginPass(shadow.rt.get(), Qt::white, { 1, 0 }, updates);
    cb->setViewport({ 0, 0, float(shadowSize), float(shadowSize) });
    if (input.shadows)
        for (int i = 0; i < entities.size(); ++i)
            drawMesh(cb, shadowPipeline.get(), shadowBindings.get(), tray, shadowOffsets[i]);
    cb->endPass();
    cb->beginPass(opaque.rt.get(), Qt::black, { 1, 0 });
    cb->setViewport({ 0, 0, float(targetSize.width()), float(targetSize.height()) });
    drawMesh(cb, backgroundPipeline.get(), geometryBindings.get(), backgroundTriangle, bgOffset);
    if (input.shadowDiagnostic)
        drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), floor, floorOffset);
    else
        for (int i = 0; i < entities.size(); ++i) {
            if (input.geometryDiagnostic)
                for (auto mesh : { lid, spine, tray })
                    drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), mesh,
                             opaqueOffsets[i]);
            drawMesh(cb, opaquePipeline.get(), geometryBindings.get(), paper, opaqueOffsets[i]);
        }
    cb->endPass();
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
            { "passes", QJsonArray{ "shadow", "opaque-color-normal-depth", "review-output" } },
            { "msaaSamples", 1 }
        };
        auto *reads = api->nextResourceUpdateBatch();
        const std::array<QRhiTexture *, 4> textures{ opaque.color.get(), opaque.normal.get(),
                                                     opaque.depth.get(), shadow.color.get() };
        for (int i = 0; i < 4; ++i) {
            p->results[i].completed = [weak = std::weak_ptr<Pending>(p), i] {
                if (auto state = weak.lock()) {
                    auto img = imageOf(state->results[i], state->yUp);
                    if (i == 0)
                        state->capture.color = img;
                    else if (i == 1)
                        state->capture.normal = img;
                    else if (i == 2)
                        state->capture.depth = img;
                    else
                        state->capture.shadow = img;
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
