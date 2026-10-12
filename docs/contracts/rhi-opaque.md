# RHI 场景快照、目标与不透明通道

`YalahooRhi` 的 `src/render/rhi/RhiItem` 实现真实 QQuickRhiItem/Renderer；R10D 前由 Review 的 `rhi-audit` / `rhi-shell-audit` 使用，产品继续运行现有 Quick3D bootstrap。`implementationReady=false` 表示完整产品替换尚未完成。

GUI 调用 `submit(Input, targetSize)`；Input 含 SceneAdapter 的不可变 SceneFrame、实体矩阵、纸面图像及 SelectionLighting 的值快照。targetSize 由调用者的 RenderQuality 有效尺寸提供；Review 明确使用 DPR1、100% 尺寸。Renderer 在 synchronize 拷贝值、独立持有图像，render 不访问 GUI 对象。Renderer 独占 QRhi 资源；读回按 request 标识，回调持有独立结果存储，GUI 使用受保护的排队投递和过期 request 检查。目标重建先释放 pipeline/binding，再释放 target/pass/attachments；读回存活到 QRhi 完成，销毁前 finish。30 次连续替换与恢复由实机检查覆盖。

RHI 私有头和所有 QRhi 类型仅在 `src/render/rhi/` 与 CMake 能力探针中使用。顶层从自动选出的最新 Kit 查找 GuiPrivate；每次配置重新编译探针，不缓存前一个 Kit 的通过状态，不固定 Qt minor/patch。平台选择在 Review 入口分别为 Windows D3D11、macOS Metal、Linux OpenGL；产品仍使用自己的现有平台入口。

本契约的不透明基础为 shadow → opaque-color/normal/depth → Review output；当前磨砂扩展的 capture/resolve/mips → sorted-shell、稳定身份和真实 MSAA 边界按 [rhi-shell.md](rhi-shell.md) 执行。R10A 的独立通道对照仍是单采样，不把它当作完整后处理验收；AO/DOF/AA 与局部图集上传由 R10C 接入。

阴影目标为 RGBA32F canonical-depth + 独立硬件深度附件，颜色、法线、RGB24 线性深度为三个独立 RGBA32F MRT + 硬件深度附件。所有目标在硬件尺寸和 8,294,400 像素约束下创建，失败明确报错；阴影使用 RenderQuality 的 1024/2048/4096 选择，不悄悄改变用户设置。Renderer 为源 FrontSide 的 shadow pass 使用 Front cull，只有 Optical_Diffuser 投射阴影。阴影 bounds [-16,16,-15,15,.1,45]，光源 (-6,14,-5) 指向原点；接收侧世界法向偏移 .035、canonical-depth bias -.0003、strength .32、radius 4。

冻结源码请求 PCFSoftShadowMap；锁定 Three 0.183.2 的 WebGLShadowMap 在执行时转换为 PCFShadowMap。实际 shader 使用 IGN 旋转的 5 点 Vogel disk，每点硬件双线性 depth compare。本实现通过 nearest float target 的四次比较再插值取得同一有效滤波。canonical 光投影保持源 OpenGL 约定；实际 raster 矩阵用 QRhi::clipSpaceCorrMatrix 修正。接收 UV、Vogel offset 与 IGN 的 gl_FragCoord 按 isYUpInFramebuffer 转换，不能只翻转中心 UV。clip-space 能力来自 QRhi，不假定 D3D/Metal/GL 深度范围一致。Three 滤波源码与 MIT 署名继续由 resources/licenses/three-LICENSE、materials-NOTICE.md 保存。

三组壳体直接从已有 GLB 的 POSITION/NORMAL/TEXCOORD/indices 读取，复用 R03 有界解析器；没有重建模型或轴翻转。源节点变换为 identity，实体矩阵只应用一次。纸面 3.06 方形、(.17,1.85,.027)，contain + 1/64 透明留白、alphaTest .025、像素中心 clamp 保持 R07。源 RGBA8 是 sRGB，经一次解码后，沿 R07 的 musicPrintExposure、musicIrradiance 与固定 Room PMREM 输出线性 diffuse，不加白、不自发光、不做纸面 ACES。纸面透明处不写任何颜色/法线/深度。`tools/assets/prepare-rhi-shaders.ps1` 从 R07 准备共享 math，只有 uniform 名适配，不复制未接入的壳体算法。

R10A 原始 frame.glsl std140 block 为 560 bytes；当前 R10B 保留其前缀并扩展为 752 bytes，C++ static_assert 核对大小与偏移，每 draw 采用设备对齐的动态 offset。vertex 属性为位置/法向/UV 的 32-byte stride，shader 的每个 binding/location 显式声明。原六个基础 qsb 与新增 shell qsb 均编译 SPIR-V、GLSL330、GLSL ES300、HLSL50、MSL12；变体生成来自同一已选 Kit，不表示 Metal/GL GPU 已执行。

`tools/build/verify-r10a.ps1` 只运行 Release，保存 Windows runtime、shader reflection/变体、源码/资产 hash、成对 PNG、完整 Review/production CTest 与 qmllint；`tools/audit/audit-r10a.ps1` 检查证据闭包并输出 macOS/Linux 逐项静态矩阵。Windows 36 组当前 Quick3D/RHI 对照，三种封面、双主题、前/低视角覆盖纸面/法线/深度；已知盒盖平面及四角位置独立验收。九种必测内容尺寸、恢复旧尺寸、30 次未等待读回的 resize 替换共同验收目标所有权。阴影使用 R10 的 9–20 同输入冻结 witness：只比较两者共有的白色 receiver，不将透射壳体掩码混入不透明轮次；全壳体五 ROI、shadow reception、捕获、MSAA 边界与排序仍由 R10B 验收。

R10A 历史复建入口固定六个基础 shader，按对应验收提交重建；当前源树以 `verify-r10b.ps1` 复建七个 shader 并同时执行不透明基础回归，历史 hash 不回写。R10A 的 readback 数字与 RGBA32F/整图 cover 提交只用于通道验证，不作为图集传输字节或最终性能预算证据。后续 R10C 必须实测 tile destination regions/bytes；不得从 R10A 的 CPU 数据量推导 GPU 上传预算。

接口语义依据：[QQuickRhiItem](https://doc.qt.io/qt-6/qquickrhiitem.html)、[QRhi](https://doc.qt.io/qt-6/qrhi.html)、[CustomMaterial](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)。升级后以最新 Kit 头文件和实际探针为准。
