#include "RenderTarget.h"
#include <QOpenGLContext>
#include <utility>

namespace yalahoo::rhi {
void OutputTarget::create(QRhi *api, QRhiTexture *color, QRhiRenderBuffer *multisample,
                          QRhiTexture *resolve)
{
    require((color && !multisample && !resolve) || (!color && multisample && resolve),
            "Invalid Qt output color attachments");
    QRhiColorAttachment attachment;
    if (color)
        attachment.setTexture(color);
    else {
        attachment.setRenderBuffer(multisample);
        attachment.setResolveTexture(resolve);
    }
    // initialize() also runs when Qt rebuilds native storage behind the same texture object.
    // Recreate the target on every such notification, retaining a compatible pass and PSO.
    rt.reset(api->newTextureRenderTarget({ attachment }));
    std::unique_ptr<QRhiRenderPassDescriptor> candidate(rt->newCompatibleRenderPassDescriptor());
    if (!pass || !pass->isCompatible(candidate.get()))
        pass = std::move(candidate);
    rt->setRenderPassDescriptor(pass.get());
    require(rt->create(), "Cannot create color-only Qt output target");
}
void Target::create(QRhi *api, QSize size, bool mrt, int samples, bool mips,
                    QRhiTexture::Format format, const Target *sharedAttachments)
{
    auto &target = *this;
    target.reset();
    target.mrt = mrt;
    target.prepass = mrt && format == QRhiTexture::RGBA8;
    target.size = size;
    auto texture = [&](QRhiTexture::Format pixelFormat, bool mipmapped = false) {
        std::unique_ptr<QRhiTexture> t(api->newTexture(
                pixelFormat, size, 1,
                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource
                        | (mipmapped ? QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips
                                     : QRhiTexture::Flags())));
        require(t->create(), "Cannot create RHI float target");
        return t;
    };
    if (!target.prepass)
        target.color = texture(format, mips);
    if (sharedAttachments) {
        require(sharedAttachments->rt && sharedAttachments->size == size
                        && sharedAttachments->mrt == mrt
                        && sharedAttachments->rt->sampleCount() == samples
                        && sharedAttachments->color && sharedAttachments->color->format() == format,
                "Shared transient attachments are incompatible");
        target.z = sharedAttachments->z;
        // Only resolved opaque color survives into the composite pass. Its MSAA
        // storage can be cleared/reused; each pass retains its own resolve images.
        target.multisample = sharedAttachments->multisample;
        target.multisampleBuffers = sharedAttachments->multisampleBuffers;
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
            if (sharedAttachments) {
                if (target.multisample[i])
                    attachment.setTexture(target.multisample[i].get());
                else
                    attachment.setRenderBuffer(target.multisampleBuffers[i].get());
                attachment.setResolveTexture(resolves[i]);
                attachments.append(attachment);
                continue;
            }
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
    desc.setDepthStencilBuffer(target.z.get());
    target.rt.reset(api->newTextureRenderTarget(
            desc,
            sharedAttachments ? QRhiTextureRenderTarget::PreserveDepthStencilContents
                              : QRhiTextureRenderTarget::Flags()));
    target.pass.reset(target.rt->newCompatibleRenderPassDescriptor());
    target.rt->setRenderPassDescriptor(target.pass.get());
    require(target.rt->create(), "Cannot create RHI render target");
}
}
