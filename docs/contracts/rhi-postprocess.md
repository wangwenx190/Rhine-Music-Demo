# RHI 图集与后处理契约

`RhiItem::Input` 保持值快照边界；图集、实例缓冲、阴影和后处理资源只由 render 线程持有。产品接入仍属于 R10D，`implementationReady` 在本契约中不代表产品已替换。

图集为固定页面和 256×256 RGBA8 tile，不生成页面 mip，不做各向异性跨格采样。`AtlasUpload` 携带页面 generation、递增 lease、槽位内递增 serial；过期 generation/lease 被拒绝，重复 serial 不再次上传，新 serial 替换同槽待处理结果。稳态每帧最多八 tile（2 MiB），初始化每批最多 64 tile（16 MiB）；剩余队列跨帧提交。窗口/渲染目标 resize 保留图集，页面重建必须换 generation。CPU 像素载荷、QRhi 请求统计和实际 D3D11 目标传输字节分别记录。

Review 的 `TransferAudit` 检查所选 Qt 的实际 D3D11 命令列表，按目标原生纹理筛选 `UpdateSubresource`，记录 destination box、row pitch 和 destination bytes。随后完成整页 GPU 读回，逐像素核对初始化、区域之外不变、稳定帧与还原。这是原生命令的目标写入量，不是物理总线流量；其他平台不伪造原生 D3D11 测量。

Frame std140 为 816 bytes：R10B 的前 752 bytes 保留，coverUv 在 752，paperSize 在 768，originLight0/1 在 784/800。仅依赖实体 origin/既有光场设置的源码函数在 CPU 每实体计算并缓存；局部几何、法向、视线、材质、光照、透射和滤波仍在 shader 执行。几何实例记录为 112 bytes，四列 model、tile UV 和两组 origin light；近到远纸面、阴影和几何覆盖共享几何批次，壳体按全局 depth、稳定 ID、surface 顺序提交。壳体彩色通道保留逐 draw 的全局顺序和各自材质 uniform，不跨表面重排。

生产顺序为阴影、线性不透明捕获、MSAA resolve/mips、独立壳体合成、SSAO、41 点 Bokeh、按设置 SMAA、一次 ACES/精确 sRGB。颜色及中间目标使用与源码 composer 对应的 RGBA16F；view normal/RGB24 depth 为 RGBA8，阴影比较使用 D32F 和源码同样的硬件双线性 comparison sampler。五点 IGN/Vogel、偏置、强度不变。Room PMREM 上传为 RGBA16F，逐值验证导出的 RGBA32F 数值本来就能被 half 精确表示；没有量化拟合。

不透明捕获不包含透射壳体。合成颜色附件独立，复用非重叠通道的硬件 depth/stencil，preserve/load 后复制已 resolve 的不透明输出；禁止采样正在写入的附件。壳体先写精确 front-surface depth，再用 Equal/read-only depth 着色，保留 source alpha=1、无 blending 和整体深度写入语义。阴影在 caster 矩阵/开关/目标变化时重绘；相机变化不重画不变的世界空间阴影。主灯贡献为零的表面跳过零贡献计算，同一片元复用同一阴影值。

SSAO 的独立几何覆盖包含完整纸面，原始 32/极高 64 个 seed=417 样本、半径 .18、min/max .001/.035、5×5 box、强度 .22 不变。核按同一 RGBA32F little-endian 资产加载到 std140 表，数值不变；16/32/64 样本各自使用固定索引展开，保留所有样本与原累加顺序，将半径乘法移到循环外；noise 保留 scalar xyz 复制与原采样方向。Bokeh 使用独立完整几何深度，AO 为全分辨率时共用同一覆盖结果，降分辨率时另建完整深度。关闭的通道不分配其目标。颜色 ping/pong 复用非重叠生命周期；每个 pass 有独立 uniform/SRB/目标，输入不与输出反馈。

SMAA 使用既有 Medium/area/search 和 gamma 2.2；framebuffer 的后端行方向转换与 lookup PNG 行序分开。几何顶点／片元签名使用连续 location 前缀，保留未使用输入占位，避免原生 stage linkage 改变材质参数位置。qsb 由最新同一 Kit 生成 SPIR-V、GLSL330/ES300、HLSL50、MSL12。

验收输入来自现有 R10/R10B/R09 fixture，不读取用户曲库。R10B 的 61 组 pre-stage/five ROI/shadow/order/isolation/alpha/source normal/depth 回归与 R10 的 50 组最终输出分开；残差逐 sourceIndex/ROI 保存，不能用平均值消除残差。432 实体检查覆盖 tile 更新/还原、倒序、resize、generation、九 tile 队列跨帧及 1920×1080 原始画质 120 秒真实呈现。CPU 提交、GPU timestamp 和 frameSwapped 间隔分别记录；timestamp 诊断的开销不混入普通呈现门槛，无法取得 timestamp 时记为不可用。

所有窗口遵循 [测试窗口契约](test-window-policy.md)。本平台运行和跨平台静态审计结果以对应 `verification/` 产物为准；macOS/Linux 结论必须为“静态审计通过，实机待用户验证”。

当前完整 Release 复建使用 `tools/build/verify-r10c.ps1`，执行 Review/production CTest、格式、qmllint 和 24 个同 Kit shader 变体，再生成静态矩阵与 evidence-index。Windows 产物在 `verification/windows/rhi-postprocess/`，当前 pre-stage/final 图片在 `docs/media/rhi-postprocess/`。资源来源与算法适配 hash 见 `resources/manifests/rhi-postprocess.json`；R10A/R10B 历史证据不回写。性能门槛使用 `QSG_NO_VSYNC=1` 的真实 frameSwapped 间隔，明确限定为未限速原生渲染吞吐；系统 compositor/vsync 节奏与最终产品交互仍需 R10D 和用户实际环境验证。
