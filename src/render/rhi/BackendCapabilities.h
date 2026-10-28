// Queries actual native graphics capabilities before allocating production targets. Unsupported
// explicit requests produce actionable failure rather than altered quality.

#pragma once
#include <QSGRendererInterface>

namespace yalahoo {
constexpr const char *graphicsBackendName(QSGRendererInterface::GraphicsApi api) noexcept
{
    switch (api) {
    case QSGRendererInterface::Direct3D11:
        return "d3d11";
    case QSGRendererInterface::Direct3D12:
        return "d3d12";
    case QSGRendererInterface::Metal:
        return "metal";
    case QSGRendererInterface::Vulkan:
        return "vulkan";
    case QSGRendererInterface::OpenGL:
        return "opengl";
    default:
        return "unknown";
    }
}
// Called after QGuiApplication and before constructing any Quick window.
[[nodiscard]] bool probeOptionalGraphicsBackend();
}
