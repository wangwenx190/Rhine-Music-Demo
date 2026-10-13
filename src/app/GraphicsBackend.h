#pragma once
#include <QQuickWindow>
#include <QSGRendererInterface>

namespace yalahoo {
inline bool selectGraphicsBackend()
{
    const auto requested = qgetenv("QSG_RHI_BACKEND");
    if (requested == "opengl") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        return true;
    }
#if defined(Q_OS_WIN)
    if (!requested.isEmpty() && requested != "d3d11")
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    if (!requested.isEmpty() && requested != "metal")
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    if (!requested.isEmpty() && requested != "opengl")
        return false;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    return true;
}
}
