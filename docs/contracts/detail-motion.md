# 详情抽取、换片与拖动契约

源码依据为冻结 `music-camera.ts` 的 MusicPlacementMotion/MusicPresentation、`scene.ts` 的 setMode/switchMusicAlbum/select/bindPointer/update，以及 music-app.ts 的实际 presentation ports。冻结身份与清理后的 Git 读取规则按 web-baseline.md；测试 oracle 由 `tools/migration/export-detail-golden.mjs` 直接执行冻结类及生产旋转表达式生成，不修改 Web 基线。

`DetailMotion` 管理 archive/placing/presented/returning-center/returning-array。唯一 FrameClock 按 MotionController → SelectionController → SceneAdapter → PresentationController 顺序交付已经缩放的 motionDt。1.45/1.05 秒的五次轨迹保留 x/v/a 连续性，同目标不重启；其钳制进度同时驱动 .9→3.47 的抽取、相机 yaw/elevation、取景跨度与瞄准点，实际相机继续使用源码 rate=9 阻尼。生产只通过 requestDetail 发意图；setDetail 保留给已有固定状态/几何探针，不作为生产开关。

DetailPorts 是产品与单元测试共用的接线。退 browse 后才请求抽取；placement、相机、四条 rail/column/shoulder/lane、真实列、lift/rotation 都落稳并保持 .08 秒后才能通过场景门禁，再等当前 layoutRevision。详情换片先退旧字，保留详情构图，rail/新 lift/旧副本用 rate=9 同时运行，然后进新字。详情期间不启动浏览五次导航轨迹或选择波纹。相机轨道消费实际 rail/column 速度；竖屏/真实列保留源码的行速度约束。归位/浏览保留各自的 4.2/4.5 lift rates，不把详情 rate=9 施加到普通返回。

ReturningCasePool 最多三个副本，加一个选中实体。每个副本保留 cell、albumId、objectId、lift 的显示导数、yaw 和可选世界高度；旋转归零前冻结世界高度，之后才下降。同列相邻五行内的旋转归位副本未对齐时，新盒等待净空。反向选择回收同一 cell 的实际 objectId 和显示状态，不重新生成封面。池满时 PresentationController 只保留最后一个 pendingSelection，等待完成回收；不驱逐未完成副本、不提前清掉目标或准备错误详情。到达原 lift/rotation 退出阈值后先解除副本所有权，再恢复阵列实体，避免空缺一帧。

PointerDragController 仅保存一个指针和最新 yaw 目标。横向每逻辑像素 .004 弧度、范围 ±.8；竖向不改变角度；按 `1-exp(-2.8*motionDt)` 跟随，释放保留目标，回正使用冻结 returnStep。减少动态直接走相同终点/完成路径。生产 SceneItem 额外要求 detailEnabled 和实际净空；退字、换片、失去抓取、隐藏/失焦、多指和取消都撤销手势。点击只在释放且全程未越过 6px 时提交。hover 使用同一相机 pointer 偏移；实体矩阵同时用于真实渲染、光场和逆射线拾取，不在 QML 做假旋转。

CaseClarity 直接移植冻结 decryption.ts 的材质状态：原时间 34.12–39.56、固定参考倍率 1.5、六个 REVEAL 节点的单调 Hermite 曲线、ready 后起计、重新进入时保留显示值、离开/副本按 exp(-9*motionDt) 回雾。1.5 是原片时间映射，不再次缩放界面速度；静态导数在编译期缓存。副本独立保存 clarity，反向回收同一状态。SceneFrame 交付各实体的值，生产阵列基准 clarity=0；盖面仅替换既有材质 uniform 中的粗糙度，其余源光学公式、PSO/纹理和资源键不变。固定状态 Review 入口保留原全局 clarity，用于既有材质 oracle；该兼容输入不作为生产材质时序。

## 资源与测量边界

运动只发布变换和必要光场，不重建材质、图集或不变附件。四个清晰封面CPU租约、分块与mip按实际投影需求尺寸记账；GPU清晰区包含于生产图集，布局与所有权分别由archive-pool.md/covers.md维护。它们不等于完整封面系统内存。

产品回放分别记录请求交付到场景及 CPU 封面 ready、实际 Qt submission、atlasUploadBytes、resourceGeneration/postprocessRevision、CPU 租约/分块暂存、ResourceInventory 附件与保守重建重叠。motion 状态不能触发目标重建，普通上传批次上界 2MiB。GPU time 是 QRhi 已完成帧的可用观测，可能对应先前帧；不可用为 null。descriptor 字节是估算，不包含驱动 padding/PSO/SRB 和窗口/compositor，不冒充实测显存。局部回放不提供屏幕显示延迟；性能是否采样及协议唯一见performance.md。
## 检查入口

- `node tools/migration/export-detail-golden.mjs`生成冻结数学；`detail-motion`比较598轨迹帧、50旋转点、1200材质帧，x/v/a≤1e-6。覆盖20/30/60/120fps、1/120秒打断、三次换片/反向回收、满池最后目标、返回/搜索替换、拖动取消、隐藏/换库/九尺寸拾取和真实列边界。
- `python3 tests/runners/verify-detail.py --backend <目标后端> --dpr 1|1.5|2 --runs 1 --output 全新目录`通过真实App/FrameAnimation回放；夜间`--night`，前台仅授权轮次加`--foreground`。
- `--window-lifecycle`仅补生产App同窗口的一次hide与一次minimize/restore，核对隐藏无视觉工作、原窗口/SceneItem与实际提交/自身输出恢复；不增加换片压力或QML根重建。
- 普通功能保持lifecycleStressRequested=false。性能/资源寿命先按performance.md严格静态审查，仅实际未决项才选择最小生命周期观测；以下次数是工具的完整压力能力，不是默认验收要求。`--lifecycle`另执行三十次换片、十次surface及十次完整App/QML根窗口重建，重绑输入并检查重建后换片；basic允许surface复用context，记录实际context初始化数。
- ready核对同revision实体身份/CPU封面租约集合，最终owner回落且active/pending为0、运动不改target/postprocess代次、单批≤2MiB。旧窗口回调核对epoch；清理帧在有效context交付后再hide/destroy。
- 目标系统、物理设备与实际焦点/播放集成的剩余事实唯一由迁移计划R36维护。
