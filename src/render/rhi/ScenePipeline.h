// Builds compatible geometry pipelines from explicit pass and instancing roles.
// Returned pipelines are owned by the frame resource coordinator and borrow its targets/SRBs.

#pragma once
#include "RenderTarget.h"
#include "RhiItem.h"

namespace yalahoo::rhi {
enum class ScenePass {
    Opaque,
    Shadow,
    Shell
};
enum class InstanceMode {
    Single,
    Instanced
};
std::unique_ptr<QRhiGraphicsPipeline>
createScenePipeline(QRhi *api, const Input &input, const Target &target,
                    QRhiShaderResourceBindings *bindings, ScenePass pass,
                    InstanceMode instances = InstanceMode::Single);
}
