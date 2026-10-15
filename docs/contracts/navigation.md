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

数字使用同源 MiSans、字重和字距，Qt Quick Text 的两个面按源行高垂直裁切，整个小组件用 qsb 边缘渐隐合成。多行标题、grapheme/shaping、文字缓存与TransportTitle归[title-reels.md](title-reels.md)。

## Canvas Painter 选型、资源和失效

生产采用 **C++ QCanvasPainterItem**，与同输入 Canvas2D、Qt Quick Rectangle/ShaderEffect 对照。两种 Canvas 均可表达源刻线；C++ 能直接同步固定值快照，在 resize/隐藏恢复时复用业务对象，并把三份静态路径缓存限定在所属 renderer，避免预览 Canvas2D 的绘制上下文重建成为产品生命周期的一部分。选择依据包含等价性和所有权复杂度，不把局部 CPU 诊断当作完整产品提速。

绘制顺序为既有三维 SceneItem 输出→阅读遮罩→界面/数字层→滑尺透明附件的 Qt Quick 合成。刻线自身改变高度，没有覆盖指示条。Canvas 附件固定为桌面 130×30、窄屏 96×21 逻辑像素，RGBA8、1 sample、透明混合；空列不显示绘制项，宽度动画只改变 Qt Quick 的矩形 clip 节点，不重建附件；没有额外全屏遮罩、mips、封面上传或 MSAA resolve。绘制完整刻线后统一裁切，避免把不足一像素的露出部分重建成细矩形而改变抗锯齿覆盖。淡隐直接乘入边缘刻线的 fill alpha。三份 actual-size rest/full/hover 路径只在尺寸几何变更时替换并移除旧 path group；变化中的高度用 fillRect，不能拉伸已经细分好的抗锯齿边缘。

renderer/路径/内部缓冲归 Qt render-thread item 生命周期；隐藏不发布无效绘制，restore/scene graph release 重建所属渲染资源。QML 保留固定十六个按钮及有界字轮面；文字内容、字体和几何未变时不重建。比较用 Canvas2D 只保留一个 Loader 上下文，表面替换时重建，状态存在外层，没有独立 Timer 或 requestAnimationFrame。

`ResourcePlan` 在原图集、三维目标和 Qt 输出之外加入物理 DPR 下 UI 附件估算：滑尺 130×30 的 RGBA8 与保守 D32FS8 上界、主数字层最多 700×65 的 RGBA8、四个最多 160×12 的小数字层。这里宽度取十位数字/最大导航字号的保守容量，不是每帧必须分配。DPR1 的这组描述符上界为 259520 bytes，DPR2 按实际取整尺寸计算；resize 时旧/新 UI 附件可能同时存活，要把二者相加，再加三维 resize 资源。Qt 字形图集、模块内部几何/PSO、驱动对齐及 compositor 储存另外列为未知，不当作显存实测。

主题只失效颜色；交互只更新十六条活跃刻线；窄屏分界失效三份几何路径，DPR/实际附件尺寸变化由 Qt 重建相应目标。导航 DPR 只影响累计资源描述符，不能触发无关三维 PSO/纹理重建。GUI/render 热路径没有读盘、解码、等待、GPU finish 或读回。
## 非像素验证入口

- `NavigationTest`：滚轮余量/反向/上限、有限列、36px/1.3/1400ms触摸边界、控件所有权、循环点击、rebase、待提交目标/方向、隐藏及数字重定向。
- `node tools/review/export-ruler.mjs`导出冻结数学；`ui-audit --ruler 输出目录`（CTest `ruler-drawing`）中的状态检查覆盖20/30/60/120fps及1/120秒槽采样，double误差≤1e-6。实际三API检查完整刻线/Quick clip、淡隐、两主题/桌面窄屏/实际DPR、零无效重绘及十次恢复/renderer归零；自身输出只用于内容/实际质量检查，不读浏览器PNG或运行图像分数。
- Canvas2D比较项显式rect/fill，antialias保持Qt默认1，与C++路径同值，DPR由所属目标处理；不足一像素露出仍绘完整刻线后裁切。
- `yalahoo --verify-navigation 输出目录`通过真实App发送pixelDelta/反向/切列/编辑焦点/有限端点/滑尺/主题/触摸/隐藏恢复事件，核对导航与播放隔离、input与实际提交revision。
- Windows包装入口`verify-r15.ps1`及`audit-r15.ps1`复用非像素检查；源码审计统一为`python3 tools/audit/audit-browse-navigation.py`。共享测试分组见[test-gates.md](test-gates.md)，欠验只归计划R34的R19E-P。
