# 详情抽取、换片与拖动契约

源码依据为冻结 `music-camera.ts` 的 MusicPlacementMotion/MusicPresentation、`scene.ts` 的 setMode/switchMusicAlbum/select/bindPointer/update，以及 music-app.ts 的实际 presentation ports。冻结身份与清理后的 Git 读取规则按 web-baseline.md；测试 oracle 由 `tools/review/export-detail-golden.mjs` 直接执行冻结类及生产旋转表达式生成，不修改 Web 基线。

`DetailMotion` 管理 archive/placing/presented/returning-center/returning-array。唯一 FrameClock 按 MotionController → SelectionController → SceneAdapter → PresentationController 顺序交付已经缩放的 motionDt。1.45/1.05 秒的五次轨迹保留 x/v/a 连续性，同目标不重启；其钳制进度同时驱动 .9→3.47 的抽取、相机 yaw/elevation、取景跨度与瞄准点，实际相机继续使用源码 rate=9 阻尼。生产只通过 requestDetail 发意图；setDetail 保留给已有固定状态/几何探针，不作为生产开关。

DetailPorts 是产品与单元测试共用的接线。退 browse 后才请求抽取；placement、相机、四条 rail/column/shoulder/lane、真实列、lift/rotation 都落稳并保持 .08 秒后才能通过场景门禁，再等当前 layoutRevision。详情换片先退旧字，保留详情构图，rail/新 lift/旧副本用 rate=9 同时运行，然后进新字。详情期间不启动浏览五次导航轨迹或选择波纹。相机轨道消费实际 rail/column 速度；竖屏/真实列保留源码的行速度约束。归位/浏览保留各自的 4.2/4.5 lift rates，不把详情 rate=9 施加到普通返回。

ReturningCasePool 最多三个副本，加一个选中实体。每个副本保留 cell、albumId、objectId、lift 的显示导数、yaw 和可选世界高度；旋转归零前冻结世界高度，之后才下降。同列相邻五行内的旋转归位副本未对齐时，新盒等待净空。反向选择回收同一 cell 的实际 objectId 和显示状态，不重新生成封面。池满时 PresentationController 只保留最后一个 pendingSelection，等待完成回收；不驱逐未完成副本、不提前清掉目标或准备错误详情。到达原 lift/rotation 退出阈值后先解除副本所有权，再恢复阵列实体，避免空缺一帧。

PointerDragController 仅保存一个指针和最新 yaw 目标。横向每逻辑像素 .004 弧度、范围 ±.8；竖向不改变角度；按 `1-exp(-2.8*motionDt)` 跟随，释放保留目标，回正使用冻结 returnStep。减少动态直接走相同终点/完成路径。生产 SceneItem 额外要求 detailEnabled 和实际净空；退字、换片、失去抓取、隐藏/失焦、多指和取消都撤销手势。点击只在释放且全程未越过 6px 时提交。hover 使用同一相机 pointer 偏移；实体矩阵同时用于真实渲染、光场和逆射线拾取，不在 QML 做假旋转。

CaseClarity 直接移植冻结 decryption.ts 的材质状态：原时间 34.12–39.56、固定参考倍率 1.5、六个 REVEAL 节点的单调 Hermite 曲线、ready 后起计、重新进入时保留显示值、离开/副本按 exp(-9*motionDt) 回雾。1.5 是原片时间映射，不再次缩放界面速度；静态导数在编译期缓存。副本独立保存 clarity，反向回收同一状态。SceneFrame 交付各实体的值，生产阵列基准 clarity=0；盖面仅替换既有材质 uniform 中的粗糙度，其余光学公式、校准、PSO/纹理和资源键不变。固定状态 Review 入口保留原全局 clarity，用于既有材质 oracle；该兼容输入不作为生产材质时序。

## 资源与测量边界

运动只发布变换和必要光场，不重建材质、图集或不变附件。四个清晰封面 CPU 租约最多 16MiB，四份 256px 分块暂存另最多 16MiB；它们不等于整个封面系统内存。既有解码并发、在途预留、完成 LRU、generation 取消和上传队列独立记账。固定 GPU 图集 5632×6656 RGBA8 的载荷是 149,946,368 bytes，清晰区已包含其中，不再当成四张额外 GPU 纹理相加。

产品回放分别记录请求交付到场景及 CPU 封面 ready、实际 Qt submission、atlasUploadBytes、resourceGeneration/postprocessRevision、CPU 租约/分块暂存、ResourceInventory 附件与保守重建重叠。motion 状态不能触发目标重建，普通上传批次上界 2MiB。GPU time 是 QRhi 已完成帧的可用观测，可能对应先前帧；不可用为 null。descriptor 字节是估算，不包含驱动 padding/PSO/SRB 和窗口/compositor，不冒充实测显存。局部回放不替代 R28 的 120 秒正常 VSYNC/产品预算，也不提供 input-to-photon。

## Release 复现

```sh
cmake --preset qt-release
cmake --build --preset qt-release --target format
cmake --build --preset qt-release
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
ctest --preset qt-release --output-on-failure
node tools/review/export-detail-golden.mjs
```

`detail-motion` 比较 598 帧冻结轨迹、50 个旋转检查点及 1,200 帧材质状态；包含 20/30/60/120fps、1/120 秒逐帧打断、反向/改速/减少动态，位置/速度/加速度门槛各 1e-6。行为测试覆盖连续三次换片、反向物理所有权回收、池满拒绝/最后目标等待、途中返回/搜索替换、拖动中换片、隐藏/恢复、库替换、九尺寸的实体 transform 拾取和真实列边界。来源 hash 由 audit-detail.py 对清理映射后的冻结 Git 内容核验。

可见产品回放仍须本轮授权；未授权时不得打开前台入口。macOS 默认隐藏策略不能为方便而解除。授权轮次的入口如下，其他平台使用对应实际可用后端；输出目录必须全新：

```sh
python3 tools/build/verify-detail.py --backend metal --dpr 1 --runs 3 --foreground --output verification/macos/detail/new-metal
python3 tools/build/verify-detail.py --backend opengl --dpr 2 --runs 3 --foreground --night --output verification/macos/detail/new-gl
python3 tools/audit/audit-detail.py --report verification/macos/detail/new-metal/run-1/report.json
```

每次用真实 App.qml、Detail/TrackList、匿名 qrc 演示库和实际 FrameAnimation：打开、鼠标拖动、点击详情上一张/下一张、三次换片及反向、退字期连选、换片中返回/搜索路由、.25→3 倍速度、减少动态、额外三十次切片、Escape、十次产品 hide/release/destroy/create/show，以及十次完整 App/QML 根窗口销毁重建；后者重绑输入路由并验收重建后的换片。等待场景与 CPU 封面所有权集合一致后才判断资源回落；旧 polish 不能充作新场景的资源状态。每次保留自身 PNG、逐帧状态/提交、ready 延迟与窗口策略，不采集用户桌面。basic 循环可以在 surface 重建后复用上下文；完整根窗口删除才保证最后窗口的图形资源释放，报告分别记录 surface 次数、完整窗口次数和实际 context 初始化次数。`runtime-detail` 纳入 CTest，在默认 macOS 可见测试 OFF 时明确禁用；本轮授权可打开该选项运行，结束恢复 OFF 并真实回归 verification-window-policy。

## 平台验证与剩余交接

本机为 macOS/arm64、Apple M4（10 核 GPU），自动发现最新 Kit Qt 6.12.0，AppleClang 21.0.0.21000334；实际运行结论及简明范围在本节收尾记录。Windows、Linux 当前没有目标原生 Kit、窗口管理器和 GPU 会话，COMMON-01–05、PERF-01–07 及平台分支由 audit-detail.py 记录源码符号/hash 和人工调用链推演到本地 `verification/*-static/detail/`。静态审计通过，实机待验证（R34），不使用 macOS 图片代替目标运行。

Windows 取得目标环境后运行上述 Release 命令，再以 `--backend d3d11 --dpr 1|1.5|2` 串行重放；检查 D3D11 透明/深度、实际鼠标/触摸/键盘、退场门禁、焦点与十次图形上下文恢复。Linux 以同一工具 `--backend opengl` 分别在实际 X11/xcb、Wayland 下运行；核对逻辑坐标、DPR、抓取取消和重建。每个场景至少三次，保留来源/二进制/目标系统、实际后端/尺寸/画质及窗口策略。源码轨迹 1e-6、投影 2 逻辑 px、事件差一个 60Hz 帧及既有像素门槛不变。

三平台实际触摸硬件、系统级焦点/指针丢失与后续 R23/R24 搜索/歌曲定位不改播放集成仍按 R34。三维同机源 PMREM 参照、原 ROI/SSIM/颜色门槛按 MAC-VISIBLE-3D，完整 GUI 对照按 R29；本轮数学/结构/自身渲染通过不表示这些像素或后续产品边界已完成。后续修改共享状态、所有权或相机时回归 detail-motion、scene、archive-pool、real-columns、presentation、presentation-qml、navigation、title-reels 和对应实际后端产品回放。
本轮实际结论：授权前台配置的 Release CTest 32/32、check-format、官方 Yalahoo_qmllint/ui-audit_qmllint 通过；专项单元含 598 个 x/v/a 帧、50 个旋转检查点和 1,200 个材质帧，最大轨迹误差分别约 5.89e-15/1.90e-14/6.11e-14，材质约 1.33e-14，原 1e-6 门槛不变。原有阵列 GPU 的 Metal/OpenGL 回归和固定材质 210 参数检查通过。

同一最终 Release 二进制在 Metal/threaded 与 OpenGL/basic、实际 DPR1/1.5/2 六种上下文中各三次回放，18/18 通过，DPR2 使用夜间主题。每次包含三次正常换片及反向、退字连选、途中返回/搜索路由、拖动中切片、改速/减少动态、额外三十次切片、十次 surface 恢复和十次完整 App/QML 根窗口重建，重建后按钮仍可换片；QML warnings 为零。Metal 各次实际初始化 context=21，OpenGL=11；两者线程模式分别实测为独立渲染线程/GUI 基本循环，成本不混合解释。

回放实测峰值为三个 CPU 封面 owner/两个 returning，最终每次 owner=1、解码 active/pending=0，单批最大图集上传 2,097,152 bytes，运动期间 target/postprocess generation 不变；严格容量仍为选中加三个副本，满池/原位回收由专项与阵列压力入口覆盖。当前资源描述符最大值在 257,590,132–341,319,396 bytes，属于估算且不代表实测 VRAM。逻辑窗口 1280×720，正常画质的内部三维目标遵守原 RenderQuality 比例及上限，实际 DPR2 不等于三维目标未受限地渲染二倍分辨率；各目标尺寸、Qt 输出与上传/CPU租约分别保留在原始报告。

最终回放在本地 verification/macos/detail/r18/final-materials/，失败/中断及前置诊断目录均保留。原始图像、逐帧状态、机器矩阵和 hash 清单不进入 Git。默认配置恢复 YALAHOO_MACOS_VISIBLE_TESTS=OFF、YALAHOO_REVIEW=OFF，授权环境开关只用于已退出的验证子进程；恢复后完整 Release CTest 的 26 项启用测试全部通过，包含 verification-window-policy，另 5 项可见测试明确禁用。
