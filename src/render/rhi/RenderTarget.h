// Owns one render target and its attachment lifetimes. Compatible sequential passes may share
// transient MSAA/depth storage; resolve textures remain owned by their individual target.

#pragma once
#include "RhiApi.h"
#include "RenderData.h"
#include <array>

namespace yalahoo::rhi {
struct Target final
{
    Q_DISABLE_COPY_MOVE(Target)
    Target() = default;
    bool mrt = false;
    bool prepass = false;
    QSize size;
    std::unique_ptr<QRhiTexture> color, normal, depth;
    std::array<std::shared_ptr<QRhiTexture>, yalahoo::rhi::renderData::CaseSurfaceCount>
            multisample;
    std::array<std::shared_ptr<QRhiRenderBuffer>, yalahoo::rhi::renderData::CaseSurfaceCount>
            multisampleBuffers;
    std::shared_ptr<QRhiRenderBuffer> z;
    std::unique_ptr<QRhiTexture> hardwareDepth;
    std::unique_ptr<QRhiRenderPassDescriptor> pass;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    void create(QRhi *api, QSize size, bool mrt, int samples = 1, bool mips = false,
                QRhiTexture::Format format = QRhiTexture::RGBA16F,
                const Target *sharedAttachments = nullptr);
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
}
