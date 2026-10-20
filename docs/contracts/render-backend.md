# 渲染后端与业务边界

本文只定义后端选择及业务隔离；具体通道、资源和生产接线分别归[rhi-opaque.md](rhi-opaque.md)、[rhi-shell.md](rhi-shell.md)、[rhi-postprocess.md](rhi-postprocess.md)、[rhi-integration.md](rhi-integration.md)。机器接口见[render-backend.json](render-backend.json)。

- 生产唯一实现为QQuickRhiItem + QRhi/qsb；`SceneAdapter.renderBackendContract`发布`implementationReady=true`、`activeImplementation=rhi`。Qt Quick 3D仅用于独立Review，普通产品不打包第二套场景。
- SceneAdapter/QML/导航接口不随渲染实现变化。QRhi/GuiPrivate集中在`src/render/rhi/`并由最新Kit编译探测适配，不向core/media泄漏私有类型。
- 选择RHI的能力依据为源阴影投影/偏置/滤波、跨三壳体及副本的全局稳定排序、显式深度和resolve控制；禁止以历史图像分数或参数拟合决定框架。
- 保持源屏幕空间透射近似：音乐壳体FrontSide/transparent=false，独立不透明捕获不含透射壳体；DoubleSide专用背面流程不适用于本模型，不新增递归玻璃捕获。
- 修复边界为源码或实际产品确认的阴影、resolve、alpha边缘、排序/深度及质量缺陷；不改几何/IOR/主题/封面/曝光来掩盖错误。

## 生产API选择

`GraphicsBackend.h`在QGuiApplication前选择API。平台默认值为Windows D3D11、macOS Metal、Linux Vulkan；默认值不限制可选后端。`QSG_RHI_BACKEND=opengl`和`vulkan`为三平台通用选项，Windows另支持`d3d11`/`d3d12`，macOS另支持`metal`。选择依赖当前Qt Kit的对应编译特性；未知名称、平台不适用或Kit未编译的显式请求以明确日志和非零退出失败，不静默替换API。macOS OpenGL的CoreProfile/basic适配沿平台契约。

`probeOptionalGraphicsBackend`在QGuiApplication后、任何Quick窗口前对Vulkan及Windows显式D3D12一次初始化临时QRhi并核对生产RGBA16F/R16F/D32F附件能力。Vulkan使用临时QVulkanInstance，要求Qt Vulkan特性、SDK开发头及运行时/驱动；macOS可使用MoltenVK，但仅安装MoltenVK不能给未启用Vulkan的Qt构建添加该特性。显式请求初始化或格式失败时退出；仅Linux未显式指定时允许默认Vulkan失败后提示并更新实际API为OpenGL。

探测资源在GUI启动阶段释放，生产实例/设备仍由Qt场景图拥有，不跨线程传递探测资源。生产渲染器记录实际QRhi后端和设备，验证报告按实际API与选择值核对。初始化不证明surface/swapchain、呈现或完整画质；D3D11/Metal及实际启用的D3D12/Vulkan动态检查归R36，OpenGL归R35。平台部署与运行操作仅在各平台文档维护。后端选择的无GPU检查为Release `graphics-backend`，不创建上下文或窗口。

依据：[Qt QRhi后端与能力](https://doc.qt.io/qt-6/qrhi.html)、[Qt Quick显式D3D12选择](https://www.qt.io/blog/direct3d-12-support-in-qt-6.6)。

HDR/SDR自动输出与默认绘制质量见[display-output.md](display-output.md)。
