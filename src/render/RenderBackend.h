#pragma once
#include <QVariantMap>
namespace yalahoo {
enum class RenderBackend {
    Rhi
};
// The product has one renderer; Review-only Quick3D is a source comparison oracle.
struct RenderBackendContract
{
    static constexpr RenderBackend selected = RenderBackend::Rhi;
    static QVariantMap fields()
    {
        return { { "selected", "rhi" },
                 { "implementationReady", true },
                 { "activeImplementation", "rhi" },
                 { "contractRevision", 4 },
                 { "contract", "docs/contracts/render-backend.json" } };
    }
};
}
