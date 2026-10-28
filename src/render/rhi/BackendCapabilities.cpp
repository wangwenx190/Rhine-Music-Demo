#include "BackendCapabilities.h"
#include "Logging.h"
#include <QQuickWindow>
#include <memory>

using namespace Qt::StringLiterals;

#if (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)) || defined(Q_OS_WIN)
#  include "RhiApi.h"
#  include <rhi/qrhi_platform.h>
#endif
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#  include <QVulkanInstance>
#  include <QVersionNumber>
#endif

namespace {
#if (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)) || defined(Q_OS_WIN)
bool requiredFormats(const std::unique_ptr<QRhi> &api)
{
    if (!api)
        return false;
    const bool supported =
            api->isTextureFormatSupported(QRhiTexture::RGBA16F, QRhiTexture::RenderTarget)
            && api->isTextureFormatSupported(QRhiTexture::R16F, QRhiTexture::RenderTarget)
            && api->isTextureFormatSupported(QRhiTexture::D32F, QRhiTexture::RenderTarget);
    qCInfo(yalahoo::logRender) << "Graphics device probe" << "backend=" << api->backendName()
                               << "device=" << api->driverInfo().deviceName
                               << "requiredFormats=" << supported;
    return supported;
}
#endif
}

bool yalahoo::probeOptionalGraphicsBackend()
{
    const auto selected = QQuickWindow::graphicsApi();
    qCInfo(logRender) << "Graphics selection" << "requested=" << qgetenv("QSG_RHI_BACKEND")
                      << "selected=" << selected;
#if defined(Q_OS_WIN) && QT_CONFIG(direct3d12)
    if (selected == QSGRendererInterface::Direct3D12) {
        QRhiD3D12InitParams params;
        if (!requiredFormats(std::unique_ptr<QRhi>(QRhi::create(QRhi::D3D12, &params)))) {
            qCCritical(logRender)
                    << "Explicit D3D12 initialization or required formats unavailable";
            return false;
        }
    }
#endif
    if (selected != QSGRendererInterface::Vulkan)
        return true;
    bool supported = false;
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
    // Probe resources never cross threads. Qt owns the production instance/device.
    QVulkanInstance instance;
    instance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    instance.setApiVersion(QVersionNumber(1, 1));
    if (instance.create()) {
        QRhiVulkanInitParams params;
        params.inst = &instance;
        supported = requiredFormats(std::unique_ptr<QRhi>(QRhi::create(QRhi::Vulkan, &params)));
    } else {
        qCWarning(logRender) << "Vulkan instance creation failed"
                             << "error=" << instance.errorCode();
    }
#else
    qCWarning(logRender) << "Vulkan probe unavailable: Qt Vulkan feature or SDK headers missing";
#endif
    if (supported)
        return true;
#if defined(Q_OS_LINUX) && QT_CONFIG(opengl)
    if (qgetenv("QSG_RHI_BACKEND").isEmpty()) {
        qCWarning(logRender) << "Default Vulkan unavailable; selecting OpenGL";
        qputenv("QSG_RHI_BACKEND", "opengl"_ba);
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        return true;
    }
#endif
    qCCritical(logRender) << "Explicit Vulkan initialization or required formats unavailable;"
                             " install a Vulkan-enabled Qt Kit, SDK headers and runtime/driver"
                             " (MoltenVK on macOS)";
    return false;
}
