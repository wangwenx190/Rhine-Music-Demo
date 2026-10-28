// Captures actual submitted bytes and owned resources for deterministic render regressions.
#include "Renderer_p.h"
#include "TransferAudit.h"
#include "RhiObservation.h"
using namespace Qt::StringLiterals;
namespace yalahoo::rhi::renderDetail {
void Renderer::captureFrame(QRhiCommandBuffer *cb, bool batched, int visibleCount, int cursor,
                            const QByteArray &bytes, quint32 floorOffset, bool shadowRendered,
                            QJsonArray fogUniforms, const float *out,
                            const QJsonArray &captureKinds, const QJsonArray &opticalEvents,
                            const QVector<QMatrix4x4> &entities)
{
    const auto recordFog = [&](const Uniform &submitted) {
        appendFogUniform(fogUniforms, submitted);
    };
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
                         { u"shellCull"_s, u"Back"_s } };
    inspectCapture(capture);
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
        effectiveRegions.append(QJsonObject{
                { u"x"_s, tile.x() },
                { u"y"_s, tile.y() },
                { u"width"_s, tile.width() },
                { u"height"_s, tile.height() },
                { u"lease"_s,
                  effectiveCoverLeases.isEmpty() ? QJsonValue()
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
                      || std::find(shellBindings->cbeginBindings(), shellBindings->cendBindings(),
                                   QRhiShaderResourceBinding::sampledTexture(
                                           ShadowBinding, QRhiShaderResourceBinding::FragmentStage,
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
void Renderer::publishProfile(QRhiCommandBuffer *cb, double cpuMs, int cursor, bool shadowRendered,
                              int visibleCount)
{
    const auto gpu = cb->lastCompletedGpuTime();
    profile(QJsonObject{
            { u"cpuSubmitMs"_s, cpuMs },
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
              gpu > 0 ? QJsonValue(gpu * yalahoo::units::MillisecondsPerSecond) : QJsonValue() },
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
