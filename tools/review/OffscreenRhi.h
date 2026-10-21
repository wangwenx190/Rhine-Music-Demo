// Owns a direct offscreen QRhi together with the native surfaces/instance it borrows.
// Declaration order destroys QRhi before its fallback OpenGL surface or Vulkan instance.

#pragma once
#include "RhiApi.h"
#include <QOffscreenSurface>
#include <QQuickWindow>
#include <rhi/qrhi_platform.h>
#include <memory>
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#  include <QVulkanInstance>
#endif

namespace yalahoo::review {
class OffscreenRhi final
{
    Q_DISABLE_COPY_MOVE(OffscreenRhi)
public:
    OffscreenRhi()
    {
        switch (QQuickWindow::graphicsApi()) {
#if QT_CONFIG(opengl)
        case QSGRendererInterface::OpenGL: {
            QRhiGles2InitParams params;
            m_fallback.reset(QRhiGles2InitParams::newFallbackSurface());
            params.fallbackSurface = m_fallback.get();
            m_api.reset(QRhi::create(QRhi::OpenGLES2, &params));
            break;
        }
#endif
#if defined(Q_OS_MACOS) && QT_CONFIG(metal)
        case QSGRendererInterface::Metal: {
            QRhiMetalInitParams params;
            m_api.reset(QRhi::create(QRhi::Metal, &params));
            break;
        }
#elif defined(Q_OS_WIN)
        case QSGRendererInterface::Direct3D11: {
            QRhiD3D11InitParams params;
            m_api.reset(QRhi::create(QRhi::D3D11, &params));
            break;
        }
#  if QT_CONFIG(direct3d12)
        case QSGRendererInterface::Direct3D12: {
            QRhiD3D12InitParams params;
            m_api.reset(QRhi::create(QRhi::D3D12, &params));
            break;
        }
#  endif
#endif
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
        case QSGRendererInterface::Vulkan: {
            m_vulkan.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
            m_vulkan.setApiVersion(QVersionNumber(1, 1));
            rhi::require(m_vulkan.create(), "Cannot create Vulkan instance");
            QRhiVulkanInitParams params;
            params.inst = &m_vulkan;
            m_api.reset(QRhi::create(QRhi::Vulkan, &params));
            break;
        }
#endif
        default:
            break;
        }
        rhi::require(bool(m_api), "Cannot create selected offscreen RHI backend");
    }
    QRhi *get() const
    {
        return m_api.get();
    }

private:
    std::unique_ptr<QOffscreenSurface> m_fallback;
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
    QVulkanInstance m_vulkan;
#endif
    std::unique_ptr<QRhi> m_api;
};
}
