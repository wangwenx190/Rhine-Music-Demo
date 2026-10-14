# 滑尺、数字字轮与导航输入

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `music-ruler.ts`、`music-navigation-ruler.css`、`music-wheel-navigation.ts`、`viewport-layout.ts`、`music-text-motion.ts`、`music-app.ts` 及其锁定的 rolling-number 数字轨迹。来源通过 `frozen-git.mjs` 解析，不改写 `legacy-web/`。输入和布局仍以实际生产调用路径为准。

## 所有权与接口

- `SelectionController` 继续独占专辑 ID、实体坐标、真实列边界、循环和列记忆；`LibrarySession` 的导航与播放 ID 独立。
- `InputRouter` 接收 SceneItem 的原生滚轮/触摸、窗口的非编辑快捷键以及导航按钮。QML 滑尺的点击区和 Canvas 绘制共用 `RulerController` 的同一槽快照；不从图像反推选中项，不建立第二套库模型。
- `RulerController` 消费唯一 `FrameClock::frame` 已缩放的 motionDt。临界阻尼 rate=18、12 个可见槽、左右各 2 个预留槽、anchor=5、560ms ripple 和每槽 24ms 的列变更展开延迟均来自源函数。宽度、位置偏移、存在感、刻线高度及透明度各自接续原速度。
- 槽只在真正不可见后复用；组变更按当前物理位置匹配最近目的地。循环溢出列保留显示 occurrence，相对当前物理行提交点击；短列显示唯一项，真实列提交有限行。场景 rebase 只调整与场景的坐标桥，不重置滑尺的弹簧、fractional scroll、速度或 ripple 进度。
- 桌面刻线 width/rest/full/hover/gap 为 3/10/30/22/8px；≤700px 为 2/8/21/16/6px。溢出两端按 gap 宽线性 alpha mask 淡隐。不可见槽不能点击、Tab 或辅助技术选中；焦点离开的槽把组件焦点交给当前或第一个可见项。

## 输入边界与背压

Qt `pixelDelta` 优先；没有 pixelDelta 时，angleDelta 的 120 单位映射 48px。Qt 滚轮正 Y 与浏览器 deltaY 相反，在入口明确反号。每 48px 一张，单事件至多三张，余量绝对值始终小于 48px；200ms 以上间隔、反向、切列、库/模式替换、指针按下、按键和窗口失活清余量。水平手势、Shift 滚轮和 Control 捏合保留原所有权。

窗口键盘过滤器保护 TextInput/TextEdit、Slider、SpinBox、ComboBox、Popup 及显式 `navigationInputOwner` 祖先；Control/Command/Alt 快捷键不触发导航。滚轮由 Qt 先选择接收对象，再检查点击位置的控件/祖先：按钮、Flickable、面板、歌单、编辑器和滑块在滚动边界仍保留事件。未来面板可在根项设置 `navigationInputOwner: true`；`InputRouter.enabled` 用于整体模式互斥。所有方向最终走同一个 SelectionController 边界。

触摸保留 36px 最小位移、主/次轴 1.3 比、1400ms 最大时长，单点手势结束只提交一次。取消和多点输入放弃本次滑动。按键和滑尺不使用任意延迟 debounce。

已有选中/归位封面最多四份所有权。连续输入碰到三个归位副本容量时，InputRouter 只保存**一个可替换绝对目的地**，后续反向输入以该目的地继续累计；唯一帧时钟在容量释放后尝试提交。没有输入队列或无限归位副本。库/模式替换取消待提交目标，rebase 平移其坐标；切列按既有列记忆重算唯一目的地，后续行输入可继续作用于尚待提交的列目标；外部选择取代此前待提交目标。

`SceneFrame` 分别携带 inputRevision/inputReceivedNs 与 navigationRevision/navigationInputRevision/navigationCommittedNs；实际提交报告保留这组字段和 submittedNs。待提交输入尚未成为选中状态时，帧的 navigationInputRevision 仍指向此前真正提交的导航，不能把带有最新输入时间戳的旧状态帧当作响应。它们只测输入→状态/Qt 提交，不是屏幕显示或 input-to-photon 实测。

## 数字轨迹与节点

`OrdinalMotion` 移植锁定 rolling-number 的 49 点、460ms 临界阻尼采样函数：从当前显示位置/采样段速度重定向，以原速度上限生成新轨迹，最后一点精确落目标。循环按输入方向选数字 occurrence；行序号保留行方向，列序号/列内总数保留列方向，专辑编号采用最后导航方向，列总数自动选向。尚未提交的列目标后接行输入时保留两者各自方向；外部选择清掉旧方向，显式滑尺点击取代待提交目标。相同数字不重启。完成后归一到十进制局部轮，避免长期无界坐标。初始 QML 属性在 componentComplete 前静态准备，之后第一次选择同样动画。

最多十位，每位只发布当前和下一个可见面；整数 Repeater 模型不会随逐帧 QVariantList 变化重建。专辑编号、选中序号、列内总数及列序号/总数五个数字组件共用同一 FrameClock，速度只缩放一次；减少动态立即落点，隐藏停止推进。列表快照每次发布只构造一次、通过 Qt 隐式共享读取；绘制线程只复制固定十六条刻线值，不读库、磁盘或 QObject 业务状态。

数字使用同源 MiSans、字重和字距，Qt Quick Text 的两个面按源行高垂直裁切，整个小组件用 qsb 边缘渐隐合成。多行标题、grapheme/shaping、文字测量缓存和 TransportTitle 的全面选型仍由 R16 执行，本组件不提前替代它们。

## Canvas Painter 选型、资源和失效

生产采用 **C++ QCanvasPainterItem**，与同输入 Canvas2D、Qt Quick Rectangle/ShaderEffect 对照。两种 Canvas 均可表达源刻线；C++ 能直接同步固定值快照，在 resize/隐藏恢复时复用业务对象，并把三份静态路径缓存限定在所属 renderer，避免预览 Canvas2D 的绘制上下文重建成为产品生命周期的一部分。选择依据包含等价性和所有权复杂度，不把局部 CPU 诊断当作完整产品提速。

绘制顺序为既有三维 SceneItem 输出→阅读遮罩→界面/数字层→滑尺透明附件的 Qt Quick 合成。刻线自身改变高度，没有覆盖指示条。Canvas 附件固定为桌面 130×30、窄屏 96×21 逻辑像素，RGBA8、1 sample、透明混合；空列不显示绘制项，宽度动画只改变 Qt Quick 的矩形 clip 节点，不重建附件；没有额外全屏遮罩、mips、封面上传或 MSAA resolve。绘制完整刻线后统一裁切，避免把不足一像素的露出部分重建成细矩形而改变抗锯齿覆盖。淡隐直接乘入边缘刻线的 fill alpha。三份 actual-size rest/full/hover 路径只在尺寸几何变更时替换并移除旧 path group；变化中的高度用 fillRect，不能拉伸已经细分好的抗锯齿边缘。

renderer/路径/内部缓冲归 Qt render-thread item 生命周期；隐藏不发布无效绘制，restore/scene graph release 重建所属渲染资源。QML 保留固定十六个按钮及有界字轮面；文字内容、字体和几何未变时不重建。比较用 Canvas2D 只保留一个 Loader 上下文，表面替换时重建，状态存在外层，没有独立 Timer 或 requestAnimationFrame。

`ResourcePlan` 在原图集、三维目标和 Qt 输出之外加入物理 DPR 下 UI 附件估算：滑尺 130×30 的 RGBA8 与保守 D32FS8 上界、主数字层最多 700×65 的 RGBA8、四个最多 160×12 的小数字层。这里宽度取十位数字/最大导航字号的保守容量，不是每帧必须分配。DPR1 的这组描述符上界为 259520 bytes，DPR2 按实际取整尺寸计算；resize 时旧/新 UI 附件可能同时存活，要把二者相加，再加三维 resize 资源。Qt 字形图集、模块内部几何/PSO、驱动对齐及 compositor 储存另外列为未知，不当作显存实测。

主题只失效颜色；交互只更新十六条活跃刻线；窄屏分界失效三份几何路径，DPR/实际附件尺寸变化由 Qt 重建相应目标。导航 DPR 只影响累计资源描述符，不能触发无关三维 PSO/纹理重建。GUI/render 热路径没有读盘、解码、等待、GPU finish 或读回。

## 验证入口与结论边界

Windows Release：`./tools/build/verify-r15.ps1`；平台/算法调用链审计：`./tools/audit/audit-r15.ps1`。

- `NavigationTest` 检查滚轮余量/反向/上限、真实列夹取、36px/1.3/1400ms、编辑所有权、循环点击、rebase、快速反向最终目标、隐藏与数字重定向。
- `node tools/review/export-ruler.mjs` 从冻结生产闭包重建参考；原生 `ui-audit --ruler <output>` 比较 20/30/60/120fps，代表场景逐 1/120 秒核对全部槽。数字纯轨迹直接对照锁定依赖的原始 track/sample 函数。
- D3D11/OpenGL、双主题、桌面/窄屏、实际 DPR1/1.5/2 比较 C++、Canvas2D、Quick。原始 SSIM/RGB/轮廓保留；沿用 R14 的**参照独立的一物理像素**边缘带方法，轮廓仅通过冻结 DOM 提高 alpha 生成，位置/高度/裁切不变。带外仍用 SSIM≥0.98、RGB≤6，独立 double 状态误差≤1e-6，另保存每条几何 ROI 的未归一化墨色积分误差，并以冻结源码矩形/线性淡隐的连续面积积分与原始浏览器墨色积分形成覆盖区间；原生墨色超出该区间仍使用 RGB≤6 门槛。区间仅由源码几何、源颜色/alpha、裁切和参考图确定，不读取原生误差来调整；缺线和过深的亚像素裁切不能由边缘带豁免。不扩带、不模糊，也不把带外分数称为原始全图分数。
- 每条 API 执行 baseline 加十次隐藏/release/resize/恢复的自身提交图像对照，C++ renderer 同时至多一个，退出回到零。静止重绘采样使用可见且已落点的滑尺，先完成切回可见及资源初始化的三次实际提交，再建立 paint 计数基线并推进控制器一次，区分必要初始化与无效失效。单个异步读回按后端 MaxAsyncReadbackFrames+1 排空；诊断像素在完整比较范围外，不进入产品。
- `yalahoo --verify-navigation <output>` 在真实 App 路径发送事件，检查 pixelDelta、连续反向、切列、编辑焦点、有限端点、QML 滑尺点击、窄屏/主题、hide/restore、导航/播放独立以及输入和实际提交 revision。

同步、Canvas paint、Canvas2D JS 和 Qt Quick before/afterRendering 的 CPU 诊断分别保留。Date.now 为毫秒分辨率，0 不是零成本；GPU timestamp 不可用写 null。局部指标包含初始化/诊断提交，不是正式 FPS、GPU 完成或屏幕延迟。完整产品预算仍按 R28 验证。

DPR1.5 的冻结浏览器合成在 2px 窄线两边产生 75% 覆盖，即使重新加载同 DPR 文档也保留；源码矩形并无这种额外透明度。覆盖区间保留这份原始差异，同时允许源码连续填充的正确墨色量，不将浏览器栅格损失复制进产品。Quick 对照显式开启几何抗锯齿；两种 Canvas 都绘制完整刻线后用相同 Quick clip 裁切；Canvas2D 使用显式 rect/fill 路径，并按实际 DPR 设置 antialias=1/DPR，保持一物理像素的抗锯齿范围，避免固定逻辑像素宽度在高 DPR 上侵蚀细线。

原始图像、逐帧序列、报告和 hash 矩阵仅保存在忽略的 `verification/windows/navigation/` 及各平台静态审计目录。各平台后续按当前可靠能力验收，历史静态结论不限制macOS等当前平台的真实验证；未可靠执行的compositor、字体回退、Metal/GL、触控或部署项目严格审计并保留到R34。

## 本阶段结论

Windows 最新原生 Kit 的 Release 构建、check-format、两组 qmllint 和 26 项 CTest 通过。D3D11/OpenGL、双主题、桌面/窄屏及实际 DPR1/1.5/2 的三方绘制对照共 864 张、生命周期对照共 198 项通过；静止推进不重绘，renderer 退出归零，缺线/过量墨色反例被拒绝。滑尺 double 状态误差最大 6.19e-13，数字轨迹误差最大 2.23e-16；固定边缘带外 SSIM 最低 0.99745、平均 RGB 误差最高 0.0592/255，覆盖区间外逐刻线误差最高 4.591/255。原始全图 SSIM 最低 0.94977，原始差异与失败样本留在本地；不将边缘带外指标当作原始全图成绩。

两个真实 App 后端的 20 项检查分别通过，覆盖输入/提交 revision、反向、列切换、编辑焦点、滑尺、触摸边界、隐藏恢复及导航/播放独立；行/列循环方向、待提交列后接行输入及外部选择清除旧方向另由 NavigationTest 验证。PERF-01–07 的调用链和资源审计通过；局部 CPU 诊断不构成完整产品性能结论。macOS/Linux：静态审计通过，实机待用户验证。
