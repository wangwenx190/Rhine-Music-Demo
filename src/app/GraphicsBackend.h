#pragma once
#include <QString>
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
    if (requested == "opengl"_ba) {
#if defined(Q_OS_MACOS)
        // Current AppKit requires NSOpenGLContext::setView on the main thread.
        // The Qt basic loop keeps the visible OpenGL drawable on that thread.
        qputenv("QSG_RENDER_LOOP", "basic"_ba);
        // Cocoa's legacy default cannot consume the Kit's GLSL 150 shaders.
        // Select the core context before QGuiApplication creates any shared contexts.
        auto format = QSurfaceFormat::defaultFormat();
        format.setVersion(4, 1);
        format.setProfile(QSurfaceFormat::CoreProfile);
        QSurfaceFormat::setDefaultFormat(format);
#endif
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        return true;
    }
#if defined(Q_OS_WIN)
    if (!requested.isEmpty() && requested != "d3d11"_ba)
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    if (!requested.isEmpty() && requested != "metal"_ba)
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#elif defined(Q_OS_LINUX)
    if (requested.isEmpty() || requested == "vulkan"_ba) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        return true;
    }
    if (!requested.isEmpty() && requested != "opengl"_ba)
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    return true;
}
}
