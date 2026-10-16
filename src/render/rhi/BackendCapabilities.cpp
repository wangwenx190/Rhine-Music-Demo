#include "BackendCapabilities.h"
#include <QQuickWindow>
#include <QDebug>

#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#  include "RhiApi.h"
#  include <rhi/qrhi_platform.h>
#  include <QVulkanInstance>
#  include <memory>
#endif

void yalahoo::probeOptionalGraphicsBackend()
{
#if defined(Q_OS_LINUX)
    if (qgetenv("QSG_RHI_BACKEND") != "vulkan")
        return;
    bool supported = false;
#  if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
    // This temporary device proves initialization and required render formats. Qt
    // owns the production instance/device; no probe resource crosses threads.
    QVulkanInstance instance;
    instance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    instance.setApiVersion(QVersionNumber(1, 1));
    if (instance.create()) {
        QRhiVulkanInitParams params;
        params.inst = &instance;
        const std::unique_ptr<QRhi> api(QRhi::create(QRhi::Vulkan, &params));
        supported = api
                && api->isTextureFormatSupported(QRhiTexture::RGBA16F, QRhiTexture::RenderTarget)
                && api->isTextureFormatSupported(QRhiTexture::R16F, QRhiTexture::RenderTarget)
                && api->isTextureFormatSupported(QRhiTexture::D32F, QRhiTexture::RenderTarget);
    }
#  endif
    if (!supported) {
        qWarning("Vulkan initialization or required formats unavailable; using OpenGL");
        qputenv("QSG_RHI_BACKEND", "opengl");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    }
#endif
}
