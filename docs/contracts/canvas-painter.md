# 二维绘制与 R14 浏览界面契约

生产入口 `App.qml` 使用 `DesignTokens`、`Header`、`BrowseInfo`、`Footer`、`NavigationFrame` 和 `ReadingVeil`，叠在已有三维 `SceneItem` 之上。旧运行探针移至 `BootstrapProbe.qml`，只由验证参数加载。导航输入、生产滑尺和数字字轮按 [navigation.md](navigation.md) 接入同一选择控制器及帧时钟；标题与歌名字轮按 [title-reels.md](title-reels.md) 接入，其完整绘制选择及矩阵验收留在R16B1–B3，详情、面板和播放操作分别归后续轮次。

## 源码与接口

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/music-app.ts`、`music.css`、`music-theme.css`、`music-theme-switch.css`、`music-transport-title.css`、`music-title.ts` 及 `viewport-layout.ts`。读取经 `frozen-git.mjs` 映射清理后的 Git 对象；Web 参照由冻结生产 markup/CSS 和实际 `setupMusicTitleLayout` 静态调用生成，不访问用户曲库。本阶段明确保留窄屏两个主题文字，因此参照对横屏 ≤1100px 的 theme span 施加这一项显式差异；不修改其他样式。品牌绘制按 R03B 替换，容器锚点仍比较，原品牌字形不参与像素等同比较。

- `LayoutMetrics` 是二维和相机共用的逻辑尺寸源，1.05 分界、信息区 x/right 不复制到第二个控制器。
- `LibrarySession::browseInfo` 按稳定导航 ID 查询已有专辑索引和新建的类型名索引，交付标题/艺术家/类型/年份/曲数。快照替换即使 ID 不变也通知；搜索、排序和浏览不发出播放命令。类型索引仅在快照替换构建；全局/列内序号索引在快照或排序列变化时一次构建，读取不扫描集合。
- `ArchiveText` 按同源字体度量提供 CSS 行框与上下留白：normal 行框使用分别取整的 ascent/descent，小数字号以相邻整数的未 hint 矢量文字加局部比例变换保持精确尺寸/字距，不扩大字形缓存，也不增加 layer/FBO。眉题的 1.7 行高、两个 span 的4px边距和空格 advance 分别建模；打开按钮按实际 CSS 级联继承16px，并显式清除 Basic 默认内边距；`NavigationFrame` 保留父级 16px 的 inline strut，并按 ascent/descent 和 inline span 的半行距计算字行盒。这解释浏览信息底边，不能用一张截图反推常量。导航输入/刻线/数字序号由 R15 的 InputRouter/RulerController/OrdinalReel 接管；R16 接管完整文字测量缓存及标题字轮。
- 当前歌名为 `Header.playingTitle`，宽度上限按冻结 760/610/320px 规则；播放/停止按钮始终锚定最右侧。主题请求和其他按钮通过 `themeRequested` / `actionRequested` 交付，不修改音乐队列。减少动态与速度从已有 `MotionController` / `FrameClock` 注入。
- 七种颜色和 nightMix 的过渡保持 650ms、源 cubic-bezier(.4,0,.2,1)；选择 pill 保持 380ms、(.22,1,.36,1)。夜间侧向遮罩横屏为 32/58/78%，竖屏在 650ms 内消退；阅读层位于页眉之前，不覆盖顶栏。

## Canvas Painter 两种 API

最新原生 Kit 自动发现增加 `CanvasPainter`；应用链接 `Qt6::CanvasPainter`。独立 `ui-audit` 链接同一模块并导入 `QtCanvas2D`，QML module 明确依赖 `QtQuick` / `QtCanvas2D`，资源使用 qrc 别名；未锁定 minor/patch 或安装路径。

[`QCanvasPainterItem`](https://doc.qt.io/qt-6/qcanvaspainteritem.html) 由 `createItemRenderer` 创建并拥有 render-thread renderer，`synchronizeData` 只复制 bool。`CanvasProbe` 的 QCanvasPath 和 QFont 在 renderer 初始化一次，使用独立 painter（setSharedPainter(false)）按固定 path group 缓存曲线，缓存随 renderer 释放；`paint` 无磁盘、解码、等待或主动读回。透明 fill 必须同时开启 alphaBlending，sampleCount 明确为 1，保留模块自身几何抗锯齿。

[`Canvas2D`](https://doc.qt.io/qt-6/qml-qtcanvas2d-canvas2d.html) 使用 QML `property path2d`，以 `addPath(svgString)` 初始化；它不是浏览器的 `new Path2D()`。矩形裁切使用 `ctx.setClipRect`，不调用不支持的通用 `clip()`。每次绘制先 `ctx.reset()` / `resetClipping()`，restore 后显式 resetClipping，防止文字仍被矩形裁切。path2d 在节点生命周期内复用，JS 不使用共享 painter 的 GPU path group，避免 Loader 反复重建积累缓存；主题和首次 available 的失效通过 Qt.callLater 合并后触发 requestPaint。当前预览 Canvas2D 上下文不能可靠重放到替换的表面：外层保存业务状态及累计计数，内层 Loader 在窗口隐藏/恢复、scene graph 失效和尺寸改变时销毁/重建唯一绘制实例。路径在每个新上下文初始化一次；不因主题或静止帧重建。C++ 继续复用自身资源，这一差异是后续选择具体 API 的成本输入，不使用 requestAnimationFrame 或另一个时钟。通用路径裁切不属于已验证能力，后续需要时用 Qt Quick 的实际合成接口单独对照，不以矩形裁切冒充。

三个路径用同一 360×160 内容、路径、渐变、透明度、裁切、MiSans/中文、下伸字符、组合字和 emoji 输入对照：C++ Canvas Painter、Canvas2D、实际 Qt Quick CurveRenderer/Text。图形区域和文字区域分别报告；文字差异不以大面积空白稀释。C++ 与 Canvas2D 文字独立执行 SSIM ≥0.98 / RGB ≤6，并要求可见文字像素数。Qt Quick Text 的高 DPR 字形栅格差异单独记录：当前 DPR2 原始文字 SSIM 约0.93，不把它表述为文字路径全部等价，R16 仍需针对字轮输入比较和选择。Windows emoji 通过系统字体回退进入同一 shaping 链，不能据此宣称 macOS/Linux 的彩色 emoji 已实机等价。

生产滑尺按 navigation.md 采用 C++ Canvas Painter 的有界状态同步、固定容量附件和实际尺寸路径缓存，Canvas2D/Qt Quick 保留同输入比较。R20 的序章可使用已验证的曲线、SVG 路径、描边、渐变、alpha 和矩形裁切接口，按其输入/缓存/生命周期与两种 API 成本对照选择。R16 保留 Qt Quick Text 和 Canvas 的独立文字比较，包含整行 shaping、grapheme、彩色 emoji、DPR 与裁切；本探针不替代该验收。

## 绘制顺序、资源与测量边界

生产次序：SceneItem 的既有 RHI 图 → 横屏全宽阅读渐变或竖屏阅读渐变 → 暖昼或夜间竖向渐变 → 夜间侧向渐变 → 竖屏信息区局部渐变 → 页眉/浏览文字/静态计数/页脚。稳定态透明层 visible=false，没有 layer.enabled 或整屏 ShaderEffectSource；普通矩形渐变和控件直接进入 Qt Quick 主场景。Flickable 用矩形裁切；文字、SVG 和图形节点内容未变时不重建。主题切换只改颜色/透明度，不重建 SceneItem 目标、纹理或模型。

探针的 Canvas item 各自先绘制到 Qt 管理的 RGBA8、1 sample 离屏颜色附件，然后作为透明纹理合成到 Qt Quick 输出。没有 MSAA resolve、mip 链或业务封面上传。每个同时可见 probe 只有一项；其颜色附件为 `ceil(360*dpr)×ceil(160*dpr)×4`，潜在深度附件按每像素 8 bytes 保守计入；resize 的新旧峰值同时计入，不能将这一估算当成实测显存。Qt swapchain/主输出、字体图集、模块内部 vertex/uniform buffer 与驱动储存另计，未获得数据的项明确未知。探针不属于产品资源计划；R15/R20 真正采用 Canvas 后必须把其附件和缓存加入累计 ResourcePlan。

renderer / JS item 的字体、路径及内部 GPU 资源归 Qt item 生命周期；hide 不发布绘制状态，C++ restore/releaseResources 由 Qt 在所属线程重建，JS 使用上述受控绘制实例生命周期，恢复与420×200 resize 后对实际提交图像再次执行 SSIM/RGB 门槛；退出和十次 Loader 卸载后没有残留 renderer 或 JS item。完成图像缓存只保留固定的两主题 C++ 参照，所有图片读回/比较发生在验证点，不进入产品绘制热路径。

`ui-audit` 的 GUI state-set、C++ synchronizeData、paint 的 CPU 纳秒，以及 Qt before/afterSynchronizing / before/afterRendering 时间分别保存。Canvas2D 的 Date.now 脚本计时分辨率约 1ms，0 表示低于分辨率，不能当作零工作；内部 Qt 渲染 CPU 指标另列，包含初始化及诊断提交帧，不能当成纯 paint 的稳态成本。before/afterRendering 是 CPU 提交诊断，GPU timestamp 当前不可用写 null，不表示 GPU 为零。读取和图片比较不计入这些区间。QCPAINTER_DEBUG_COLLECT 关闭：模块诊断定时器会触发无内容失效的 Canvas2D 重绘，不能把它引起的额外绘制当作产品工作。模块内部未公开的提交/显存数据不填写推测值。没有从此微探针推导完整 GUI 提速或稳定 FPS。

## 复建与平台边界

Windows：`./tools/build/verify-r14.ps1`；独立冻结参照：`node tools/review/export-browse-ui.mjs`；三方图形/生命周期：`build/release/tools/ui-audit <output> [--representative]`；实际产品：`build/release/bin/yalahoo --verify-browse <output>`。DPR 由同一运行时查询屏幕因子后校准，报告核对实际值，不能只把环境变量当成 DPR 事实。九个尺寸在 DPR1 完整覆盖；DPR1.5/2 使用 1280×720、930×900、390×844 的代表场景。D3D11/OpenGL 串行运行，所有窗口第一次 show 前置底/拒绝焦点，UI 和产品采集自身 grabWindow；Canvas 在 available/onPaint 的首次工作完成后，从 afterRendering对实际提交的 swapchain 做 QRhi 读回并校验 revision，仅一个在途请求；排空帧以当前后端 MaxAsyncReadbackFrames +1 为上界，完成回调立即停止，静止节点用同一诊断标记确保真实提交。静止场景以比较范围外的1px诊断标记保证一次真实提交；窗口362×162，图像只读取未变的完整360×160 probe 范围，恢复检查使用420×200窗口中的同一内容范围。诊断标记和读回均不是产品代码。Canvas 的 grabWindow 会强制重绘并清空 Canvas2D 命令，因此不能作为成对图形证据。禁止采集桌面。

本地报告、源码/hash 审计矩阵和图像保存在忽略的 `verification/windows/browse-ui/`，macOS/Linux 审计在各自 `*-static/browse-ui/`。审计入口 `tools/audit/audit-r14.ps1` 核对共同 CMake/QML、render-thread 所有权、Metal/OpenGL 分支、字体/资源/插件闭包及同一窗口策略。macOS/Linux 的结论为“静态审计通过，实机待用户验证”；真实平台的 compositor、Retina/字体回退与系统 GPU 仍由用户以后验证。部署 CanvasPainter 动态库、被采用的 QtCanvas2D 插件及 Qt 自带 shader/随附版权归 R30–R32，R14 未制作部署包。

## 同输入图像判定

UI 比较使用完整浏览信息、完整页眉导航和完整页脚组件 ROI；品牌容器锚点单独验收，品牌字形按原创替换排除。保留未经修改的 SSIM/RGB 误差、轮廓距离和差分图，同时把固定一物理像素的参照边缘带单列。边缘带只由冻结参照确定，不从原生差异挑选；页眉导航额外生成8倍源组件轮廓，以区分透明边缘与不透明主题 pill 内的字形边界。参照裁剪范围之外不当作透明像素，裁剪边界不产生虚构轮廓。带外仍采用原 11×11/σ1.5 SSIM ≥0.98、平均 RGB 误差 ≤6 和轮廓距离 ≤1px，报告边缘带比例与误差，不扩大带宽。缺字/位移/不透明内部改变由独立测试确保不能被归一化掩盖。

功能图标保持冻结 SVG 的 viewBox、路径、颜色、端点与线宽，Qt Svg Image 按实际窗口 DPR 栅格化。VectorImage 在小尺寸设置图标上改变了描边/圆点填充叠放，此图标采用相同24px SVG几何的 Qt Quick Shape，横线与两个圆点明确分层；没有增加额外模块依赖。静态控件使用 Text.CurveRendering 与未 hint 的同源字体，fractional CSS 字号只改变局部矢量比例，字形与行框仍按同输入对照；这些路径选择只适用于当前静态控件，R16 另行比较字轮路径。
其余 SVG 功能图标在4倍物理输出尺寸栅格化并由 Qt mipmap 面积滤波；单个17px图标的 RGBA8+mips 约24KiB×DPR²，四个可见栅格图标的上界约96KiB×DPR²（描述符估算，Qt图集/分配对齐另计）；设置图标只用固定Shape节点/缓冲，没有这份纹理。仅主题/DPR或图标内容变化失效，没有每帧重栅格或整屏模糊。

## 本阶段结论

Windows 最新原生 Kit 的 Release 构建、check-format、两组 qmllint 和25项 CTest通过。D3D11/OpenGL、双主题、DPR1/1.5/2、九尺寸及实际产品/绘制生命周期验证通过；60个同输入场景、180个完整UI区域的带外SSIM最低0.98046，带外RGB平均误差最高1.621/255，轮廓距离最高0.254物理像素，主锚点误差最高0.5逻辑像素。原始SSIM、固定1px边缘带及局部成对8倍放大图另存于本地忽略目录，不将带外指标当作未经处理的全图成绩或完整产品性能。macOS/Linux：静态审计通过，实机待用户验证。

## 标题绘制的单上下文边界

R16B1及R16B2的 D3D11/实际DPR1双主题文字、字轮与歌名功能/释放门槛通过，具体原始与固定1px边缘带结果见 title-reels.md。普通整行glyph节点保留当前生产路径；Qt Quick Text和C++ Canvas fillText候选沿用同一布局与时钟，但字面重新shaping的API不能直接复用完整glyph run，不据此宣称复杂连字等价。槽内几何按字体身份/字形/位置复用，隐藏字形路径不继续建节点，层随组件/窗口可见性关闭；没有改变三维资源键。R16B2修复隐藏Canvas仍绘制文字的问题，DPR1三个串行样本中其他路径的Canvas文字绘制计数均为零；仅GUI守卫不能消除已排队的Qt回调，render同步/paint需同时核对可见性，恢复通过窗口可见性连接失效。成本差异和估算/未知资源见 title-reels.md，不推导GPU或完整产品FPS。最终文字绘制选型和其余DPR/后端比较仍由R16B2V/W、R16B3完成。
