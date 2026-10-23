#include "ReadbackQueue_p.h"
#include "ReadbackImage.h"
#include "ResourceInventory.h"
#include "RenderData.h"
#include <QFloat16>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
namespace {
constexpr auto MaximumPendingCaptures = 2;
}
ReadbackQueue::~ReadbackQueue() = default;
bool ReadbackQueue::hasCapacity() const
{
    return pending.size() < MaximumPendingCaptures;
}
void ReadbackQueue::retire()
{
    std::erase_if(pending, [](const auto &capture) {
        return capture->remaining == 0;
    });
}
bool ReadbackQueue::drain()
{
    bool needsFrame = false;
    for (const auto &capture : pending)
        if (capture->remaining > 0 && capture->drainFrames > 0) {
            --capture->drainFrames;
            needsFrame = true;
        }
    return needsFrame;
}
void ReadbackQueue::finish(QRhi *api)
{
    if (api && !pending.empty())
        api->finish();
}
void ReadbackQueue::reset()
{
    pending.clear();
    for (auto &texture : alignedReadbacks)
        texture.reset();
    readbackPackAlignment = 0;
    alignedReadbackRevision = 0;
}
void ReadbackQueue::inventory(ResourceInventory &inventory) const
{
    for (int i = 0; i < int(alignedReadbacks.size()); ++i)
        inventory.texture(u"capture-aligned-scalar-%1"_s.arg(i), alignedReadbacks[i].get());
}
void ReadbackQueue::enqueue(QRhi *api, QRhiCommandBuffer *cb,
                            const std::array<QRhiTexture *, CaptureFieldCount> &textures,
                            Capture capture, const Input &input,
                            std::function<void(Capture)> deliver,
                            std::function<void(QString)> failed,
                            const std::function<QJsonObject()> &inventory)
{
    require(hasCapacity(), "RHI capture queue capacity exceeded");
    auto p = std::make_shared<Pending>();
    p->drainFrames = api->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1;
    p->capture = std::move(capture);
    p->opticalProbe = input.opticalProbe;
    p->captureShadowDepth = input.captureShadowDepth;
    p->deliver = std::move(deliver);
    p->failed = std::move(failed);
    p->yUp = api->isYUpInFramebuffer();
    ResourceUpdates reads(api->nextResourceUpdateBatch());
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
            return completeReadback(weak, i);
        };
        auto *readTexture = textures[i];
        readTexture = prepareReadback(api, readTexture, i, reads.get(), *p, alignedCopies);
        reads->readBackTexture(QRhiReadbackDescription(readTexture), &p->results[i]);
    }
    p->capture.metadata[u"alignedScalarCopies"_s] = alignedCopies;
    p->capture.metadata[u"alignedReadbackRevision"_s] = qint64(alignedReadbackRevision);
    p->capture.metadata[u"resources"_s] = inventory();
    p->capture.metadata[u"uniqueReadbackTextures"_s] = p->remaining;
    p->capture.metadata[u"readbackFieldSlots"_s] =
            (input.captureAtlas ? 13 : 12) + int(input.captureFloorDepth);
    pending.push_back(p);
    cb->resourceUpdate(reads.release());
}
QRhiTexture *ReadbackQueue::prepareReadback(QRhi *api, QRhiTexture *readTexture, int i,
                                            QRhiResourceUpdateBatch *reads, Pending &pending,
                                            QJsonArray &alignedCopies)
{
    const auto logical = readTexture->pixelSize();
    if (api->backend() != QRhi::OpenGLES2 || readTexture->format() != QRhiTexture::R16F
        || logical.width() * 2 % readbackPackAlignment == 0)
        return readTexture;

    const int rowBytes = (logical.width() * 2 + readbackPackAlignment - 1) / readbackPackAlignment
            * readbackPackAlignment;
    const QSize padded(rowBytes / 2, logical.height());
    auto &texture = alignedReadbacks[i];
    if (!texture || texture->pixelSize() != padded) {
        texture.reset(
                api->newTexture(QRhiTexture::R16F, padded, 1, QRhiTexture::UsedAsTransferSource));
        require(texture->create(), "Aligned scalar readback texture failed");
        ++alignedReadbackRevision;
    }
    QRhiTextureCopyDescription copy;
    copy.setPixelSize(logical);
    reads->copyTexture(texture.get(), readTexture, copy);
    readTexture = texture.get();
    pending.logicalReadbackSizes[i] = logical;
    alignedCopies.append(QJsonObject{ { u"field"_s, i },
                                      { u"logicalWidth"_s, logical.width() },
                                      { u"storageWidth"_s, padded.width() },
                                      { u"height"_s, logical.height() } });

    return readTexture;
}
void ReadbackQueue::completeReadback(const std::weak_ptr<Pending> &weak, const int &i)
{
    auto state = weak.lock();
    if (!state)
        return;

    try {
        const auto image = readbackImage(state->results[i], state->yUp, i == AtlasField,
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
                    std::memcpy(&half, readback.data.constData() + offset + channel * 2, 2);
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
        const auto message = u"Readback field %1: %2"_s.arg(i).arg(QString::fromUtf8(error.what()));
        qWarning("%s", qPrintable(message));
        state->capture.metadata[u"readbackError"_s] = message;
        if (state->failed)
            state->failed(message);
    }
    if (--state->remaining == 0)
        state->deliver(std::move(state->capture));
}
}
