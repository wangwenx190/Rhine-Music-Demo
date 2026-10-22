# 几何、相机与布局契约

冻结输入为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/viewport-layout.ts`、`music-camera.ts`、`scene.ts`、`motion.ts`、`music-model.ts`、`archive-loop.ts`。`tools/migration/export-scene-golden.mjs` 执行 Git 中的函数与实际音乐相机目标代码块，记录输入 hash；仅使用 `legacy-web/` 已有 TypeScript/Three 开发依赖。产品不运行这些依赖，也不读取私人曲库。

## 共用对象与坐标

`src/render/` 的 C++23 `YalahooScene` 链接 Core/Gui 与已有运动内核；它不依赖 Quick3D 私有类型。`SceneAdapter` 在 `main` 中由唯一 `FrameClock` 注入，寿命覆盖 QML 引擎；生产 QML 使用 `SceneItem`，它只读取 SceneAdapter 的不可变相机/实体快照及既有灯光/画质值；`GeometryScene` 仅是 Review 的 Quick3D oracle。接入边界见 [rhi-integration.md](rhi-integration.md)。物理单位不缩放，三个导入 mesh 的变换只应用一次，不翻转 Z。生产RHI几何诊断使用独立法线/深度输出以隔离几何；材质检查见[单盒材质](materials.md)，几何和材质验收各自冻结相机与图层。动态灯光、阵列和后续后处理按对应轮次接入。

`SceneFrame` 是按值复制的只读快照，含 revision、double 相机 position/aim/span、Qt view/projection、caseTransform、内容尺寸、布局类别与 clipNear=5/clipFar=300。RHI 在同步边界复制快照；所有状态写入在 GUI 线程，矩阵实际不变时不重新发布几何 revision。输入 revision/time 另按集成契约记录。C++接口`projectWorldPoint(world)`返回optional，存在时为内容逻辑像素x/y和OpenGL约定NDC z；相机后方、非法视口或非有限结果为空，调用者先检查再使用。不得将此z直接写入D3D/Metal深度：Quick3D负责自身后端修正，生产RHI使用QRhi修正矩阵。

`hitTest(x,y)` 使用同一 frame 的 inverse(projection·view·caseTransform) 做近/远射线与原盒体包围体相交，返回 `objectId`、localPoint、worldPoint、frameRevision；屏幕外/未命中为空。独立探针对象ID `inspection-case`不能作为专辑持久ID。实体身份/实例和旋转拾取分别见archive-pool.md/detail-motion.md。不要把包围体命中当成每个镂空三角形的精确命中。

## 布局与相机

`LayoutMetrics::setViewport(width,height,coarse,cinematic)` 接收窗口内容逻辑像素，正数下限 1。宽高比严格小于 1.05 才是 portrait；等于 1.05 是 landscape。compact 为 portrait、width<1100 或 coarse 且 height<600。普通 desktop 的参考 scale=height/1080；compact/portrait 为 1；电影参考 1920×1080、scale=min(width/1920,height/1080)。这些是布局测量参考，三维 View3D 仍直接填满真实内容，不能整体缩放产品为固定画布。

信息区域 x/right 解析冻结 music.css 最终覆盖：横屏 `.68633333*width` 与 `max(24,.022*width)`；竖屏使用 `clamp(23,.04*width,42)`，x 再包括原 -12px margin。其他控件密度、断行与实际页面在 R14/R16 接入，不能从调试文字外观推断产品布局。

浏览静止目标使用实际生产 shot=29.1 的终态：yaw=59°、elevation=25°、span=7.33、distance=140、arrayAim=(-1.091,-.045,.481)。详情 progress 在 [0,1]，同一进度插值 yaw 到 8°、elevation 到 20°、distance 到 72、基础 span 到 5.9。portrait span、previewY=.36、detailY=.27+34/height 以及 compact/desktop detailY=.49/560÷1080 来自 `archiveFraming`；音乐入口显式覆盖横屏 detailX=.25，不能误用通用 detailX=.27 或 550/1920。

横屏浏览 offset 的 16:9→21:9 过渡使用原 quintic smooth：right 0→3.5%、up 8→10%。portrait 采用固定 preview aim；detail 使用固定 inspection slot，不跟随实时盒体位置抵消抽取。独立固定状态selected slot采用匿名位置，基础 y=-4.6+settlingWave(0,26.56)，预览 lift=.9，inspection lift=3.35+.12，z=-2.17；完整波浪/实例池和抽取留在 R12/R18。

`advance(motionDt)` 消费已经乘速度的唯一 FrameClock 输出，不再乘倍率或另设产品计时器。position/aim 每轴和 span 按 rate=9 的解析临界阻尼推进；FOV=2 atan(max(.1,span)/(2 max(1,distance)))。`observe` 从当前 position/aim/span 差分保留 authored pose 的速度，为R20接入开场时间线提供接口。`setDetail`/resize 重定向保留相机状态；resize 同步发布新 aspect 和 portrait，同时等待现有速度连续追随新目标。减少动态直接落到新目标。`setNavigation` 保留 rate=6 小轨道，realColumns 屏蔽 row orbit；`setPointer` 只在普通浏览且非减少动态时提供原 .12 偏移。

固定静止锚点的基础高度在CameraFraming函数级静态值中求解一次，portrait、详情与独立selected slot共用同一源公式。相机位置、aim、span和viewport不变时，SceneAdapter保留view/projection、相机旋转与view-projection乘积；实体波浪仍独立更新快照revision。

## 源状态与投影检查

`node tools/migration/export-scene-golden.mjs`生成`tests/fixtures/scene/`；`scene`检查48布局/相机目标、528顶点/锚点、694帧20/30/60/120fps的非零初态、反向/改速/长帧/resize/减少动态，double≤1e-6。

生产RHI真实投影≤2逻辑px；`rhi-opaque-gpu`验证实际几何/深度，`archive-pool-gpu`/`real-columns-gpu`核对阵列投影与生产快照。窗口记录实际QWindow DPR和目标像素；验证进程缩放环境变量不能替代真实DPR。几何输出须renderError为空，超大目标共用RenderQuality保护并检查恢复。

源码/投影检查使用已注册的`scene`及上述生产RHI组件测试；公共构建与窗口行为由native-bootstrap/test-gates/test-window-policy维护。材质/光照/后处理各有独立组件契约。
