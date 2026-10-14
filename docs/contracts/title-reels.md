# 标题字轮实现与补验收边界

R16 已接入多行标题、译名与顶栏歌名槽。D3D11/DPR1 补验收见下文；完整绘制选择、其余 DPR/后端视觉和局部成本验收由迁移计划 R16B2V-P/W、R16B3 收尾，R17 的前置条件仍等待这些轮次。本文区分实现与验收，不以开发中的对照图宣称完整视觉通过。

## 来源与接口

冻结来源 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `music-title-reels.ts`、`music-title.ts`、`music-transport-title.css`、生产 markup/CSS 与 lockfile 指定的 rolling-number，均经 `frozen-git.mjs` 解析。字体沿用已转换的原始 MiSans SFNT 资产，系统彩色 emoji 使用 Qt 字体回退。

`BrowseInfo → TitleReel.qml → TitleReelMotion → TextLayoutAdapter` 接收标题、字体、字号、宽度、垂直预算、窄屏标志与真实窗口 DPR。`QTextLayout` 先完成整行断行/shaping，`QTextBoundaryFinder` 提供 grapheme 边界；按完整 glyph run 的 stringIndexes 合并连字与组合字，不把 UTF-16 code unit 当作字形。主标题最多两行、中文后的拉丁译名另排；大小按源粗搜/细搜拟合，保留实际宽度省略、CSS 行框及下伸部留白。

`TitleReel` 是增量 ListModel，按行/列键复用 `ReelSlot`，空闲槽上限64；快速重定向只保留当前视口相交的字面及新目标，旧字面结束即折叠为目标。FrameClock 已缩放速度，字轮不再次缩放。标题使用460ms、归一rate10、速度限幅12的解析状态与49点显示轨迹；歌名字轮使用来源滚动包的采样、入场 hold/blur、退出透明度。宽度/字体/DPR变化直接重排到明确终态，减少动态立即完成，隐藏时停止活动更新。

`Header → TransportTitle → TransportMotion` 保留640ms宽度与18px归位、400ms透明度、180ms播放图标透明度/缩放及源 cubic-bezier，反向从当前状态开始并缩短反转时长。歌名文本按真实测量省略，Tooltip/Accessible 保留原始全名；按钮仅发出 `locateRequested → actionRequested("locate-song")`，不调用音频 API。停止时保留最后标题用于退出，禁止退出期间点击。App尚未接入真实播放数据与定位执行，分别由R24/R23接入；当前歌名和图标检查使用匿名状态输入，不宣称真实播放链路完成。

## 当前绘制路径与资源

当前生产普通文字由 `ReelInk → ReelTextNode` 提交整行 shaping 后的 glyph ID/位置。GUI 只缓存字体描述、字体 head 身份与字形值；render thread 独立取得 QRawFont，并核对身份，GUI 的 QRawFont/QTextLayout 不跨线程。私有 Qt Quick glyph 接口隔离在 `src/render/rhi/`，配置时探测最新 Kit 的实际接口，不锁版本或回退旧 Qt。彩色 grapheme 使用 Qt Quick Text 的系统字体/彩色缓存；普通字形的隐藏 Text 不创建。整行上下文变化即使文本相同也失效。

Qt Quick Text 与 Canvas Painter 的工具候选使用同一 controller/layout/clock。Canvas `fillText` 按字面重新 shaping，不能直接提交当前完整 glyph run，因此不适合作为存在上下文联结文字的最终生产路径；其余 DPR/后端像素与成本差异仍需 R16B2V-P/W、R16B3 完整判定。目前普通字形节点加彩色 Text 是已集成方案，尚未宣称最终比较选型通过。

测量缓存键包含文本、两种字体/字重、字号、宽度、预算、窄屏/DPR/歌名模式；96项 LRU与8MiB描述符计费上限并存，超限结果不缓存。8MiB是缓存计费而非完整进程内存。字体数据库变更清空缓存；同帧输入合并成一个 queued reconcile。布局为输入长度 n 的一次整行 shaping，拟合最多25次候选行框测量，字形提取只在选中候选执行；按 string index 分组为 O(n log n)。动画只访问活动槽，不逐帧创建 QTextLayout 或替换面板；静止入口 O(1)。当前生产字号/视口下，内容 revision 只匹配本槽至多八个字面，以 sameGlyphInk 核对字体身份、glyph ID/位置、缩放和行框，保留仍可见的 QSGTextNode/transform/clip，只创建新增字面并删除离开视口的节点；局部匹配最坏 O(f²)，f≤8，不随曲库增长。clip 容量、颜色和滚动变换独立更新，隐藏的 ReelInk 不创建几何。geometryUpdates 计新增字形节点，geometryReuses 计内容 revision 中的节点复用，均为累计诊断计数，不是逐帧 GPU 成本。

每个可见槽只有一个 Qt Quick RGBA8 layer，物理尺寸 `ceil(capacity × DPR × 2)`，容量按需要增长；源垂直边缘淡化与局部歌名 blur 在共享 qsb 中合成，不使用整屏模糊。层只在组件、槽与窗口均可见时开启，SG资源归 Qt 所在线程生命周期。`TitleReel::resourcesChanged → SceneItem::refreshTextResources → RhiInput` 交付值尺寸/revision，`ResourcePlan` 累计颜色与保守 D32FS8 深度上界，既有三维目标兼容性键不受文字内容/位置影响。resize新旧层可重叠，峰值按两份描述符计；Qt字体图集、驱动和私有缓存成本未测，不能当作已知显存。

## 验证入口与限制

实现检查：`./tools/build/verify-r16.ps1`。完整补验收：`./tools/build/verify-r16.ps1 -FullMatrix`。源码审计：`./tools/audit/audit-r16.ps1`。单元测试为 CTest `title-reels`，覆盖 grapheme、长文本/省略、缓存上限、20/30/60/120fps解析与打断、槽复用、改速/减少动态/失效及歌名过渡反转。单上下文入口为 `./tools/build/verify-r16.ps1 -TitleContext -Backend d3d11 -Dpr 1 -EvidenceDirectory verification/windows/title-reels/r16b1-accepted`；Backend 可选 d3d11/opengl，Dpr 可选 1/1.5/2，报告核对实际后端与校准后的真实 DPR。完整对照保留同输入图像、源轨迹、三次活动采样、页眉点击与十次释放检查；单上下文通过不等于完整矩阵通过。

所有验证窗口使用 VerificationWindow，第一次 show 前置底/拒绝焦点，只读取自己的 framebuffer；浏览器 headless、子进程隐藏且拦截非本地请求。QRhi 只有一个在途读回，排空受 MaxAsyncReadbackFrames 约束；退出/超时在读回状态存活期间销毁 graphics surface。GUI tick、同步、CPU绘制提交分开记录，GPU timestamp不可用写null，不能推导完整产品FPS。截图、逐帧数据、日志和内容hash审计矩阵只在忽略的verification目录。

`WEB-TEXT-PROBE-01`：Chromium 对源测量 probe 的 fine 字号更新出现 inline style 与 computed style 不一致，导致采集沿用 coarse 字号。导出器以完全相同 inline CSS 克隆测量根，仅消除陈旧观测状态，原算法、常量和生产CSS不变；未刷新的原始数据单独保留。它是采集问题，不是复制到原生的字体尺寸目标，完整视觉矩阵仍需补验收。

Windows通过范围以本轮实际实现检查结论为准。macOS/Linux对共享CMake、Qt事件/字体值交接、qrc、Metal/OpenGL与qsb变体作静态审计；结论为“静态审计通过，实机待用户验证”。目标平台字体回退/彩色emoji、编译器/私有接口、compositor与像素结果不能由Windows代替。部署仍按R30–R32的平台清单，未在本轮生成部署包。

## R16B1 单上下文结论与交接

Windows Qt 6.12.0/MSVC 19.51 的 Release、D3D11、校准实际 DPR1 验收通过：10 个匿名输入×双主题的20项生产标题、18项九尺寸页眉、三种绘制候选各三次120帧活动诊断、十次 hide/release/restore 与最终零 tracked node/renderer 释放，以及实际生产浏览18项检查通过。27项 CTest（含 title-reels/source-format）、两组 qmllint、check-format、维护脚本格式和 qsb GLSL/HLSL/MSL 检查通过。宏观产品性能及 DPR1.5/2、OpenGL 未由这些结果验收。

结构最大误差0.24087逻辑px，源码轨迹误差5.69e-13。未经归一化的原始文字 ROI SSIM最低0.66549、平均RGB误差最高25.027/255；固定1物理px源轮廓边缘带单列后，带外SSIM最低0.98172、RGB误差最高0.94975/255，原门槛通过。边缘带最大比例44.35%，不把带外成绩当作原始全图成绩；小字号曲线栅格与Chromium的字形边缘覆盖差异保留在原始成对图中。WEB-TEXT-PROBE-01的刷新声明保留，旧 web-raw 与失败报告不改写。

新回归比较整行 QTextLayout 的 glyph ID/位置与字面关联，覆盖组合字与断行空白。行末文本范围与宽度端点均限至有效 inkEnd，避免把空白 advance 并入前字面。当前生产普通 glyph 加彩色 Text 路径满足本上下文门槛；两个工具候选仍按字面 fillText/Text 重新 shaping，不能提交相同整行 glyph run，因此不能把共享控制器的结构数据当作候选绘制的整行 shaping 等价证明，最终选型仍待R16B3。

活动诊断的生产路径三次分别新建68/82/82个字形节点、复用73/65/65次；这是包含候选切换/初始化的累计节点诊断，不能当作稳态提交成本或FPS提升。两个隐藏生产字形路径的新建计数均为零；活动tick无逐帧布局miss，静止 advance 为O(1)。槽内匹配、layer显隐、Qt线程所有权、缓存/资源容量及共同平台链按PERF-01–07审计；Qt字体图集/驱动内存和GPU时长仍未知，GPU值保持null。

页眉使用真实 LibrarySession 的匿名专辑/曲目 ID 状态变更，完成两次换歌与专辑导航、0.25×/3×途中改速、开启减少动态和可见退出期间禁用点击检查。先以减少动态准备初始终态，在输入前的已呈现帧建立位置基线；按钮在换歌/改速检查中保持位置，视觉退出期另核对禁用与保留完整字面。QML直接读取实际ToolTip/Accessible附加属性验证完整歌名；点击只发locate-song，导航前后播放ID及播放通知计数不变。本轮不播放音频，实际PlaybackController及定位执行仍分别归R24/R23。

证据与失败样本均在忽略的 verification/windows/title-reels/ 下；静态审计入口 `./tools/audit/audit-r16.ps1 -Round R16B1` 为新增节点复用、可见性与释放链生成独立源码hash矩阵。macOS/Linux：静态审计通过，实机待用户验证。共享Qt事件、字体值/身份、QML/qrc、Metal/GL及qsb链已展开；目标平台编译器、彩色字体、compositor与GPU像素未运行。

## R16B2 验收入口、成本隔离与DPR1交接

单后端三DPR入口为 `./tools/build/verify-r16.ps1 -BackendMatrix -Backend d3d11`；单上下文保留 `-TitleContext -Backend d3d11 -Dpr 1`。`-UseExistingWebSource`仅复用已经生成且含所需每DPR20项的冻结参照，缺失项直接失败，原生仍逐报告核对实际DPR/后端及完整检查数量。受限环境中先在原生权限下执行 `node tools/review/export-title.mjs`，再用此复用参数验证；完整矩阵失败不会产生通过summary。摘要保留匿名输入、参照和二进制hash，不提交原始产物。

Canvas候选的GUI失效仅在有效可见时update；render同步同时核对item/window可见性，隐藏时不读取faces，paint在reset后返回。窗口替换断开旧连接、恢复重发当前状态。Qt初始化/空清理回调仍可能发生，`canvasPaints`及Canvas局部纳秒只计可见文字绘制/同步；整体Qt同步/提交区间包含框架工作。原直接连接controller.changed的隐藏Canvas在其他路径活动期间仍绘制96–120次，修复前成本不能作为公平比较。修复后普通glyph与Text路径的三次Canvas文字绘制均为零。

Windows Qt 6.12.0/MSVC 19.51、Release/D3D11/实际DPR1：20项双主题生产文字、18项九尺寸页眉、三路径各三次120帧活动诊断、十次恢复与最终零tracked节点/renderer、生产浏览18项通过；27项CTest、source-format/check-format、两组qmllint、维护脚本格式及qsb GLSL/HLSL/MSL通过。结构最大误差0.24087逻辑px；原始文字SSIM最低0.66549/RGB最高25.027，固定1物理px带外SSIM最低0.98172/RGB最高0.94975，最大边缘带比例44.35%。原始与带外结果分别解释。

下表为每组120个诊断帧的CPU累计毫秒，依次列出三次原始结果，不池化或挑样；包含模式切换、首次准备及诊断提交，活动输入为暖昼，不能当作稳态GPU或完整播放器FPS。两主题文字/页眉另由上述静态与行为矩阵覆盖。

| 路径 | GUI准备 | 活动tick | Qt同步 | CPU绘制提交 |
| --- | --- | --- | --- | --- |
| 整行glyph＋彩色Text | 3.8304 / 0.3878 / 0.3913 | 9.6765 / 6.9968 / 6.5551 | 9.7453 / 5.7093 / 6.8709 | 39.6602 / 25.7749 / 28.1475 |
| Qt Quick Text候选 | 1.8687 / 2.4260 / 1.1347 | 8.6823 / 12.7970 / 10.1289 | 6.8295 / 6.0645 / 6.9053 | 24.3915 / 23.9880 / 22.9133 |
| Canvas候选 | 0.6254 / 0.4625 / 0.4408 | 5.8813 / 4.7928 / 4.9698 | 1.6805 / 1.7513 / 1.7873 | 9.6360 / 10.3160 / 11.7763 |

Canvas三次文字同步为0.2661/0.2686/0.2709ms，paint为6.4246/6.9305/7.7498ms，实际文字paint次数98/97/97；它们是Qt区间的局部计量，不与Qt区间相加。各路径共享布局描述符缓存计费100314bytes，控制器所描述槽层RGBA8估算383408bytes；Canvas模式不实际启用这些隐藏槽层，不能把共享描述符估算当成该候选显存。可见Canvas的300×150逻辑范围、DPR1单采样RGBA8附件估算180000bytes，深度上界按8bytes/像素另计，resize新旧可重叠；Qt字体图集/内部缓存/驱动实际内存未知。生产节点新建68/82/82、复用73/65/65，两个隐藏生产路径新建为零。GPU时间全部null。

局部CPU结果支持本上下文中Canvas提交成本较低，但其按字面重新shaping与当前整行glyph语义有差异，不能据此选为生产路径。DPR1.5修复前有7项生产带外SSIM失败，最低约0.95282；DPR1.5/2与NativeRendering试验曾超时、无最终报告，未通过。生产仍用CurveRendering。原始失败保存在`verification/windows/title-reels/r16b2/`，补修任务为R16B2V-P/W，最终选型仍为R16B3。工具超时另写阶段、窗口与readback状态；DPR²完成预算只用于CPU物理像素比较的验证时限，不放宽图像或性能门槛。

本轮源码PERF-01–07、字体身份/值交接、QML/qrc层资源、窗口恢复、Metal/GL和qsb闭包审计入口为 `./tools/audit/audit-r16.ps1 -Round R16B2`。macOS/Linux：静态审计通过，实机待用户验证；未运行目标编译器、字体回退、compositor或GPU，不能由本轮Windows结果推断。

## R16B2V 恢复/混合字体补修与剩余像素任务

本轮按时间约束收敛到恢复驱动、普通字形基线及完整诊断，原像素收口拆为R16B2V-P；本轮实际耗时超过一小时目标，不将其记为规模/耗时达标。生产仍使用CurveRendering和两倍DPR层；NativeRendering、四倍层、mipmap与分数字号/度量试验全部撤回，失败样本只保留在本地。没有完成DPR2或OpenGL验收，也没有完成最终文字路径选型。

普通字形的CSS半行距仍用主字体度量，QTextLine的position减去实际混合行ascent，避免emoji回退改变普通字形baseline。彩色Text描述符补回fallback ascent差，保持原回退绘制基线。字号拟合、断行、整行glyph ID/位置关联、缓存键/容量、时钟和层资源均不变。新单元回归的普通glyph竖向位置允许Qt QFixed自身一个1/64逻辑px量化步长；这不修改原结构、像素或运动门槛。

生命周期测试从最终超宽页眉窗口恢复到300/320px标题场景，root与window尺寸一起恢复，在sceneGraphInvalidated后排队showNormal。仅等待invalidation仍会停在step88；场景尺寸修正后十次恢复及最终释放完成。Qt/compositor内部停帧原因仍未确认，超时单列visible/exposed/restorePending/readback状态，不能归因于CPU图像比较或通过抢焦点取样。GUI进程等待与退出码使用RunGui的Process.ExitCode；管线结束后的LASTEXITCODE曾沿用console诊断的6，不能当作GUI退出事实。

Windows Qt 6.12.0/MSVC 19.51 Release：D3D11/实际DPR1.5的20项文字结构/文本与源码轨迹、18项九尺寸页眉、三路径各三次120帧成本/缓存、隐藏Canvas文字零绘制、十次恢复和最终零tracked node/renderer通过；实际生产浏览18项通过，GUI进程真实退出0。固定1物理px带外像素仍有case6/7、14/15、17五项失败，最低SSIM0.97892，最高RGB平均误差1.90992/255；case10/11修正后SSIM0.98754/0.98852。原始ROI最低SSIM0.77191、最高RGB平均误差16.27161/255，不能把带外数字当原图成绩。正常TitleContext仍拒绝像素失败；TitleDiagnostics只允许结构/行为/成本/释放都通过后保存diagnosticsPassed=true、passed=false和pendingPixelCases，不能用来收口R16B2V-P。

受影响DPR1完整回归通过：20项文字的带外SSIM最低0.98773，18项页眉、三路径各三次成本、十次恢复/释放和18项生产浏览通过。27项Release CTest（含source-format/title-reels）、两组qmllint、check-format、维护脚本格式与qsb GLSL/HLSL/MSL检查通过。DPR1.5最终采集后仅修正独立单元断言，UI二进制未改变；结构/行为/成本/释放报告继续从原始完整报告核对，Release CTest重新通过。

换机保留的五项最小失败数值如下，来源为上述 Qt 6.12.0/MSVC 19.51 Release、D3D11/实际DPR1.5 的最终完整报告，生产 mode=0；这是带外归一化指标，不是原始 ROI 分数或 macOS 成绩。案例编号解析已入库 `tests/fixtures/text/titles.json`，无需将逐帧 JSON/PNG 改名为输入入库。

| case | 带外 SSIM（门槛≥0.98） | RGB 平均误差 /255 | 最大结构误差 /逻辑px |
| --- | --- | --- | --- |
| 6 | 0.979641055 | 1.632645503 | 0.046875 |
| 7 | 0.979015593 | 1.909911817 | 0.046875 |
| 14 | 0.979517407 | 1.474845679 | 0.1177675 |
| 15 | 0.978920067 | 1.678155007 | 0.1177675 |
| 17 | 0.978975576 | 1.139508148 | 0.2025 |

新机器没有旧采集目录时，先运行 `node tools/review/export-title.mjs --dpr 1.5` 重建参照，再运行下文 Windows 诊断；它读取冻结 Git 源码和原字体，不依赖旧机器缓存。Mac 接续和同机 Metal 来源入口见 [macos.md](../platforms/macos.md)，不能用 Mac 重建结果回填这些 Windows 数值。

下表是DPR1.5每组120帧的CPU累计毫秒，依次列三次独立结果；包含初始准备/切换与诊断提交，不池化、不推导GPU或产品FPS。

| 路径 | GUI准备 | 活动tick | Qt同步 | CPU绘制提交 |
| --- | --- | --- | --- | --- |
| 整行glyph＋彩色Text | 3.3031 / 0.3662 / 0.3718 | 15.9764 / 10.4975 / 11.2053 | 10.8445 / 5.3010 / 7.0073 | 36.1749 / 23.7429 / 27.7291 |
| Qt Quick Text候选 | 1.3013 / 1.1000 / 1.2422 | 13.4301 / 10.8939 / 11.6773 | 6.5179 / 7.3664 / 6.0768 | 21.4267 / 25.1163 / 22.5337 |
| Canvas候选 | 0.4375 / 0.4405 / 0.4609 | 6.7877 / 7.1306 / 5.6132 | 1.8594 / 1.8702 / 1.7645 | 11.3856 / 12.1404 / 11.4580 |

Canvas三次文字paint均96次，局部sync为0.2898/0.2770/0.2616ms、paint为7.6962/8.2193/7.6596ms；其他路径Canvas文字paint均零。共享控制器描述符缓存101156bytes、生产槽RGBA8层估算858552bytes；这不是Canvas/Text候选的实际显存，更不含字体图集/驱动或resize新旧峰值。GPU时长全部null，未测内存保持未知。PERF-01–07及共同字体值/窗口/qrc/Metal/GL静态链已审计，macOS/Linux为“静态审计通过，实机待用户验证”。

复现诊断：`./tools/build/verify-r16.ps1 -TitleContext -TitleDiagnostics -Backend d3d11 -Dpr 1.5 -UseExistingWebSource -EvidenceDirectory verification/windows/title-reels/r16b2v/accepted`；完整DPR1回归使用相同入口去掉TitleDiagnostics、设Dpr 1及独立dpr1-regression目录。静态入口为`./tools/audit/audit-r16.ps1 -Round R16B2V`，要求声明的五个像素待修项和完整DPR1回归，不将诊断摘要冒充像素通过。

## Apple Silicon 字体身份与真正离屏诊断（R16M1/M2）

本机Qt 6.12.0/AppleClang 21.0.0、arm64、macOS 27.0.1、Release、Metal/实际DPR2；同机Edge 155.0.4283.45从冻结源码生成20项参照。QQuickRenderControl渲染同一QML/生产字轮到隐藏Metal目标，显式DPR2，没有原生show、swapchain呈现或正常VSYNC。FrameClock/源轨迹、CurveRendering、两倍DPR层与原像素门槛不变。

R16M1的case10/11曾累积advance误差16.078125逻辑px。实际CDP字体身份为`Apple Color Emoji / AppleColorEmoji`，Qt却选`.Apple Color Emoji UI`；28px下前者advance为28px，后者约32px，加上源-0.6px字距后差4px。macOS初始化用公开`QFontDatabase::setApplicationEmojiFontFamilies`指定原系统字体面，缺family显式回滚/失败，Windows/Linux策略不变。整行shaping及普通字形基线未改，彩色Text与测量共享应用字体策略。该API依据为[Qt字体数据库公开文档](https://doc.qt.io/qt-6/qfontdatabase.html#setApplicationEmojiFontFamilies)。

原生诊断直接保留测量run的head hash、glyph ID/position，CDP记录真实字面与family/PostScript；不把从私有family描述符重新解析出的替代字体度量当作测量run。当前Apple面head SHA256为`bd2ad7ff0c0cff60a1ceff914f9fbb81a0197cf26472a35721912863f8501e13`，四个彩色grapheme的整行ID为3237/1139/423/822。这些标识用于本机诊断，不锁定以后系统字体版本。四种字号（12/28/29.75/44）与三个DPR输入的字体/advance回归在修复前失败、修复后通过；不等同于三个DPR的像素验收。

R16M2完整诊断：20项生产结构/文本与源码轨迹通过，最大结构误差0.240865逻辑px；case10/11误差0.078125逻辑px，固定1物理px带外SSIM为0.999285581/0.999253834，RGB平均误差为0.079922/0.079038（0–255单位）。原始ROI SSIM为0.962329861/0.943559874、RGB为2.439160/3.169872，原始与带外分别解释。18项页眉、定位不改变播放、三路径各三次120帧有界活动/缓存检查、隐藏Canvas零文字绘制、十次离屏恢复、最终零tracked节点/renderer及隐藏原生窗口策略通过。缓存计费99546bytes，生产槽RGBA8层描述符估算1524220bytes；字体图集/驱动内存未测，GPU时长仍为null，离屏CPU不代表产品threaded render-loop或窗口性能。

完整诊断正常退出，明确`diagnosticsPassed=true`、`passed=false`及六项剩余像素失败；R16M2P处理以下固定1物理px带外分数，不能用诊断豁免像素收口。Windows D3D11/OpenGL门禁保留，macOS可见窗口验证归最后R34。

| case | 带外SSIM | 带外RGB平均误差 /255 | 几何最大误差 /逻辑px |
| --- | --- | --- | --- |
| 6 | 0.974967633 | 1.733680556 | 0.046875000 |
| 7 | 0.972839274 | 1.970763889 | 0.046875000 |
| 14 | 0.939155120 | 2.469695216 | 0.117767500 |
| 15 | 0.932594943 | 2.812181713 | 0.117767500 |
| 16 | 0.942828136 | 1.897446667 | 0.202500000 |
| 17 | 0.938856444 | 2.214410000 | 0.202500000 |

重建参照：`node tools/review/export-title.mjs --dpr 2 --output verification/macos/title-reels/r16m2/source`。完整诊断：`node tools/build/verify-title.mjs --backend metal --dpr 2 --source verification/macos/title-reels/r16m2/source --output 新目录 --diagnostics`。每次原生运行使用新目录，正常像素收口去掉`--diagnostics`。原R16M1报告和失败图仍在本地`verification/macos/title-reels/r16m1/final/`；R16M2参照、报告及成对图保持忽略，不提交机器输出。

源码/性能审计：`node tools/audit/audit-title.mjs 完整诊断目录`。它要求完整隐藏Metal/DPR2诊断及同机字体证据，校验文本qsb的MSL/GLSL/HLSL，按已人工推演的COMMON/PERF-01–07调用链保存两个平台的源码hash矩阵。macOS/Linux本阶段结论为“静态审计通过，实机待用户验证”；本机真实arm64离屏结果单列，不外推可见窗口性能或Linux/Windows。Release全部23项启用CTest、source-format/check-format、两个qmllint与qsb变体通过；四项可见runtime测试disabled，按R34暂缓，未记为通过。R17仍等待R16最终选型收口。
