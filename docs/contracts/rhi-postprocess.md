# RHI 图集与后处理契约

`RhiItem::Input` 保持值快照边界；图集、实例缓冲、阴影和后处理资源只由 render 线程持有。产品接入仍属于 R10D，`implementationReady` 在本契约中不代表产品已替换。

算法/架构审计与性能结论按 [performance.md](performance.md) 执行。封面按来源cacheKey保存一次拥有像素的快照，重复同步复用；调用者发布后不经外部指针改写图像，内容变化发布新revision。图集在generation/lease/serial校验后才转换并detach一次，转换已有独立存储时不追加无效copy；队列拥有像素，GUI对象不跨同步边界。

uniform采用固定上界`uniformStride*(432*3+6)`，涵盖三个壳体逐draw、共享批次和小于8实体的普通路径；可见/实体数量变化不重建渲染图。CPU暂存复用，只上传已写入的aligned前缀，不逐帧清零/传输容量余量；容量与提交字节分别报告。深度键每实体预计算一次，保留原矩阵运算顺序和stable ID；112-byte实例记录直接填model/UV/origin light，不计算被丢弃的整份816-byte材质uniform。阴影绘制仍包含需要的非相机可见caster。

`updateDynamicBuffer`的指针重载在返回前复制载荷，因此CPU暂存可以跨帧复用；后端buffer轮换仍由QRhi负责，不能保存或改写其内部native storage。接口依据：[QRhiResourceUpdateBatch](https://doc.qt.io/qt-6/qrhiresourceupdatebatch.html#updateDynamicBuffer)。

性能Review通过frameSwapped的queued GUI回调提交一次下一帧状态，正常Qt事件循环与独立deadline timer控制时长，不用毫秒轮询、processEvents/msleep重复驱动呈现。原轮询协议证据保留原范围，新协议单列原始样本，不将采集驱动改善误记为渲染算法的FPS收益。

图集为固定页面和 256×256 RGBA8 tile，不生成页面 mip，不做各向异性跨格采样。`AtlasUpload` 携带页面 generation、递增 lease、槽位内递增 serial；过期 generation/lease 被拒绝，重复 serial 不再次上传，新 serial 替换同槽待处理结果。稳态每帧最多八 tile（2 MiB），初始化每批最多 64 tile（16 MiB）；剩余队列跨帧提交。窗口/渲染目标 resize 保留图集，页面重建必须换 generation。CPU 像素载荷、QRhi 请求统计和实际 D3D11 目标传输字节分别记录。

Review 的 `TransferAudit` 检查所选 Qt 的实际 D3D11 命令列表，按目标原生纹理筛选 `UpdateSubresource`，记录 destination box、row pitch 和 destination bytes。随后完成整页 GPU 读回，逐像素核对初始化、区域之外不变、稳定帧与还原。这是原生命令的目标写入量，不是物理总线流量；其他平台不伪造原生 D3D11 测量。

Frame std140 为 816 bytes：R10B 的前 752 bytes 保留，coverUv 在 752，paperSize 在 768，originLight0/1 在 784/800。仅依赖实体 origin/既有光场设置的源码函数在 CPU 每实体计算并缓存；局部几何、法向、视线、材质、光照、透射和滤波仍在 shader 执行。几何实例记录为 112 bytes，四列 model、tile UV 和两组 origin light；近到远纸面、阴影和几何覆盖共享几何批次，壳体按全局 depth、稳定 ID、surface 顺序提交。壳体彩色通道保留逐 draw 的全局顺序和各自材质 uniform，不跨表面重排。

生产顺序为阴影、线性不透明捕获、MSAA resolve/mips、独立壳体合成、SSAO、41 点 Bokeh、按设置 SMAA、一次 ACES/精确 sRGB。颜色及中间目标使用与源码 composer 对应的 RGBA16F；view normal/RGB24 depth 为 RGBA8，阴影比较使用 D32F 和源码同样的硬件双线性 comparison sampler。五点 IGN/Vogel、偏置、强度不变。Room PMREM 上传为 RGBA16F，逐值验证导出的 RGBA32F 数值本来就能被 half 精确表示；没有量化拟合。

不透明捕获不包含透射壳体。合成颜色附件独立，复用非重叠通道的硬件 depth/stencil，preserve/load 后复制已 resolve 的不透明输出；禁止采样正在写入的附件。壳体先写精确 front-surface depth，再用 Equal/read-only depth 着色，保留 source alpha=1、无 blending 和整体深度写入语义。阴影在 caster 矩阵/开关/目标变化时重绘；相机变化不重画不变的世界空间阴影。主灯贡献为零的表面跳过零贡献计算，同一片元复用同一阴影值。

SSAO 的独立几何覆盖包含完整纸面，原始 32/极高 64 个 seed=417 样本、半径 .18、min/max .001/.035、5×5 box、强度 .22 不变。核按同一 RGBA32F little-endian 资产加载到 std140 表，数值不变；16/32/64 样本各自使用固定索引展开，保留所有样本与原累加顺序，将半径乘法移到循环外；noise 保留 scalar xyz 复制与原采样方向。Bokeh 使用独立完整几何深度，AO 为全分辨率时共用同一覆盖结果，降分辨率时另建完整深度。关闭的通道不分配其目标。颜色 ping/pong 复用非重叠生命周期；每个 pass 有独立 uniform/SRB/目标，输入不与输出反馈。

SMAA 使用既有 Medium/area/search 和 gamma 2.2；framebuffer 的后端行方向转换与 lookup PNG 行序分开。几何顶点／片元签名使用连续 location 前缀，保留未使用输入占位，避免原生 stage linkage 改变材质参数位置。qsb 由最新同一 Kit 生成 SPIR-V、GLSL330/ES300、HLSL50、MSL12。

验收输入来自现有 R10/R10B/R09 fixture，不读取用户曲库。R10B 的 61 组 pre-stage/five ROI/shadow/order/isolation/alpha/source normal/depth 回归与 R10 的 50 组最终输出分开；残差逐 sourceIndex/ROI 保存，不能用平均值消除残差。432 实体检查覆盖 tile 更新/还原、倒序、resize、generation、九 tile 队列跨帧及 1920×1080 原始画质 120 秒真实呈现。CPU 提交、GPU timestamp 和 frameSwapped 间隔分别记录；timestamp 诊断的开销不混入普通呈现门槛，无法取得 timestamp 时记为不可用。

所有窗口遵循 [测试窗口契约](test-window-policy.md)。本平台运行和跨平台静态审计结果以对应 `verification/` 产物为准；macOS/Linux 结论必须为“静态审计通过，实机待用户验证”。

R10C历史Release复建工具为`tools/build/verify-r10c.ps1`，只在对应验收版本的隔离checkout重建，不作为维护后源码覆盖历史目录的默认入口。原Windows产物在`verification/windows/rhi-postprocess/`，pre-stage/final图片在`docs/media/rhi-postprocess/`；来源与算法适配hash见`resources/manifests/rhi-postprocess.json`，历史证据不回写。维护后源码按`performance.md`执行Release格式/CTest/qmllint、`rhi-postprocess-gpu`及逐项静态审计，新证据在`verification/windows/performance-review/`及对应静态目录，后续维护继续使用独立目录。性能门槛使用`QSG_NO_VSYNC=1`的真实frameSwapped间隔，仅为未限速原生渲染吞吐；系统compositor/vsync节奏与完整产品交互仍需R10D/R28实际验证。
