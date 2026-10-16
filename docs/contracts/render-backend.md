# 渲染后端与业务边界

本文只定义后端选择及业务隔离；具体通道、资源和生产接线分别归[rhi-opaque.md](rhi-opaque.md)、[rhi-shell.md](rhi-shell.md)、[rhi-postprocess.md](rhi-postprocess.md)、[rhi-integration.md](rhi-integration.md)。机器接口见[render-backend.json](render-backend.json)。

- 生产唯一实现为QQuickRhiItem + QRhi/qsb；`SceneAdapter.renderBackendContract`发布`implementationReady=true`、`activeImplementation=rhi`。Qt Quick 3D仅用于独立Review，普通产品不打包第二套场景。
- SceneAdapter/QML/导航接口不随渲染实现变化。QRhi/GuiPrivate集中在`src/render/rhi/`并由最新Kit编译探测适配，不向core/media泄漏私有类型。
- 选择RHI的能力依据为源阴影投影/偏置/滤波、跨三壳体及副本的全局稳定排序、显式深度和resolve控制；禁止以历史图像分数或参数拟合决定框架。
- 保持源屏幕空间透射近似：音乐壳体FrontSide/transparent=false，独立不透明捕获不含透射壳体；DoubleSide专用背面流程不适用于本模型，不新增递归玻璃捕获。
- 修复边界为源码或实际产品确认的阴影、resolve、alpha边缘、排序/深度及质量缺陷；不改几何/IOR/主题/封面/曝光来掩盖错误。
