include(CheckCXXSourceCompiles)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
if(NOT TARGET Qt6::GuiPrivate)
    message(
        FATAL_ERROR
        "Latest selected Qt Kit must provide GuiPrivate for QRhi; install its matching development files"
    )
endif()
set(CMAKE_REQUIRED_LIBRARIES Qt6::Quick Qt6::GuiPrivate)
# Always re-probe: Kit upgrades and private interface adaptations must not reuse a cached success.
unset(YALAHOO_RHI_API CACHE)
check_cxx_source_compiles(
    [=[
    #include <QQuickRhiItem>
    #include <rhi/qrhi.h>
    #include <rhi/qshader.h>
    class Probe : public QQuickRhiItemRenderer
    {
        void synchronize(QQuickRhiItem *) override
        {
        }
        void initialize(QRhiCommandBuffer *) override
        {
        }
        void render(QRhiCommandBuffer *cb) override
        {
            QRhi *r = rhi();
            auto matrix = r->clipSpaceCorrMatrix();
            auto t = r->newTexture(QRhiTexture::RGBA32F, { 16, 16 }, 1,
                                   QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
            auto d = r->newRenderBuffer(QRhiRenderBuffer::DepthStencil, { 16, 16 }, 1);
            QRhiTextureRenderTargetDescription desc(QRhiColorAttachment(t), d);
            auto target = r->newTextureRenderTarget(desc);
            auto pass = target->newCompatibleRenderPassDescriptor();
            target->setRenderPassDescriptor(pass);
            auto u = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                  r->ubufAligned(816) * 2);
            auto vb = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 32);
            auto ib = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, 12);
            auto sampler = r->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                         QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            auto bindings = r->newShaderResourceBindings();
            auto stages =
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
            bindings->setBindings(
                    { QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, stages, u, 816),
                      QRhiShaderResourceBinding::sampledTexture(
                              1, QRhiShaderResourceBinding::FragmentStage, t, sampler) });
            auto pipeline = r->newGraphicsPipeline();
            pipeline->setShaderStages({ { QRhiShaderStage::Vertex, QShader::fromSerialized({ }) },
                                        { QRhiShaderStage::Fragment, QShader() } });
            QRhiVertexInputLayout layout;
            layout.setBindings({ { 32 } });
            layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
                                   { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
                                   { 0, 2, QRhiVertexInputAttribute::Float2, 24 } });
            pipeline->setVertexInputLayout(layout);
            pipeline->setShaderResourceBindings(bindings);
            pipeline->setRenderPassDescriptor(pass);
            pipeline->setDepthTest(true);
            pipeline->setDepthWrite(true);
            pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
            pipeline->setCullMode(QRhiGraphicsPipeline::Front);
            pipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
            pipeline->setTargetBlends({ { }, { }, { } });
            pipeline->setSampleCount(renderTarget()->sampleCount());
            auto b = r->nextResourceUpdateBatch();
            auto msaa = r->newTexture(QRhiTexture::RGBA32F, { 16, 16 }, 4, QRhiTexture::RenderTarget);
            auto capture = r->newTexture(QRhiTexture::RGBA32F, { 16, 16 }, 1,
                                        QRhiTexture::RenderTarget | QRhiTexture::MipMapped
                                                | QRhiTexture::UsedWithGenerateMips);
            QRhiColorAttachment attachment(msaa);
            attachment.setResolveTexture(capture);
            b->generateMips(capture);
            (void)r->supportedSampleCounts();
            QRhiReadbackResult result;
            b->readBackTexture(QRhiReadbackDescription(t), &result);
            b->uploadTexture(t, QImage({ 16, 16 }, QImage::Format_RGBA8888));
            QRhiTextureSubresourceUploadDescription tile(QImage({ 8, 8 }, QImage::Format_RGBA8888));
            tile.setSourceSize({ 8, 8 });
            tile.setDestinationTopLeft({ 4, 4 });
            b->uploadTexture(t, { { 0, 0, tile } });
            (void)cb->lastCompletedGpuTime();
            cb->beginPass(target, Qt::black, { 1, 0 }, b);
            cb->setGraphicsPipeline(pipeline);
            cb->setViewport({ 0, 0, 16, 16 });
            QRhiCommandBuffer::DynamicOffset offset{ 0, 0 };
            cb->setShaderResources(bindings, 1, &offset);
            QRhiCommandBuffer::VertexInput input(vb, 0);
            cb->setVertexInput(0, 1, &input, ib, 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(3, 1, 0);
            cb->endPass();
            cb->resourceUpdate(r->nextResourceUpdateBatch());
            (void)r->resourceLimit(QRhi::TextureSizeMax);
            (void)r->driverInfo();
            (void)r->isYUpInFramebuffer();
            (void)r->isYUpInNDC();
            (void)r->isClipDepthZeroToOne();
            (void)renderTarget();
            (void)colorTexture();
            (void)resolveTexture();
        }
    };
    int main()
    {
        Probe p;
        return 0;
    }
]=]
    YALAHOO_RHI_API
)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT YALAHOO_RHI_API)
    message(
        FATAL_ERROR
        "Selected latest Kit QRhi/QQuickRhiItem API probe failed. Adapt src/render/rhi; no older Qt fallback."
    )
endif()
if(WIN32 AND YALAHOO_REVIEW)
    set(CMAKE_REQUIRED_LIBRARIES Qt6::Quick Qt6::GuiPrivate)
    set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/src/render/rhi")
    set(CMAKE_REQUIRED_DEFINITIONS -DYALAHOO_RHI_AUDIT)
    unset(YALAHOO_RHI_TRANSFER_AUDIT_API CACHE)
    check_cxx_source_compiles(
        "#include \"TransferAudit.h\"\nint main() { auto function = &yalahoo::rhi::nativeTransfers; (void)function; return 0; }"
        YALAHOO_RHI_TRANSFER_AUDIT_API
    )
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    if(NOT YALAHOO_RHI_TRANSFER_AUDIT_API)
        message(FATAL_ERROR "Latest Qt D3D11 command audit API changed. Adapt the isolated Review probe; do not substitute CPU byte counters.")
    endif()
endif()
