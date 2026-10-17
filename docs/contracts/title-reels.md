# 标题与歌名字轮

本文负责标题/译名/歌名字轮的布局、shaping、运动和缓存；字体资产归[assets.md](assets.md)，静态页眉布局归[browse-ui.md](browse-ui.md)，公共帧源归[motion.md](motion.md)。

## 来源与接口

冻结来源 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `music-title-reels.ts`、`music-title.ts`、`music-transport-title.css`、生产 markup/CSS 与 lockfile 指定的 rolling-number，均经 `frozen-git.mjs` 解析。字体沿用已转换的原始 MiSans SFNT 资产，系统彩色 emoji 使用 Qt 字体回退。

`BrowseInfo → TitleReel.qml → TitleReelMotion → TextLayoutAdapter` 接收标题、字体、字号、宽度、垂直预算、窄屏标志与真实窗口 DPR。`QTextLayout` 先完成整行断行/shaping，`QTextBoundaryFinder` 提供 grapheme 边界；按完整 glyph run 的 stringIndexes 合并连字与组合字，不把 UTF-16 code unit 当作字形。主标题最多两行、中文后的拉丁译名另排；大小按源粗搜/细搜拟合，保留实际宽度省略、CSS 行框及下伸部留白。

`TitleReel` 是增量 ListModel，按行/列键复用 `ReelSlot`，空闲槽上限64；快速重定向只保留当前视口相交的字面及新目标，旧字面结束即折叠为目标。FrameClock 已缩放速度，字轮不再次缩放。标题使用460ms、归一rate10、速度限幅12的解析状态与49点显示轨迹；歌名字轮使用来源滚动包的采样、入场 hold/blur、退出透明度。宽度/字体/DPR变化直接重排到明确终态，减少动态立即完成，隐藏时停止活动更新。

`Header → TransportTitle → TransportMotion` 保留640ms宽度与18px归位、400ms透明度、180ms播放图标透明度/缩放及源 cubic-bezier，反向从当前状态开始并缩短反转时长。歌名文本按真实测量省略，Tooltip/Accessible 保留原始全名；按钮仅发出 `locateRequested → actionRequested("locate-song")`，不调用音频 API。停止时保留最后标题用于退出，禁止退出期间点击。实际播放数据来自PlaybackController，定位执行见[library-data.md](library-data.md)；匿名歌名和图标输入仅验证本组件。

## 当前绘制路径与资源

当前生产普通文字由 `ReelInk → ReelTextNode` 提交整行 shaping 后的 glyph ID/位置。GUI 只缓存字体描述、字体 head 身份与字形值；render thread 独立取得 QRawFont，并核对身份，GUI 的 QRawFont/QTextLayout 不跨线程。私有 Qt Quick glyph 接口隔离在 `src/render/rhi/`，配置时探测最新 Kit 的实际接口，不锁版本或回退旧 Qt。彩色 grapheme 使用 Qt Quick Text 的系统字体/彩色缓存；普通字形的隐藏 Text 不创建。整行上下文变化即使文本相同也失效。

Qt Quick Text 与 Canvas Painter 的工具候选使用同一 controller/layout/clock。Canvas `fillText` 按字面重新 shaping，不能直接提交当前完整 glyph run，因此不适合作为存在上下文联结文字的最终生产路径；跨平台最终判定与欠验按后端归计划R34/R35。普通字形节点加彩色Text是当前平台阶段生产路径；全平台最终选型仍待R34/R35。

测量缓存键包含文本、两种字体/字重、字号、宽度、预算、窄屏/DPR/歌名模式；96项 LRU与8MiB描述符计费上限并存，超限结果不缓存。8MiB是缓存计费而非完整进程内存。字体数据库变更清空缓存；同帧输入合并成一个 queued reconcile。布局为输入长度 n 的一次整行 shaping，拟合最多25次候选行框测量，字形提取只在选中候选执行；按 string index 分组为 O(n log n)。动画只访问活动槽，不逐帧创建 QTextLayout 或替换面板；静止入口 O(1)。当前生产字号/视口下，内容 revision 只匹配本槽至多八个字面，以 sameGlyphInk 核对字体身份、glyph ID/位置、缩放和行框，保留仍可见的 QSGTextNode/transform/clip，只创建新增字面并删除离开视口的节点；局部匹配最坏 O(f²)，f≤8，不随曲库增长。clip 容量、颜色和滚动变换独立更新，隐藏的 ReelInk 不创建几何。geometryUpdates 计新增字形节点，geometryReuses 计内容 revision 中的节点复用，均为累计诊断计数，不是逐帧 GPU 成本。

每个可见槽只有一个 Qt Quick RGBA8 layer，物理尺寸 `ceil(capacity × DPR)`，容量按需要增长；源垂直边缘淡化与局部歌名 blur 在共享 qsb 中合成，不使用整屏模糊。`TitleReel.layerSizes()`供显式Review核对QML层请求尺寸与C++计划；不在生产帧循环调用。层只在组件、槽与窗口均可见时开启，SG资源归 Qt 所在线程生命周期。`TitleReel::resourcesChanged → SceneItem::refreshTextResources → RhiInput` 交付值尺寸/revision，`ResourcePlan` 累计颜色与保守 D32FS8 深度上界，既有三维目标兼容性键不受文字内容/位置影响。resize新旧层可重叠，峰值按两份描述符计；Qt字体图集、驱动和私有缓存成本未测，不能当作已知显存。
## 字体定位与隐藏完成

- 普通glyph只传描述符/head身份/ID/位置；彩色grapheme用Quick Text。macOS在QML创建前指定Apple Color Emoji，主字体半行距floor按逻辑CSS px，混合行按实际QTextLine.ascent补偿普通基线；彩色回退基线独立保留。Windows/Linux不套用macOS定位规则。
- 字面范围和宽度终点均截到inkEnd，普通glyph保留整行上下文。源测量probe若computed字号陈旧，替换相同CSS/children的测量根，不改拟合算法。
- naturalWidth仅实际变化通知；caption由存在标题或退出动画状态决定，退出保留完整字面/宽度曲线并禁点击。hide立即停advance，合并一个对象所属queued finish，restore取消陈旧完成。
- 生产ReelInk和比较Canvas均在GUI与render侧核对item/window有效可见性；隐藏不排版/建字形/绘字，窗口替换断旧连接，恢复重发状态。

## 检查与范围

- `ctest --preset qt-release -R '^title-reels$'`覆盖grapheme/连字/组合字、长文省略、缓存容量、20/30/60/120fps/打断/反转/速度/减少动态/复用、宽度通知、hide合并与恢复取消。
- 完整组件检查为20项双主题文字、18项九尺寸页眉、三路径各一次120帧活动/缓存（明确成本对比用`--cost-runs 3`）、隐藏零文字工作、独立一次十次恢复和最终节点/renderer释放，按test-gates.md复用，不与所有DPR/布局交叉重复；轨迹≤1e-6、结构≤1逻辑px。实际播放与定位由[playback.md](playback.md)和[library-data.md](library-data.md)的对应入口验证，匿名歌名事件不能替代这些检查。
- 工具候选Text/fillText重新按字面shaping，不能因共用控制器布局就认定整行语义等价；当前生产为glyph+彩色Text/CurveRendering/实际DPR槽层。候选语义/实际质量与成本分开判断，跨平台选型按后端分工在R34/R35。
- 源数据导出`node tools/review/export-title.mjs --dpr 数值 --output 新目录`；原生驱动`node tools/build/verify-title.mjs --backend <目标后端> --dpr 数值 --source 源目录 --output 新目录`。导出器只生成`source-structure-font-metrics-motion-v1`结构、DOM字体框和数值轨迹，不生成浏览器PNG或放大轮廓；旧报告须重新导出。原生入口不读取参考图，保留自身输出供质量检查，自动passed只表示结构/运动/资源等非图像门槛。
- 静态来源/调用链入口`python3 tools/audit/audit-title-source.py`；每个原生输出目录再执行`node tools/audit/audit-title.mjs 输出目录`，独立核对源DOM字体框、主字体基线与彩色字体身份、同Kit qsb变体。Windows薄入口`verify-r16.ps1`/`audit-r16.ps1`转交相同检查，不得使用诊断放行与像素成绩；详细参数以脚本为准。实际DPR读取窗口报告；`--visible`遵守公共窗口策略。
- macOS Metal/OpenGL已覆盖有效DPR1/1.5/2的隐藏及授权可见组件；有效缩放不等于换用其他物理显示器。原Windows高DPR、共享通知和最终选型欠验分别归R34的D3D11与R35的OpenGL专项，历史像素失败不重新设为补修门槛。当前样本不证明完整产品120秒性能/GPU/屏幕延迟。
