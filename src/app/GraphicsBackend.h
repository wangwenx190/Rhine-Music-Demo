#pragma once
#include <QString>
#include "Logging.h"
using Qt::StringLiterals::operator""_ba;
#include "../render/rhi/BackendCapabilities.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

namespace yalahoo {
inline bool selectGraphicsBackend()
{
    QQuickWindow::setTextRenderType(QQuickWindow::CurveTextRendering);
    // The application owns HDR policy; external force-HDR must not bypass
    // system enablement checks or disagree with the final color conversion.
    qunsetenv("QSG_RHI_HDR");
    const auto requested = qgetenv("QSG_RHI_BACKEND");
    if (requested == "vulkan"_ba) {
#if QT_CONFIG(vulkan)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        return true;
#else
        qCCritical(logRender) << "Vulkan requested but this Qt Kit has no Vulkan support";
        return false;
#endif
    }
    if (requested == "opengl"_ba) {
#if QT_CONFIG(opengl)
#  if defined(Q_OS_MACOS)
        // AppKit requires NSOpenGLContext::setView on the main thread.
        qputenv("QSG_RENDER_LOOP", "basic"_ba);
        auto format = QSurfaceFormat::defaultFormat();
        format.setVersion(4, 1);
        format.setProfile(QSurfaceFormat::CoreProfile);
        QSurfaceFormat::setDefaultFormat(format);
#  endif
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        return true;
#else
        qCCritical(logRender) << "OpenGL requested but this Qt Kit has no OpenGL support";
        return false;
#endif
    }
#if defined(Q_OS_WIN)
    if (requested == "d3d12"_ba) {
#  if QT_CONFIG(direct3d12)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
        return true;
#  else
        qCCritical(logRender) << "D3D12 requested but this Qt Kit has no D3D12 support";
        return false;
#  endif
    }
    if (requested.isEmpty() || requested == "d3d11"_ba) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
        return true;
    }
#elif defined(Q_OS_MACOS)
    if (requested.isEmpty() || requested == "metal"_ba) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
        return true;
    }
#elif defined(Q_OS_LINUX)
    if (requested.isEmpty()) {
        // The post-application probe handles an unavailable default Vulkan device.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        return true;
    }
#endif
    qCCritical(logRender) << "Unsupported graphics backend for this platform:" << requested;
    return false;
}
}
