# 标题字轮实现与补验收边界

R16 已接入多行标题、译名与顶栏歌名槽。用户于本轮明确允许把耗时的剩余工作转移到后续轮次；完整绘制选择、跨 DPR/后端视觉和局部成本验收由迁移计划 R16B1–B3 收尾，R17 的前置条件仍等待该轮。本文区分实现与验收，不以开发中的对照图宣称完整视觉通过。

## 来源与接口

冻结来源 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `music-title-reels.ts`、`music-title.ts`、`music-transport-title.css`、生产 markup/CSS 与 lockfile 指定的 rolling-number，均经 `frozen-git.mjs` 解析。字体沿用已转换的原始 MiSans SFNT 资产，系统彩色 emoji 使用 Qt 字体回退。

`BrowseInfo → TitleReel.qml → TitleReelMotion → TextLayoutAdapter` 接收标题、字体、字号、宽度、垂直预算、窄屏标志与真实窗口 DPR。`QTextLayout` 先完成整行断行/shaping，`QTextBoundaryFinder` 提供 grapheme 边界；按完整 glyph run 的 stringIndexes 合并连字与组合字，不把 UTF-16 code unit 当作字形。主标题最多两行、中文后的拉丁译名另排；大小按源粗搜/细搜拟合，保留实际宽度省略、CSS 行框及下伸部留白。

`TitleReel` 是增量 ListModel，按行/列键复用 `ReelSlot`，空闲槽上限64；快速重定向只保留当前视口相交的字面及新目标，旧字面结束即折叠为目标。FrameClock 已缩放速度，字轮不再次缩放。标题使用460ms、归一rate10、速度限幅12的解析状态与49点显示轨迹；歌名字轮使用来源滚动包的采样、入场 hold/blur、退出透明度。宽度/字体/DPR变化直接重排到明确终态，减少动态立即完成，隐藏时停止活动更新。

`Header → TransportTitle → TransportMotion` 保留640ms宽度与18px归位、400ms透明度、180ms播放图标透明度/缩放及源 cubic-bezier，反向从当前状态开始并缩短反转时长。歌名文本按真实测量省略，Tooltip/Accessible 保留原始全名；按钮仅发出 `locateRequested → actionRequested("locate-song")`，不调用音频 API。停止时保留最后标题用于退出，禁止退出期间点击。App尚未接入真实播放数据与定位执行，分别由R24/R23接入；当前歌名和图标检查使用匿名状态输入，不宣称真实播放链路完成。

## 当前绘制路径与资源

当前生产普通文字由 `ReelInk → ReelTextNode` 提交整行 shaping 后的 glyph ID/位置。GUI 只缓存字体描述、字体 head 身份与字形值；render thread 独立取得 QRawFont，并核对身份，GUI 的 QRawFont/QTextLayout 不跨线程。私有 Qt Quick glyph 接口隔离在 `src/render/rhi/`，配置时探测最新 Kit 的实际接口，不锁版本或回退旧 Qt。彩色 grapheme 使用 Qt Quick Text 的系统字体/彩色缓存；普通字形的隐藏 Text 不创建。整行上下文变化即使文本相同也失效。

Qt Quick Text 与 Canvas Painter 的工具候选使用同一 controller/layout/clock。Canvas `fillText` 按字面重新 shaping，不能直接提交当前完整 glyph run，因此不适合作为存在上下文联结文字的最终生产路径；其他像素/成本差异仍需 R16B1–B3 完整判定。目前普通字形节点加彩色 Text 是已集成方案，尚未宣称最终比较选型通过。

测量缓存键包含文本、两种字体/字重、字号、宽度、预算、窄屏/DPR/歌名模式；96项 LRU与8MiB描述符计费上限并存，超限结果不缓存。8MiB是缓存计费而非完整进程内存。字体数据库变更清空缓存；同帧输入合并成一个 queued reconcile。布局为输入长度 n 的一次整行 shaping，拟合最多25次候选行框测量，字形提取只在选中候选执行；按 string index 分组为 O(n log n)。动画只访问活动槽，不逐帧创建 QTextLayout 或替换面板；静止入口 O(1)。几何内容变化时当前实现重建该槽的文字节点，保留字面几何的进一步增量复用须在R16B1–B3审计并验收。

每个可见槽只有一个 Qt Quick RGBA8 layer，物理尺寸 `ceil(capacity × DPR × 2)`，容量按需要增长；源垂直边缘淡化与局部歌名 blur 在共享 qsb 中合成，不使用整屏模糊。层只在可见时开启，SG资源归 Qt 所在线程生命周期。`TitleReel::resourcesChanged → SceneItem::refreshTextResources → RhiInput` 交付值尺寸/revision，`ResourcePlan` 累计颜色与保守 D32FS8 深度上界，既有三维目标兼容性键不受文字内容/位置影响。resize新旧层可重叠，峰值按两份描述符计；Qt字体图集、驱动和私有缓存成本未测，不能当作已知显存。

## 验证入口与限制

实现检查：`./tools/build/verify-r16.ps1`。完整补验收：`./tools/build/verify-r16.ps1 -FullMatrix`。源码审计：`./tools/audit/audit-r16.ps1`。单元测试为 CTest `title-reels`，覆盖 grapheme、长文本/省略、缓存上限、20/30/60/120fps解析与打断、槽复用、改速/减少动态/失效及歌名过渡反转。完整对照工具还保留同输入图像、源轨迹、三次活动采样、页眉点击与十次释放检查；未完成矩阵不能记为通过。

所有验证窗口使用 VerificationWindow，第一次 show 前置底/拒绝焦点，只读取自己的 framebuffer；浏览器 headless、子进程隐藏且拦截非本地请求。QRhi 只有一个在途读回，排空受 MaxAsyncReadbackFrames 约束；退出/超时在读回状态存活期间销毁 graphics surface。GUI tick、同步、CPU绘制提交分开记录，GPU timestamp不可用写null，不能推导完整产品FPS。截图、逐帧数据、日志和内容hash审计矩阵只在忽略的verification目录。

`WEB-TEXT-PROBE-01`：Chromium 对源测量 probe 的 fine 字号更新出现 inline style 与 computed style 不一致，导致采集沿用 coarse 字号。导出器以完全相同 inline CSS 克隆测量根，仅消除陈旧观测状态，原算法、常量和生产CSS不变；未刷新的原始数据单独保留。它是采集问题，不是复制到原生的字体尺寸目标，完整视觉矩阵仍需补验收。

Windows通过范围以本轮实际实现检查结论为准。macOS/Linux对共享CMake、Qt事件/字体值交接、qrc、Metal/OpenGL与qsb变体作静态审计；结论为“静态审计通过，实机待用户验证”。目标平台字体回退/彩色emoji、编译器/私有接口、compositor与像素结果不能由Windows代替。部署仍按R30–R32的平台清单，未在本轮生成部署包。

本轮实现检查：Windows最新原生Kit Qt6.12.0/MSVC的Release构建、27项CTest、两组qmllint、check-format及维护脚本格式检查通过；最后的行末空白修正另通过title-reels/source-format，生产D3D11/DPR1九尺寸×双主题共18项检查通过，播放ID保持不变。qsb包含GLSL/HLSL/MSL变体；三平台实现源码审计通过，完整选型仍待补验收。

本地单上下文标题诊断的源轨迹最大误差5.69e-13，三种路径各三次活动采样未逐帧重排，18项页眉检查及十次释放检查通过；整体报告为失败，原因是行末空白并入前字面导致结构文本比较不一致。实现已限制字面文本至有效行末，最终完整结构对照尚未重跑，保留失败报告交由R16B1，不把像素带外门槛通过当作整体通过。本轮显著超时，用户新增的规模/时间约束已写入计划，后续轮次须在开始前按代理实际能力拆分并评估。
