# RHI 磨砂捕获、壳体与稳定排序

`YalahooRhi` 的 `RhiItem` 在不可变 Input / SceneFrame 边界上实现 shadow → opaque MRT → resolve/mips → sorted-shell MRT → Review output。生产场景已使用同一 RHI 实现，接入/测量和研究隔离见 [rhi-integration.md](rhi-integration.md)。当前图集、AO、景深、SMAA、目标格式、共享硬件深度和 816-byte uniform 按 [rhi-postprocess.md](rhi-postprocess.md) 执行；以下 752-byte/RGBA32F 描述保留 R10B 历史验收边界。本契约补充 [rhi-opaque.md](rhi-opaque.md)，不把本轮 Review 窗口当作完整播放器。

所有 Review、运行与证据重建遵守 [测试窗口契约](test-window-policy.md)：非视觉检查隐藏，必须 exposed 的真实 GPU 窗口在首次 show 前置底且不接受焦点，不提高 Z 顺序、不激活用户桌面；报告包含实际 windowPolicy。普通播放器默认窗口不受测试策略影响。

不透明目标只绘制背景、独立纸面和诊断 receiver。捕获颜色、法线、RGB24 线性深度分别 resolve，壳体只采样已 resolve 的不透明颜色 mip 链。合成目标独立拥有颜色/法线/深度和硬件深度附件，先重绘同一不透明内容，再绘制壳体；它不进入透射捕获。两个目标形成明确的读/写边界，禁止反馈或递归玻璃捕获。重绘保留硬件深度及 alphaTest 语义，不从量化颜色猜深度。MSAA attachment 的 resolve 在 endPass 后完成，generateMips 单独提交，然后才能开始壳体 pass；规则依据 [Qt QRhiColorAttachment](https://doc.qt.io/qt-6/qrhicolorattachment.html) 和 [QRhiResourceUpdateBatch](https://doc.qt.io/qt-6/qrhiresourceupdatebatch.html)。

`captureSamples` 是调用者提供的真实采样数，设备不支持时明确失败。R10B 的材质输入使用 4×（冻结 Three transmission pass 至少 4×），R10A 独立通道仍使用 1×。RGBA32F MRT 的 MSAA attachments 只用于 raster，采样和读回只使用单采样 resolve 纹理。R10D 将 RenderQuality 的有效大小与产品画质规则接入，不保存更低设置。qsb 同 Kit 生成 SPIR-V、GLSL330/ES300、HLSL50、MSL12；实际 GPU 验证为 Windows D3D11，其他系统仅静态审计。

壳体复用源 GLB 三批次。模型矩阵只应用一次，法向用 inverse transpose；局部纸面 LOD 使用原 `.17/1.85/1.53`、`.015/.15` 和 `.16`。IOR 固定 1.46，源 tint/attenuation、transmission/roughness/thickness/distance、clearcoat 与 clarity 由 `ShellMaterial` 计算；`rhi-material-parameters` 对冻结 R10 的全部 210 个表面参数核对。R19C1移除了R07的ROI拟合系数，transport/coat直接按冻结源码提交，逐实体lid clarity也不乘系数；历史R10B manifest的fixedOptics仅表示当时实现，不能覆盖当前源码。原 GGX/DFG 多次散射、LTC、PMREM、bicubic、ACES/sRGB 与世界空间音乐光场通过 `prepare-rhi-shell.py` 适配为显式 binding。许可证/来源在 `resources/manifests/rhi-shell.json`、`resources/licenses/materials-NOTICE.md`。

纸面保持原 3.06 几何、中心与 UV 映射。实际印刷 pass 在顶点阶段按同一 contain 和 1/64 inset 裁出可见矩形，同时反向映射 UV，使用 centroid 插值，使真实几何 MSAA 覆盖印刷边缘；不再仅在片元阶段丢弃矩形外部而绕过 MSAA。图片自身透明区仍按 .025 alphaTest 丢弃。上传预乘 RGBA8，在 shader 滤波后恢复未预乘颜色，再执行 sRGB 解码，避免透明黑 texel 混入印刷形成黑边。几何诊断保持完整原平面，封面中心、采样比例、颜色和清晰度不变，不增加整体模糊。

壳体只绘制正面：Back cull、CCW、LessOrEqual、depthTest/depthWrite=true、blend=false、alpha=1，与冻结 `FrontSide / transparent=false / opacity=1` 一致。阴影只由 Optical_Diffuser 投射，纸面和壳体接收同一 R10A canonical 投影、偏置、强度与 5 点 Vogel/双线性比较；只衰减方向灯的 diffuse/specular/coat 项，不错误地压暗环境或整幅透射。

每个壳体批次使用源包围球中心（由同一源顶点包围盒求中心）在固定相机中的深度，全局从远到近绘制，不能分别在三个模型内部排序。深度相同先比较稳定 `entityIds`，再比较表面索引。不透明纸面从近到远排序，并使用同一稳定 ID 处理同深度；否则它的捕获仍会依赖提交顺序。多实体必须携带与矩阵对应的唯一 ID，GUI submit 在同步前拒绝缺失/错位/重复 ID。ID 随排列、抽取、归位保持不变；不能用当前数组下标代替真实逻辑身份。

Frame std140 扩展到 752 bytes：前 560 bytes 保留原块，nativeVP 在 560，camera 在 624，scatter 在 736。C++ static_assert、设备动态 offset 与 qsb reflection 共同核对。capture / composite 的三个颜色附件独立，target reset 先释放 pipelines/bindings，再释放 RT/pass/depth/MSAA/resolve 纹理。读回拥有独立结果存储，销毁和 Kit/RHI 替换前 finish；GUI 回调 QPointer 排队，不保留 GUI 对象到 render。float 资产从小端转为本机字节序；所有资源经 qrc 解析，无系统盘或当前目录依赖。

R10B历史复建入口`tools/build/verify-r10b.ps1 -Python <本机Python>`，R10C为`tools/build/verify-r10c.ps1`，均需对应验收提交并在隔离checkout保存输出，不能用维护后源码覆盖历史证据。当前维护验证和来源索引按`performance.md`及`verification/windows/performance-review/`执行；开发依赖不进入产品运行路径。冻结源码捕获使用已有Node、锁定Web依赖与本机浏览器，只访问隔离本地harness并阻止外网请求；相同源/fixture/image hash可复用已有对照。先format/check-format，再执行受影响的Release Review CTest/qmllint，最后关闭Review执行生产Release构建和CTest。

`tests/fixtures/rhi-shell/visual.json` 保留 R10 `sourceIndex`、源参数和独立投影；61 状态覆盖原 2、4–57、60–65，三种封面、双主题、前/低角度、邻盒、抽取中间位置、三档阴影与九实例。普通场景采集源码 pre-AO/Bokeh/SMAA 的线性输出，三个 direct-material 例外继续按原单盒路径采集；R10B Quick3D 与 RHI 使用完全相同输入和颜色域比较。原 R10 0/1/3 的画质、Bokeh 和 320px SSAA 诊断已纳入当前 R10C final-output 重放；R10B 历史证据仍只代表 pre-stage。原 58/59 另按真实 GPU 法线/深度和原 source 图检查，RGB24 线性深度还原为 canonical OpenGL depth 后对比；R10A 的已知平面、角点、九尺寸与读回生命周期也执行回归。

五个 ROI 全部计分，MAE/SSIM/edge 和等权损失不屏蔽边缘、侧脊或背景。退出使用迁移计划的“全部材质 ROI 通过或残差进一步缩小”分支；整体损失降低不宣称每个 ROI 都达到 .97/6。逐项残差保留于 `verification/windows/rhi-shell/release-runtime.json`，源/Quick3D/RHI/差分与 4× 局部图位于 `docs/media/rhi-shell/`。较窄 rim 的 SSIM 无完整窗口时明确为 null，仍保留 MAE 和轮廓，不删除该区域。R10D 成对重放时必须复验这些具体 sourceIndex/ROI 和 `verification/windows/rhi-postprocess/` 的当前 pre-stage/final 残差，不能用整体平均数覆盖。

以下为R10B历史GPU门槛，尚待R19C2拆出非像素功能检查，不在当前执行：两场景开启/关闭壳体的不透明捕获逐像素一致；六场景倒序提交颜色/深度完全一致；相同深度使用固定 ID、重复 ID 拒绝；合成透明留白的 1×/4× resolve 只改变边缘覆盖，中心印刷不变且透明区不写法线/深度。阴影 on/off 用壳体实际深度掩码统计有效像素。R10B 历史图集传输字节仍未知；当前 R10C 的上传预算与性能证据独立保存于 `verification/windows/rhi-postprocess/`。
