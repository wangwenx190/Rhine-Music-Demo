# 几何、相机与布局契约

冻结输入为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/viewport-layout.ts`、`music-camera.ts`、`scene.ts`、`motion.ts`、`music-model.ts`、`archive-loop.ts`。`tools/review/export-scene-golden.mjs` 执行 Git 中的函数与实际音乐相机目标代码块，记录输入 hash；仅使用 `legacy-web/` 已有 TypeScript/Three 开发依赖。产品不运行这些依赖，也不读取私人曲库。

## 共用对象与坐标

`src/render/` 的 C++23 `YalahooScene` 链接 Core/Gui 与已有运动内核；它不依赖 Quick3D 私有类型。`SceneAdapter` 在 `main` 中由唯一 `FrameClock` 注入，寿命覆盖 QML 引擎；QML 的 `GeometryScene` 只绑定其相机位置、四元数、视角、裁切面及盒体位置。物理单位不缩放，三个导入 mesh 的变换只应用一次，不翻转 Z。暂用无光、不透明调试材质以隔离几何；纸面、透射、光照与后处理由后续轮次实现。

`SceneFrame` 是按值复制的只读快照，含 revision、double 相机 position/aim/span、Qt view/projection、caseTransform、内容尺寸、布局类别与 clipNear=5/clipFar=300。以后渲染器在同步边界复制快照；本轮所有状态写入在 GUI 线程。`projectWorldPoint(world)` 返回内容逻辑像素 x/y 和 OpenGL 约定 NDC z；相机后方返回 NaN x/y、z=-1。不得将此 z 直接写入 D3D/Metal 深度：Quick3D 负责自身后端修正，条件 RHI 后端另用 QRhi 修正矩阵。

`hitTest(x,y)` 使用同一 frame 的 inverse(projection·view·caseTransform) 做近/远射线与原盒体包围体相交，返回 `objectId`、localPoint、worldPoint、frameRevision；屏幕外/未命中为空。当前唯一调试对象 ID 是 `inspection-case`，不是专辑持久 ID。R12 扩展实体实例、稳定专辑身份及精细拾取；R18 扩展盒体旋转。不要把包围体命中当成每个镂空三角形的精确命中。

## 布局与相机

`LayoutMetrics::setViewport(width,height,coarse,cinematic)` 接收窗口内容逻辑像素，正数下限 1。宽高比严格小于 1.05 才是 portrait；等于 1.05 是 landscape。compact 为 portrait、width<1100 或 coarse 且 height<600。普通 desktop 的参考 scale=height/1080；compact/portrait 为 1；电影参考 1920×1080、scale=min(width/1920,height/1080)。这些是布局测量参考，三维 View3D 仍直接填满真实内容，不能整体缩放产品为固定画布。

信息区域 x/right 解析冻结 music.css 最终覆盖：横屏 `.68633333*width` 与 `max(24,.022*width)`；竖屏使用 `clamp(23,.04*width,42)`，x 再包括原 -12px margin。其他控件密度、断行与实际页面在 R14/R16 接入，不能从调试文字外观推断产品布局。

浏览静止目标使用实际生产 shot=29.1 的终态：yaw=59°、elevation=25°、span=7.33、distance=140、arrayAim=(-1.091,-.045,.481)。详情 progress 在 [0,1]，同一进度插值 yaw 到 8°、elevation 到 20°、distance 到 72、基础 span 到 5.9。portrait span、previewY=.36、detailY=.27+34/height 以及 compact/desktop detailY=.49/560÷1080 来自 `archiveFraming`；音乐入口显式覆盖横屏 detailX=.25，不能误用通用 detailX=.27 或 550/1920。

横屏浏览 offset 的 16:9→21:9 过渡使用原 quintic smooth：right 0→3.5%、up 8→10%。portrait 采用固定 preview aim；detail 使用固定 inspection slot，不跟随实时盒体位置抵消抽取。selected slot 本轮固定匿名位置，基础 y=-4.6+settlingWave(0,26.56)，预览 lift=.9，inspection lift=3.35+.12，z=-2.17；完整波浪/实例池和抽取留在 R12/R18。

`advance(motionDt)` 消费已经乘速度的唯一 FrameClock 输出，不再乘倍率或另设产品计时器。position/aim 每轴和 span 按 rate=9 的解析临界阻尼推进；FOV=2 atan(max(.1,span)/(2 max(1,distance)))。`observe` 从当前 position/aim/span 差分保留 authored pose 的速度，为 R20 交接提供接口，不在本轮实现开场时间线。`setDetail`/resize 重定向保留相机状态；resize 同步发布新 aspect 和 portrait，同时等待现有速度连续追随新目标。减少动态直接落到新目标。`setNavigation` 保留 rate=6 小轨道，realColumns 屏蔽 row orbit；`setPointer` 只在普通浏览且非减少动态时提供原 .12 偏移。

## 验证与重建

Windows 运行 `./tools/build/verify-r06.ps1`，仅 Release，自动发现最新 Kit 并开启 `YALAHOO_REVIEW`；完成采集后关闭该开关、构建普通产物并验证 13 个普通测试和 Review CLI 拒绝。普通构建默认不识别 `--verify-scene`/`--scene-report`。它们仅用于精确验证状态，不参与播放器用户流程。`-CollectOnly` 只整理现有已通过的 16 套 Review 报告、核对 fixture hash，不重放 Review GPU 测试；仍执行 qmllint 与普通构建验证。`./tools/audit/audit-r06.ps1` 重建 macOS/Linux 新增调用链的静态审计。

CPU oracle 覆盖九必测尺寸及 1.05/1100/短屏边界，共 48 目标、528 顶点/锚点、694 帧 20/30/60/120fps 非零初态、反向、改速、长帧、resize 与减少动态。真实窗口另外逐状态重放 120fps 的 361 个采样，冻结生产 FrameAnimation 后由既有 FrameClock 规则计算的 motionDt 驱动；每个状态等待三帧实际渲染完成，不能用截图延时决定相机位置。View3D 公开投影同时比较 SceneFrame 与冻结 Three 投影，每点误差≤2逻辑像素；双精度目标/轨迹误差≤1e-6。

Review 窗口使用 frameless，允许超出当前显示器的真实客户区尺寸；不比较系统边框。测试 DPI 环境仅在验证进程有效，报告必须记录实际 QWindow DPR 与 PNG 尺寸，不能把 QT_SCALE_FACTOR 当作实际 DPR。Qt float/fuzzy 属性的相机绑定检查允许 position 1e-3、FOV 1e-4，实际投影仍严格≤2px。无光 mesh 像素、qrc/字体/ShaderTools、鼠标交互、生产帧源/隐藏恢复、空媒体源与全量回归一起验证。证据在 `verification/windows/scene/`，实际图片在 `docs/media/scene/`，源数值在 `tests/fixtures/scene/`。

公开接口语义：[PerspectiveCamera](https://doc.qt.io/qt-6/qml-qtquick3d-perspectivecamera.html)、[View3D 映射](https://doc.qt.io/qt-6/qml-qtquick3d-view3d.html)、[High DPI](https://doc.qt.io/qt-6/highdpi.html)。本轮不宣称材质、灯光、完整产品界面或性能门槛已验收。
