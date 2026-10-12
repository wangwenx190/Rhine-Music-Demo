#pragma once
#include <QVariantMap>
namespace yalahoo {
enum class RenderBackend { Rhi };
// R10 chooses the destination; R10A-D supply it. Do not claim it is running yet.
struct RenderBackendContract {
    static constexpr RenderBackend selected = RenderBackend::Rhi;
    static QVariantMap fields() {
        return {{"selected", "rhi"}, {"implementationReady", false},
            {"activeImplementation", "quick3d-bootstrap"}, {"contractRevision", 1},
            {"contract", "docs/contracts/render-backend.json"}};
    }
};
}
