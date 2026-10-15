# 冻结源码复查与交接

判定以迁移计划的源码绝对基准为准。本文只保存必要结论、历史归属和复现入口；原图、逐帧数据、逐机 hash 矩阵及日志在忽略的 `verification/` 中。`待 R19…` 表示尚未完成对应组件复查，不能当作通过。

## R19A 范围与来源

复查起点为 `279d9a7`。通过 `git log --reverse -- QT_QUICK_MIGRATION_PLAN.md`、各实现提交前的计划和实现 diff 恢复任务，而非只读取已删除小节后的当前计划。原冻结身份 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 由 `history-rewrite.json` 映射到 `df251a2eb0406b1ccde11a0e9f8a5017c0293185`；不改写原来源标识。Node/PowerShell 分别使用 `execFrozenGit`/`ResolveFrozenRevision`。资产 C++ 入口现在也在写入前解析同一映射并核对全部输入。

`check-web-baseline.mjs` 实际核对 88 个 `src/` blob 与归档文件、47 项准备后资产、24 个冻结资产输入、36 项原创品牌文件，以及匿名库/列顺序/数值输入的内部约束。它不再读取截图清单、`web-scenes/`、成功采集报告或 Room GPU 字节，也不要求它们存在。检查成功仅证明这些范围；运动/投影等公式仍由所属轮次独立核对。

| 原轮次 | 引入及后续历史 | 实际对象、当前判定与复查归属 |
| --- | --- | --- |
| R01 | `a48211c`；`21ef751`、`c7e9b61`、`e98c798` | `source-map.json`、`export-web-baseline.mjs`、`review-hook.txt`、冻结映射；源码清单/隔离匿名输入有效，旧 GPU/图像采集不再约束基础检查。R19A 审查，组件参数继续下列各轮。 |
| R02 | `24b125a`；`6740abc`、`c7e9b61`、`e98c798`、`1c9b8c6` | `QtKit.cmake`、`Cxx23.cmake`、`ApplicationIdentity.h`、模块/qsb/presets；最新原生 Kit、强制 C++23、同 Kit 工具与默认隐藏策略保留。R19A 审查；渲染 API 适配归 R19D，文字私有 API 归 R19F。 |
| R03 | `83f933c`；`5b2df80`、`6740abc`、`c7e9b61` | `PrepareAssets`、`GeometryAudit`、`AssetCatalog` 与资源清单；原始资产/字体表/模型几何未发现图像驱动变形；补齐 POSIX/直接调用的写入前来源校验。R19A 审查。 |
| R03B | `598c4a8`；`c7e9b61` | 真 SVG 主/小稿、揭示映射、平台容器；原创品牌与小尺寸光学校准来自用户明确要求，保留。衍生资产 hash/容器检查有效，R19A 审查；页眉/开场容器归 R19E/R20。 |
| R04 | `739ee97`；`d03527e`、`b7f73d4`、`0d06b7b`、`1a082f2`、`2ee2294`、`2abb4f6`、`33e8260`、`1c9b8c6` | `MusicTypes`、`LibraryModels`、`LibrarySession`、`OfflineMetadataProvider`；保留本地 URL、字段/稳定 ID、排序/搜索、导航与播放隔离和异步离线结果。R19A 审查；布局/导航状态消费者归 R19B/E/G。 |
| R05 | `ec0af92`；`d03527e`、`ed5aaef` | `FrameClock`、阻尼/五次轨迹、改速/减少动态与 `cancelTravel`；R19B 已按原公式和实际调用顺序审查，结论见下节。 |
| R06 | `5102cb0`；`5b2df80`、`e98c798` | GLB 坐标/UV 的资产事实由 R19A 确认；`LayoutMetrics`、相机/投影/拾取由 R19B 审查；geometryOnly 像素保护属于设备分配适配，最终采样与目标尺度仍待 R19D。 |
| R07 | `1556a55`；`52f1278`、`5625d73`、`ed5aaef` | `ShellMaterial`、主盖/侧脊参数和有效override由R19C1复查，ROI系数移除；shader/捕获/LOD剩余项归R19C2。 |
| R08 | `178ca45`；`52f1278`、`5625d73`、`e98c798` | `MusicLighting`的主题/单位/轨迹参数由R19C1复查；Room/PMREM/BRDF/方向的shader实现与平台采样归R19C2。 |
| R09 | `3fc6a39`；`1f2352a`、`75775b3` | SSAO/depth/normal、核/box与数据边由R19D1复查；阴影/偏置/滤波仍R19D3。 |
| R09B | `522a3f3`；`1f2352a`、`75775b3`、`e98c798` | Bokeh/SMAA/MSAA、曝光/ACES/sRGB 与目标尺度待 R19D。 |
| R10 | `d06e5f3` | Quick3D/RHI 能力探测和历史选型理由待 R19D；不恢复旧图像分数门禁。 |
| R10A | `52f1278`；`5b2df80`、`d03527e` | opaque/纸面的入射参数与toneMapped接口由R19C1复查；最终shader语义归R19C2，阴影/资源图/采样归R19D。 |
| R10B | `5625d73`；`d03527e`、`e98c798` | 材质参数迁移及R18独立clarity由R19C1复查；透明捕获/排序和完整R10B归R19C2，MSAA/平台附件归R19D。 |
| R10C | `1f2352a`；`d03527e`、`f7e423a` | 图集租约由R19A确认，光学capture/mips由R19C2B/C复查，AO及常量数据由R19D1复查；剩余postprocess/集成仍D2–D4。 |
| R10D | `75775b3`；`b7f73d4`、`e98c798` | `SceneItem`透明重叠由R19C2C复查，标量AO/读回由R19D1复查；目标/其他资源仍D2–D4，历史性能只记录的用户边界保留。 |
| R11 | `f7e423a`；`b7f73d4`、`ed5aaef` | `CoverCache`/`AtlasAllocator`/`SelectedCoverPool`/`AtlasUploadQueue`；尺寸、颜色、所有权与有界工作有效；修正缺图脚注字重。R19A 审查；最终采样/合成待 R19C/D。 |
| R12 | `b7f73d4`；`0d06b7b`、`e98c798`、`ed5aaef` | 432 实例、波浪/rebase、选中/归位坐标与稳定物理格 ID 由 R19B 审查；合并图集来源见 R19A，透明与后处理仍待 R19C/D。 |
| R13 | `0d06b7b`；`2ee2294`、`1c9b8c6` | 真实列/填充、列记忆、库替换与导航目标由 R19B 审查；R19 的总览插值消费者仍待 R19G。 |
| R14 | `1a082f2`；`2abb4f6`、`e98c798` | 品牌容器/控件/运输信息、Canvas Painter 的实际合成及成本；待 R19E。 |
| R15 | `2ee2294`；`2abb4f6` | 滑尺、数字字轮及 Pointer/InputRouter；待 R19E。 |
| R16 | `2abb4f6` | 全部标题字轮、整行 shaping、CSS/字体/布局和候选路径；待 R19F。 |
| R16B1 | `6636ddb` | 普通 glyph 节点复用、隐藏工作、页眉测量；待 R19F。 |
| R16B2 | `509afe8` | Canvas 隐藏绘制、按组成本与高 DPR 超时；待 R19F。 |
| R16B2V | `07514a5` | 混合字体 baseline、恢复场景尺寸、已撤回采样试验；待 R19F。 |
| R16B2V-P | 任务见 `07514a5`；未找到独立完成提交 | Windows 未收口像素任务已被后续契约替代；已实施试验/共享基线代码由 R19F 追踪，不恢复原图门禁。 |
| R16B2W | 任务见 `509afe8`；未找到独立完成提交 | 没有独立生产选型；已有共享 DPR/生命周期和工具改动归 R19F，D3D11 目标实机仍归 R34。 |
| R16B3 | 任务见 `2abb4f6`；未找到独立完成提交 | Windows OpenGL 未独立完成；共享 GL 适配及候选代码归 R19F/D，目标实机仍归 R34。 |
| R16M1 | `c7e9b61`；前置 `6740abc` | 真正离屏、DPR、隐藏依赖、字体物理面识别；待 R19F。 |
| R16M2 | `b9a5ea7` | `AssetCatalog` 的 AppleColorEmoji 物理字体身份为独立依据，资产选择保留；整行 advance/回退及实际质量待 R19F。 |
| R16M2P | `9470126` | macOS 半行距 floor 来自生产字面采集；不能仅凭旧分数保留，字体框/平台规则和源码语义待 R19F 重新证明。 |
| R16M3 | `2840110` | OpenGL 4.1、绑定重入、有界隐藏完成与离屏资源估算；待 R19F，图形共享分支联动 R19D。 |
| R17 | `33e8260`；`e98c798`、`ed5aaef` | ListView 模型/复用/顶部定位、详情展示与排版；待 R19G。 |
| R18 | `ed5aaef` | 详情抽取/旋转/归位、副本 clarity、封面 ready 身份和完整根窗口重建；待 R19G。 |
| R19 | `1c9b8c6` | 总览相机/居中/折叠/标签及两步进入；待 R19G，产品渲染质量联动 R19C/D。 |

非编号提交也有归属：`cedd2a4`/`835f0b4` 的归档与品牌身份由 R19A 核对；`5b2df80` 格式改动由各组件复查语义；`d03527e` 性能改动由 R19A 核对数据/封面，R19C/D 核对渲染；`21ef751`/`391decc` 的来源映射及证据边界由 R19A 核对。`8dcd4f6` 为 Canvas 计划，实际代码归 R19E/F；`6740abc`/`b08e94e` 为平台交接/动态规则，实际共享代码归上述 M 轮及 R19D/F；`e98c798` 的所有跨组件改动分别归 R19B/D/E/F/G。`279d9a7` 只添加用户观察约束，本文登记如下。R19H 最后对照完整历史核销，不能把此覆盖表当作尚待组件的通过证明。

## R19A 资产与数据判定

| 路径/符号、来源与有效调用 | 原理、适配与结论 | 验证/后续边界 |
| --- | --- | --- |
| `PrepareAssets::verifyFrozenInputs`；R03 `add/copy` 前 | 旧 PowerShell 在外层检查冻结输入，C++/POSIX 直接入口没有同等约束。现在读取冻结映射，同时比对 Git blob、归档 bytes 和 manifest 的 24 个输入；冲突/缺失在任何资源写入前失败。`--verify-inputs` 无生成写入；GUI 构造前统一非激活策略。 | 实际正常输入通过；临时隔离根篡改 demo README，普通生成入口失败且树内文件逐字节未改变。无需覆写入库资产验证保护。 |
| `music-case-v040.glb` → `GeometryAudit::compareGeometry` → qrc/生产 GLB | 三壳体 1000 三角形；保留属性/绕序、禁 LOD/全局缩放/优化。Qt mesh V 翻转为 glTF V 的补坐标适配，非截图调参。生产 RHI 从原 GLB 提取几何，导入 QML 材质不是有效产品材质。 | `assets` 实际验证 GLB/Qt mesh 全属性及三批次、纸面间隙与 QML 加载；材质另归 R19C。 |
| WOFF2 → SFNT → `initializeAssetFonts`/`assetFont` | DirectWrite 只解容器；原 WOFF2 和 OTF、OS/2/CFF 与 CSS 300/400/600/700 映射明确。CoreText 命名字重允许不同，按物理字形表选择；没有替换字体或按截图修改轮廓。 | 四字重 glyph、OS/2/CFF、整行 shaping 实测；AppleColorEmoji 身份适配保留，字轮 baseline/倍采样归 R19F。 |
| `music-app.ts::svg`/`music.css` → 功能 SVG | 八个生产路径、24 viewBox、1.5 描边及 round 端点，昼夜颜色从实际 CSS 提取；属于源码设计。原创品牌小稿的光学改绘是用户允许的品牌差异，不能外推给原模型或封面。 | 资源 hash、QtSvg 加载与品牌真实渲染/容器检查；原署名、许可、音频原 bytes 保留。 |
| `CoverCache::decode/paint` ← `cover-atlas.ts::paintCover/containCover` | worker 的 autoTransform 和有效 ICC→sRGB 是图像解释适配；256/1024 仅缩大图、contain、1/64 留边、透明 RGBA premultiplied 与逻辑 1024 缺图画布有源码依据。原首行 500、脚注默认 400；旧 C++ 脚注继承 Medium，现重设 Normal。没有锐化、按截图扩大封面或预放大小图。 | `cover-cache` 实测横竖方/alpha、ICC、小图 bytes、缺图/过大拒绝与取消；字体栅格不要求匹配浏览器图。 |
| `AtlasAllocator::publish/uploads` → `AtlasUploadQueue::expect/enqueue/take` | generation/identity/lease/serial 先验证再共享/复制；8/64 tile 预算来自冻结 `flushUploads`。小设备分页及生产合并图集是存储适配；ready 在相同命令提交后有效。 | CPU 队列、租约、迟到/重建回放/稳定零上传的算法测试实际运行；目标 GPU 命令与连续三维质量不能由这些测试推断。 |
| `SceneItem::updatePolish`/`SelectedCoverPool` → `RhiItem` | selected/returning 独立 lease、不可变像素与低清到高清升级；生产无需整页重绘。atlas/cover 无 mip、线性 clamp；opaque shader 在颜色转换前除 alpha，避免把 premultiplied 当 straight。 | shader 最终 alpha/LOD、透明边缘与 capture mip 不在本轮宣布质量完成，由 R19C/D 连续检查。 |
| `MusicTypes`/`LibraryModels` ← `music-types/order/search.ts`/`data.ts` | 本地 QUrl/媒体 Unknown、未知字段保留属于原生离线接口；QCollator 后端与 Web ICU 可不同，匿名视觉列顺序固定。规范化键与 ID tie-break、列索引、搜索索引不改变字段/顺序；搜索不改变播放。 | `library-data` 的 216 搜索、冻结列、JSON/Unicode URL、通知与竞态及 `library-qml` 实测。 |
| `OfflineMetadataProvider::request`/`LibrarySession` 的 queued 结果过滤 | 对请求捕获 const 快照，QObject 上下文事件投递，token/generation/kind 过滤；无网络对象/请求、无缓存写入。缓存旧状态原样返回，无缓存 Deferred。数据/Provider 不因外观复查改变。 | 取消、销毁、过期结果、库替换与导航/播放隔离实际测试；扫描/真正播放不提前实现。 |

## 输入与旧图像门禁归属

直接从冻结代码执行得到的 `export-search-golden`、`export-motion-golden`、`export-scene-golden`、`export-archive-pool-golden`、`export-real-columns-golden`、`export-detail-golden`、`export-overview-golden` 等继续作为算法/状态输入；执行前核对各自截取块及 mock 只提供测试输入，不能修改被测公式。旧 `scene-parameters`、`presentation-golden` 与 Room `room-environment` 含初始化后的采集状态，须先追生产 override；它们不能覆盖源码，更不能把 GPU 字节当资产算法的证明。

| 后续轮次 | 必须分离的现存入口 | 保留的有效检查 |
| --- | --- | --- |
| R19B | `SceneReview` 仅按自身渲染检查模型存在，不比较来源图；`ArchivePoolAudit` 填充/真实列已移除全部 SSIM/RGB 比较和替代矩阵图 | x/v/a、矩阵、投影、拾取、边界、两模式/改速/减少动态、稳定身份、实际 fallback 区域及资源 |
| R19C | `verify-r07.ps1`/`verify-r08.ps1`、`calibrate-materials.py`、`RhiShellAudit`/`RhiAudit` 的原图/ROI/环境 witness | 源公式、有效 override、真实格式/通道/透明合成、双主题质量和资源 |
| R19D | `postprocess-audit`/`occlusion-audit`、`RenderBackendAudit`、`RhiPostprocessAudit`、`prepare-*-capture.py` 与 render/RHI witness 耦合 | 真实附件/采样/resolve、shader 执行、实际质量、上传、生命周期与局部成本 |
| R19E | `compare-browse-ui.ps1`、`verify-r14.ps1`/`verify-r15.ps1`、`UiAudit`/`RulerAudit` 的旧图门槛 | 控件几何/状态/交互、Canvas 隐藏工作、数字轨迹及合成质量 |
| R19F | `verify-title.mjs`/`TitleAudit`、`verify-r16.ps1`/`audit-title.mjs` 的旧 SSIM/RGB/边缘带及逐 case 采样补偿 | 字体物理身份、整行 shaping/布局、时序/宽度、真实清晰度、隐藏绘制/生命周期与成本 |
| R19G | presentation/detail/overview Review 及导出器中的原图/ROI 残留 | 展示布局/虚拟化、场景轨迹、相机/投影/状态、资源及实际质量 |

本轮没有运行这些旧图比较。`legacy-image-gate` 继续排除并 disabled，防止默认或裸 CTest 误执行；各所属轮完成拆耦后恢复必要检查，不删除真实失败来换标签。品牌自有 SVG 衍生容器一致性、图像算法的合成输入单元测试不属于原机器图像追分。

## 用户观察问题：明确保持开放

**VIS-ARRAY-BLUR-01。** 已知候选小入口 `ArchivePoolAudit`/`RealColumnAudit` 的真实窗口初始 960×640；产品 `DetailReview`/`OverviewReview` 初始 1280×720，并执行九尺寸。R19 已有本地 `overview/r19/final3-metal-1/run-1/report.json` 的实际 DPR=1，首个产品样本为逻辑 1280×720、目标 853×479、187 实体、selected 1024²、4×深度附件、Bokeh 开启；它证明该次真实提交的尺度，不能证明就是用户观察的那个窗口。`hidden-final-metal-1` 为 QML 总览标签/布局的 RenderControl，不是完整三维阵列，也不能把其大图清晰度当作产品阵列证据。相关三维记录/入口还包括 `verification/macos/visible-window/`、`verify-detail.py` 和 `verify-overview.py`。

尚缺用户观察对应的唯一入口/时刻、两窗口是否同一相机/画质/运动状态、真实 DPR/屏幕及输出缩放配对；旧记录没有自动补齐这些条件。资产侧 256 atlas/1024 selected、contain/ICC/透明留白路径已有依据，未发现按窗口尺寸重解码或放大模糊的资产补偿；这不排除最终采样问题。R19B 核对投影，R19C 核对磨砂/透射/LOD，R19D 按相同匿名库/相机/状态控制小大逻辑尺寸、实际 DPR/render scale，读取真实附件和合成缩放并主责修复额外模糊，R19G 核对总览取景。禁止以锐化、关闭 Bokeh 或未经证明提高全局分辨率收口。

**VIS-ARRAY-FLICKER-01。** 用户已确认大量专辑、透明重叠尤明显；唯一入口/后端/触发序列未确认。旧静态图与 `passed=true` 只覆盖各自断言，不能反证连续闪烁不存在。R19A 确认资产请求/租约/迟到淘汰没有无依据图像补偿，但没有据此关闭该问题。R19C 主责稳定透明排序/光学捕获/前后表面合成；R19B 核对几何/深度键，R19D 核对深度/resolve/读写反馈/跨帧资源，R19G 核对总览转换。必须连续检查静止、活动波浪、浏览与重叠/总览转换，定位首个异常 pass 并真实验证修复；不得只靠零 warnings、减少数量或冻结动效。

两项根因均未在 R19A 确认，也未移交 R34 替代本机调查。R19C/D 完成具体修复，R19H 在 R20 前核销；只有独立目标平台缺环境的回归可转 R34。

## PERF-01–07 与验证边界

| 项 | 实际链路和所有者、最坏范围与判定 |
| --- | --- |
| PERF-01 | `CoverCache::request/lookup` 的 key 含 identity/path/resolution；96 pending、3 active、16MiB LRU。`LibraryModels` 排序键预计算，比较器无 I/O，搜索角色通过快照索引查找。来源审计离线逐文件，不进入帧循环。 |
| PERF-02 | `decodePool::tryStart` 不向全局池排无界任务，完成结果仍计 active；GUI 只消费结果，worker 解码/ICC/paint，销毁不等待 worker。新增 Git/qmake 校验只在资产开发工具同步运行，不在产品启动或 render。 |
| PERF-03 | const `CoverPixels` 交接，重复/过期先拒绝，allocator 只给 dirty tile；trusted owned 不再深复制，外部输入隔离一次。脚注字重变化只影响缺图生成一次，不新增每帧重画。 |
| PERF-04 | Atlas/selected 属 render，源尺寸/内容更新不重建不相关附件/PSO；8/64 tile、selected 4MiB 单列。最终 capture/mips/透明通道图交 R19C/D，未据 CPU 审计冒充 GPU 通过。 |
| PERF-05 | 模型静态几何共享，字体先按物理面验证；256/1024 分工与无 atlas mip 是现有生产路径，不以优化名义改精度。最终缩小过滤/抗锯齿由 R19D 复查。 |
| PERF-06 | 模型/Provider 只发布实际通知，搜索/导航与播放独立；本轮没有新增 QML tick/委托或测量缓存。隐藏渲染仍走已有 OffscreenSurface，不创建可见产品回放。 |
| PERF-07 | generation/cancel/reset/close/ready 中销毁、租约 fork/release、30 次取消/十次在途关闭由现有测试覆盖；原生隐藏窗口策略实际检查。Windows DirectWrite 分支、Linux/Windows Kit/插件逻辑严格静态核对；目标 codecs/GPU/呈现仍按 R34 原任务，不能由本机通过外推。 |

本轮本机为 macOS/arm64、自动发现最高 Qt 6.12.0、Apple Clang 17，Release。重建入口：

```sh
node tools/review/check-web-baseline.mjs
cmake --preset qt-release -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
# 从本机 cache 读取 YALAHOO_SELECTED_QT，传给工具，不将路径固定到脚本。
build/release/tools/prepare-assets "$PWD" "$selected_kit" --verify-inputs
ctest --preset qt-release-full --output-on-failure
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
git diff --check
```

基础来源检查与正/负资产输入验证通过，Release 构建通过。首次受限 CTest 为 22/28，通过项保留原范围，6 项原生屏幕依赖以 `no screens available` 中止；原日志保存到 `verification/macos/source-review/r19a/sandbox-ctest.log`。使用原生服务权限、同一隐藏 preset 重跑；不得通过切换软件平台插件、降低断言或显示窗口回避权限问题。同一二进制原生权限重跑 28/28 全部通过，三个隐藏渲染项实际 Metal 执行；两组官方 qmllint 与格式检查通过。来源检查输出中的 `reviewedImplementation` 将上表符号绑定到当前文件 SHA-256，机器结果只存本地；本机未进行前台、正常 VSYNC、三维连续质量或目标平台最终验收。

## R19B 运动、相机与阵列复查

复查起点为 `8e6c5a3`。逐项恢复 `ec0af92`、`5102cb0`、`b7f73d4`、`0d06b7b` 前一版本计划及实现 diff，并追踪 `5b2df80`、`75775b3`、`2ee2294`、`e98c798`、`ed5aaef`、`1c9b8c6` 的相关修改。冻结源码由来源映射读取；`check-web-baseline.mjs` 现在逐个核对四份数学输入的源 blob hash，并为下表实际实现输出 SHA-256，机器矩阵只保存到本地。

| 原轮次 / 生产源及原生符号 | 依据、判定与适配边界 | 本机验证与后续边界 |
| --- | --- | --- |
| R05：`motion.ts::damp`、`MusicArchiveMotion`、`MusicRealColumnScalar` → `ScalarSpring`、`ArchiveMotion`、`ColumnScalar` | 保持解析临界阻尼、五次 x/v/a、1.4–2.4 秒时长、observe/retarget/translate/finish 顺序；C++ 保存精确加速度。同目标 archive 保护是计划明确的固定差异，不由截图决定。R18 `cancelTravel` 只在详情内部从原显示导数交给 rate=9，不重启 browse 轨迹。 | 两组各 694 源码样本、13 组函数、反向/速度/长负帧/rebase/完成 revision 测试；R18 独立 detail 样本回归，详情消费者复查仍归 R19G。 |
| R05：`scene.ts` 帧循环 / `music-boot-timing.ts` → `FrameClock`、`App.qml::FrameAnimation` | 生产只有一个帧源，dt 钳制后只乘一次速度；开场时间独立，恢复丢弃首次间隔，减少动态执行完成。没有因截图调参引入的额外时钟或媒体依赖。 | motion/共享状态实际通过；原生窗口曝光、正常 VSYNC、物理输入仍为 R34 的窗口/性能边界，隐藏回放不代替它们。 |
| R06：`viewport-layout.ts::viewportLayout/archiveFraming`、`scene.ts` 音乐相机目标块、`MusicCameraMotion` → `LayoutMetrics`、`SceneAdapter::target/advance` | 1.05 严格分界、1100/coarse 短屏、音乐 `.25` detailX、560/1080 detailY、preview `.36`、59°/25°→8°/20°、140→72、7.33→5.9 及固定检查槽均是有效生产值。轨道在 navigation 与 detail 插值各衰减一次，是原调用链语义。QMatrix4x4 float 是 Qt 交付适配；canonical GL z 仅用于投影/拾取，GPU 使用 QRhi clip 修正。无额外取景补偿。 | 原 48 目标、694 相机帧、528 投影点；新增直接执行同一冻结目标块的 36 个非零导航输入，覆盖正反向、0/.35/.7/1 detail、真实列、改速后 dt、减少动态、1.05 两侧和 coarse 短屏。 |
| R06：`music-model.ts` / `scene.ts::model.rotation` → `SelectionController::transform` / GLB | 模型高度 3.35、底边 .175、厚度 .14 与导入资产一致；音乐 inspection lift=3.47，4.05 只为通用 primitive 压力测试。XYZ 旋转用 double 展开再转 float，独立低 lift/同时 pitch+yaw 输入证明必要顺序，不回退为误差更大的 quaternion 乘法。geometryOnly 目标保护保持原请求精度并显式报告 limited，不是截图缩放。 | 几何/资产/ownedPose、投影与拾取实际通过。最终抗锯齿、目标尺寸和合成仍归 R19D。 |
| R12：`archive-loop.ts` / `scene.ts::field` → `ArchivePool`、`SelectionController::field/rebase/publish` | 9×48 顺序、半周期 floor、5.2/.62 间距、最多六个 3.2 秒脉冲、.24 clamp、2.5 秒 idle 与波浪原点均来自源码。按 selected 相对原点求位移、静止八 ulp 算术残差界和跨 rebase 的有限物理格 ID 是精度/身份适配；不改导数/几何/透明参数。八 ulp 仅覆盖平移、乘法和相消的有界舍入，不是可见像素偏移。 | 冻结初态/复合旋转、首尾/反向、空库/大库、LCM、运动导数、播放隔离实际通过。隐藏 Metal 的 90,000 专辑保持 432 实例、初态矩阵与 rebase 矩阵误差均为 0，cover 区域/物理 ID 不变、稳态上传为 0。 |
| R13：`MusicRealColumns`、`scene.ts::musicColumnRowOffset/musicRealColumnField`、`music-app.ts` 列记忆 → `RealColumns`、`ColumnMemory`、`SelectionController` | 保留真实行 12…11+count、独立中点/列活动度、五次归中及折返导数、目标实际显示深度、记忆开关无播放副作用。按时间惰性取样是解析算法调度适配，避免每帧遍历全部列；减少动态 epoch 同步未观察列。R19 overview 插值入口留待 R19G，不用本轮普通浏览通过替代其复查。 | 完整冻结逐帧 x/v/a 最大误差约 1.12e-10；0/1/2/3/49/106 列、实际深度折返、模式/记忆切换、库替换、损坏偏好和匿名播放隔离实际通过。隐藏 Metal 的有限列矩阵误差为 0、无重复 ID、首尾键/滚轮事件消费与 cover 重放通过。 |

`SceneAdapter::publish` 的复用键补入 `hasArchivePool`，显式包含快照是否拥有阵列的依赖；新增 attach/detach/destroy 回归验证旧实例清空。旧实现依赖 casePosition 的浮点差异也能通过该回归，本轮没有复现可见陈旧阵列，不能把这项防御性修正宣称为已证实的用户渲染根因。

`ArchivePoolAudit` 现在在 macOS 复用 `OffscreenSurface`，运行真实生产 SceneItem/RHI；`YALAHOO_VERIFY_VISIBLE=1` 才选原生窗口，默认不开。准备由 frameSubmitted/正常 render 请求驱动，上传排空最多 70 帧、单次仅一个采集请求，不用 50ms 定时轮询推进场景。capture 只保存自身结果；矩阵 oracle 不再进行替代渲染或图像比较。重建检查真实 sceneGraphInvalidated、相同逻辑实体和已排空上传；高清未就绪检查 renderer 实际使用的 256² fallback 区域及 lease，而非以另一张图的分数间接推断。有效区域诊断只在显式 capture 分支构造，不进入生产逐帧遥测。生成完整源码输入改按实际 host 的 `verification/{macos,windows,linux}` 保存，可通过 `YALAHOO_POOL_GOLDEN_ROOT` 指定同源输入根。

PERF-01–07：GUI 的 `FrameClock → MotionController/SelectionController → SceneAdapter` 有序发布不可变值；帧工作限 432+4、六脉冲、九列窗口，ID/列记忆索引在快照边界更新；RealColumns 仅推进统一时间/采样可见状态，解析等价性由完整逐帧源 oracle 验证。投影矩阵按 frame revision 缓存；实体/灯光深度键先计算再排序，rebase 不改物理 ID 的 tie-break；材质排序/通道正确性仍由 R19C 负责。运动不解码、读盘、阻塞 worker 或重建 GPU 资源，采集/PNG 和有效 cover 区域 JSON 只在 Review。atlas 与高图容量、租约/取消、隐藏失效/重建及销毁继续沿用 R19A 的有界资源链；本轮未增加产品纹理/附件或 shader 工作。

本机 macOS/arm64、自动发现 Qt 6.12.0、Apple Clang 17、Release；两组官方 qmllint 与 check-format 通过。来源重新执行有 6 个 double 的跨宿主末位变化，最大 3.56e-15，源 hash、结构和其余数值一致，保留原 canonical fixture，不据此改算法或门槛。scene 最大目标误差约 1.43e-14、相机误差约 5.23e-14、投影误差约 .000913 逻辑 px；36 项导航最大目标误差约 7.11e-15。新增 36 个目标为执行冻结源码得到的真正测试输入，非运行报告。

首次完整隐藏回归 28/30 通过：finite lifecycle 新断言误把新 renderer 从 1 开始的局部 resourceGeneration 当作全局身份，已改查真实失效事件；另一次 filled 在 far-night 遇到 windowPolicy 失败，原日志没有具体 native 字段，原因未确认，不归因于 GPU 或产品主动抢焦点。入口现在保存具体失败策略到本地，修正后的两个隐藏测试 2/2 通过，native 记录 visible=false、active=false、foregroundUnchanged=true。有限列另补真实昼夜过渡及重建复核通过；填充另以 `YALAHOO_VERIFY_LIFECYCLE=1` 完成十次串行隐藏 scene graph 重建、cover 重放及 fallback 检查，窗口策略持续通过。其余 28 项复用原有效结果；最后恢复 Review OFF、可见测试 OFF 的普通 Release 构建与 25/25 快速 CTest（含 source-format/默认隐藏窗口策略）通过。正常窗口/VSYNC/物理输入未执行。受限环境初次独立工具因 no screens available 中止，原生服务权限下同一隐藏入口真实执行；没有改用软件后端或显示窗口。

复现（最新 Kit / Release；Windows/Linux 无可靠离屏入口时按窗口契约运行，不能把其本机缺环境写成通过）：

```sh
node tools/review/check-web-baseline.mjs
node tools/review/export-motion-golden.mjs
node tools/review/export-scene-golden.mjs
node tools/review/export-archive-pool-golden.mjs
node tools/review/export-real-columns-golden.mjs
cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release-render -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure
# 用当前系统目录的 full-trajectories.json 设置 YALAHOO_REAL_COLUMN_GOLDEN 后运行 RealColumnsTest。
# 十次隐藏重建：YALAHOO_VERIFY_LIFECYCLE=1 build/release/tools/archive-pool-audit 新输出目录
ctest --preset qt-release-full --output-on-failure
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
cmake --preset qt-release -DYALAHOO_REVIEW=OFF -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release --output-on-failure
```

本轮自己的输出仍可见背景/阵列模糊；相机和几何数值通过不关闭 BLUR-01，也不证明半透明重叠无连续闪烁。静止几何深度/ID 的 rebase 没有跳变；FLICKER-01 的最早异常透明 pass、材质/捕获顺序和跨帧资源仍由 R19C/D 调查。R19D 继续控制同一库/状态的小大逻辑尺寸、真实 DPR 与实际 target scale，R19G 继续核对 overview；R19H 在 R20 前核销。独立目标平台回归见 R34 的 R19B-P。

## R19C1 材质与光场参数复查

复查起点为`ae60ff8`。先按规模契约将R19C拆为参数边界R19C1和紧随的光学合成/连续质量R19C2，未将用户闪烁问题移出复查阶段。恢复`1556a55`、`178ca45`、`52f1278`之前的原计划及实现diff，并追踪`5625d73`、`5b2df80`、`d03527e`、`e98c798`、`ed5aaef`的有效参数修改。`e98c798`未改动ShellMaterial/SelectionLighting/GlassMaterial参数，附件等修改仍归R19D。下表只覆盖参数边界，不能视作R10B或环境shader已全部复审。

| 原轮次 / 源生产路径 → 当前符号 | 判定与历史依据 | 验证及边界 |
| --- | --- | --- |
| R07：`scene.ts::configureMusicGlass → CardAppearance.register/setTheme → setMusicGlassTheme/setMusicGlassClarity` → `GlassMaterial.qml`、`ShellMaterial` | 三表面transport、coat、线性tint/attenuation及IOR=1.46均有冻结源码。R07在`1556a55`通过ROI坐标搜索引入environment/roughness=1.01、thickness=.91809、distance=1.0201；没有灯强单位或BRDF接口依据，现全部移除。主盖clarity独立，侧脊/后托保持原finish。 | 新导出器直接执行冻结完整模块及实际CardAppearance主题调用，45组连续主题/clarity输入核对真正QML和C++，保留原210组端点检查。没有浏览器/图片输入。 |
| R08：`scene.ts::setTheme`与`MusicSelectionLighting.setTheme/update` → `LightingPalette::theme`、`SelectionLighting`、`RowLightController` | 音乐有效环境.25/.16覆盖隔离R07的.48/.68；Three在material.envMap为空时以scene.environmentIntensity更新uniform，不额外乘configure的.65。key .45/.30、hemi .32/.21、fill .045、spot 180/130×64、area 34/24与曝光1.05/1.08保留；GLSL沿用源单位，没有Qt内置brightness换算。650ms曲线、double follow/同一FrameClock与相机局部offset保持。 | 924个运动/主题样本与392个场核输入实际通过；误差见下文。显式给核测试传mode/fill，但源sampleRowLighting不依赖两者，不能据此声称shader四模式合成已验收，仍归R19C2。 |
| R10A：`createAlbumPrintMaterial`及`MusicSelectionLighting.shadePrint` → `opaque.frag/paper-lighting.glsl`的参数接口 | 纸面alphaTest=.025、Lambert无specular/emission、先限制source再乘fieldExposure、最终toneMapped=false及线性入射接口有源码依据；qrc/GLB/UV/原生clip适配保留。当前参数变更不改纸面shader、颜色转换、附件或采样。 | 本机真实Metal阵列及封面/图集/资源回归通过，只证明当前输入范围；完整纸面shader/环境算法复查归R19C2，阴影/后处理归R19D。 |
| R10B参数迁移：`5625d73::Renderer::uniformTemplate`、R18 `ed5aaef::Renderer::uniform` | 两处把R07 ROI系数再次施加到实际uniform：基础transport/coat及逐实体lid roughness。现在transport/coat直接复制；lid按实体clarity计算后直接提交，其余缓存和材质不变。原R10B manifest及旧审计脚本的fixedOptics只描述历史实现，不能覆盖当前源码。 | 采集从本帧真正传给updateDynamicBuffer的uniformScratch按draw.offset读取；ArchivePoolAudit逐可见壳体检查tint/attenuation/transport/coat。稳定498、详情/归位240、有限列夜间123组实际uniform通过。 |

`calibrate-materials.py`执行即失败且无写入，MaterialReview在show/采集前拒绝calibration或任何factors字段；同时删除Review将expected乘拟合系数的自洽检查。真实负向运行两种输入均退出12。旧verify-r07/r08、RhiShellAudit/RhiAudit与MaterialReview完整ROI/原图拆耦留给R19C2，不运行旧分数。没有将历史ROI失败改写为通过。

PERF-01–07和COMMON-01–05逐项矩阵由`python3 tools/audit/audit-material-parameters.py`绑定实际符号及SHA-256，在`verification/{windows,macos,linux}-static/material-parameters/`生成。人工展开了ShellMaterial→uniformTemplate→逐实体lid→有界buffer上传、QML→SelectionLighting属性、平台CMake/QRhi分支与qrc资源路径；矩阵中的符号guard只检测源码漂移，不代替上述推演或实测。

- GUI只发布已有不可变Input/SceneFrame，render每帧七份模板和最多432+4实体，不遍历曲库；没有新增计时器、worker、I/O、纹理复制/上传、资源或PSO重建。移除系数不改变缓存键与失效条件。
- 仅显式capture构造最多3×436份值tuple，并从已提交CPU缓冲区复制小型uniform；不读取GPU、不新增等待。现有最多两个在途capture及正常事件驱动保持；采集回调不持有临时buffer指针。生产request=0没有这些JSON工作。
- Windows的D3D11枚举/API仍在Q_OS_WIN内；三平台共用无OS分支的标量/颜色/材质，816-byte std140与绑定保持。Kit/C++23/qrc检查实际通过，six-shader公共wrapper输出SPIR-V/GLSL330/ES300/HLSL50/MSL12，只是静态翻译。`bake-material-shaders.py`现从所选Kit按主机选qsb程序名、报告写实际平台，不将macOS结果写进Windows目录。Windows/Linux编译器、原生GPU及窗口管理器本机不可用；对应实机保持R34的R19C1-P。

本机为macOS/arm64、Apple M4、自动发现Qt6.12.0、AppleClang 21.0.0.21000334（本轮toolchain.json）、Release。参数/光场测试通过：轨迹误差约1.14e-12、palette约2.73e-8、GPU uniform位置转换约1.53e-5、场核约1.12e-16。首次受限完整回归22/30，其余8项在原生服务初始化出现`no screens available`/中止；负向产品入口也在受限服务环境中止。原日志分别保存，未改用软件平台或显示窗口。原生服务权限下，同二进制、相同隐藏preset的30/30完整回归通过，负向产品入口退出12；两组官方qmllint、check-format/source-format通过。最后恢复Review OFF、可见测试OFF的普通Release构建和25/25快速回归通过，默认隐藏原生窗口策略保持。

隐藏实际质量检查读取本机自身渲染的stable/detail/returning-night/finite-night；没有原图比较或评分。参数修正后无空白目标、主题丢失或材质整体白化，但低分辨率阵列仍可见模糊/轮廓阶梯，本轮不宣称其正确或消失。该入口逻辑960×640、内部真实目标480×320、4×捕获采样，RenderControl输出RGBA8单采样；明确区分目标与窗口大小，没有据图增分辨率、锐化或关闭Bokeh。BLUR-01仍由R19D控制小大尺寸/DPR/缩放定位；FLICKER-01的连续重叠与最早异常pass仍由R19C2调查。本轮的静态图/功能通过不能反证闪烁。窗口visible/active均false，前台未变，未知driver分配仍为null；本轮没有可见窗口、正常VSYNC、物理输入或性能采样。

复现：

```sh
node tools/review/export-material-parameters.mjs
node tools/review/check-web-baseline.mjs
python3 tools/audit/audit-material-parameters.py
python3 tools/audit/bake-material-shaders.py
python3 tools/assets/expand-lighting-shaders.py --check
cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release-full --output-on-failure
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
cmake --preset qt-release -DYALAHOO_REVIEW=OFF -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release --output-on-failure
```

已有参数/资源重建执行一次，相关十次隐藏资源压力复用R19B同一所有权实现，不把本轮一次重建称为十次。所有新输出保持忽略；入库的45组参数为直接执行冻结代码得到的测试输入，不是运行报告。

## R19C2A 材质/灯光 Review 与原图门禁拆耦

起点`48a9495`，依规模契约在实施前将R19C2拆为A（本入口）、B（RHI光学与捕获）和C（连续透明闪烁）；仍全部位于R20前。恢复`1556a55`、`178ca45`的原计划/实现，沿`3fc6a39`、`522a3f3`、`d06e5f3`、`5b2df80`、`5625d73`和R19C1追到当前MaterialReview调用。未修改生产shader、材质参数、PMREM/DFG/LTC资产或生产RHI；本节不能视作R07/R08/R10B剩余光学算法已复查。

| 原轮次 / 实际审查对象 | 源码依据与结论 | 验证边界 |
| --- | --- | --- |
| R07：MaterialReview、verify-r07的90状态、9纸面探针、标定与ROI门禁 | 投影/三个表面参数有源定义；R07用浏览器像素作纸面期望并计算ROI/SSIM，无独立数学必要性。删除图像读取/分数/diff及标定调度；源公式改用均匀测试墨色推导线性入射/限制/编码，3码值容差保留。 | 99状态实际通过，纸面误差0；45组连续源参数/210端点测试继续独立保留。玻璃BRDF/环境/LOD完整复查归B。 |
| R08：MaterialReview的48灯光状态、verify-r08的场核与图像混合条件 | 保留实际光场模式、detail、主题、相机与源表面参数；删除五ROI的6/.97及固定D3D11 API编号假设。924轨迹/392场核继续由LightingTest验证，脚本只等待实际CTest/进程退出。 | 48状态、4种模式、前/低视角/两主题实际输出通过；不以自己的单盒状态证明生产透明阵列无闪烁。 |
| R09：MaterialReview的AO/阴影诊断调用与目标尺寸断言 | 共用驱动不再加载原图；保留channel、shadows、near/far、aoRadius与实际effectiveTextureSize检查。原occlusion GPU evaluator仍标旧门禁disabled。 | 此处仅审查驱动接口，完整AO/shadow shader、旧报告消费者和目标后端归R19D。 |
| R09B：MaterialReview的Bokeh/SMAA/AA与focus/aperture断言 | 保留有效质量设置、目标大小、focus/aperture参数检查；移除共享ROI/full-image比较。未改任何核/曝光/采样。 | 仅共用驱动拆耦；后处理专用工具尚未恢复，算法/连续质量归R19D。 |
| R10：MaterialReview的instance/atlas probe路径与颜色输出 | Probe的实例、深度排序、atlas phase及字节元数据保留；报告不再输出edgeDistance/ROI。原render-backend GPU evaluator不自动启用。 | 未用此路径重新选型或宣称RHI等价，正式拆耦/算法结论归R19C2B/R19D。 |
| R10B：VerificationWindow接入、MaterialReview首次show与引擎所有权 | 新CLI在ApplicationWindow加载前分派；独立GeometryScene在macOS使用真正隐藏RenderControl，普通产品窗口及生产RHI不变。 | 默认原生visible/active均false；相同View3D失效后背景、完整root重建有效的边界详见计划经验记录。Windows/Linux实机归R34/R19C2A-P。 |
| R19C1：拒绝factors/calibration与参数核对 | 两类输入仍在创建window/采集之前退出12；不把expected乘同一拟合系数，不放宽源参数容差。 | 负向CLI与无图像依赖的真实输出回归；R19C1实际uniform检查保持独立。 |

PERF-01–07/COMMON-01–05与各平台符号/hash矩阵由`tools/audit/audit-material-review.py`在忽略目录生成。GUI有限状态只应用一次，真实帧事件串行推进四个准备帧；显式采集一次自己的texture并做一次线性前景存在检查，不执行Gaussian ROI/边缘搜索/两图比较。同步readback/等待和PNG只在Review，不引入产品逐帧工作、worker或上传队列；Offscreen附件按实际物理大小复用，估算与driver未知值分开。root/engine/窗口/控制器依序释放，render线程invalidation用atomic交接。十次完整root重建不声称同对象恢复，原生窗口恢复按R34保留。

本机macOS/arm64、Apple M4、自动发现Qt6.12.0/AppleClang 21、Release、真实Metal/DPR1/960×720。两个入口的147状态、十次材质和一次灯光完整root重建通过；源投影最大误差约0.000226逻辑px、CPU/Quick3D最大0.000244逻辑px，QML warnings为空。实际读取自己生成的昼/夜和前/低视角图检查有效内容，未执行原图比较；仍能看到壳体朦胧、侧缘阶梯，这不关闭光学/抗锯齿或用户BLUR-01/FLICKER-01。最初受限服务中止保留日志，原生服务权限下执行同一隐藏入口；没有改为软件后端或show。一次初期灯光仅foregroundUnchanged失败原因未确认，后续新生命周期实现的两个入口默认策略通过；失败未改写。

复现：按`materials.md`选择两个隐藏CTest；`check-web-baseline.mjs`、两组官方qmllint、check-format及`audit-material-review.py`保留来源/构建/静态核对。PowerShell薄入口只调用共同Python驱动，目标平台尚未实跑，不声称其格式/解析或D3D11/OpenGL通过。生成图、逐状态JSON、日志与机器矩阵均保持忽略；均匀paper-probe.png为必要测试输入。

Release完整隐藏回归32/32通过，两组官方qmllint与check-format/source-format通过；额外实际DPR2/1920×1440自身目标的99状态和十次完整root重建通过，纸面误差仍为0。calibration、逐case factors及缺失参数三类实际CLI均在window创建前退出12且未生成报告；报告校验器拒绝陈旧fixture hash、旧imageComparison和缺失重建记录。DPR2仅证明该Review目标，不外推生产渲染缩放或可见显示质量。

审计还发现共用驱动无条件创建R10的2048²/16MiB探针atlas，即使材质/灯光不使用它。现只在backendReview路径构造，普通两入口报告probe payload/submission为0；这是删除无效准备工作，不改实际封面/环境/透射捕获，也不声称产品显存或FPS改进。

移除无关atlas后，材质99状态/十次root重建和灯光48状态/一次root重建均补验通过，probe payload/submission均为0；一体Python入口实际完整执行成功，恢复Review OFF、可见测试OFF，生产Release构建与25/25快速CTest（含source-format及默认隐藏窗口策略）通过，生产程序拒绝Review CLI返回1。未重跑不受该无用分配移除影响的整套32项，复用上面的完整隐藏结果。首次一体入口在收尾编辑后正确拒绝未格式化的MaterialReview，格式化后的最终流程通过；不将首次失败改写为通过。

## R19C2B1 不透明RHI源数学、资源与门禁复查

起点`75b5543`。实施前将B分为B1不透明通道和B2光学壳体，紧随的C仍负责连续闪烁。逐项恢复`52f1278`的R10A原任务，沿`5625d73`、`1f2352a`、`d03527e`、`75775b3`、`f7e423a`、`b7f73d4`、`e98c798`、`ed5aaef`及R19B/C1追至当前同步、uniform、绘制、读回路径。仅复审下列不透明/采集边界，不冒充R10B光学算法或R19D后处理全量复审。

| 原轮次 / 实际审查对象 | 源码依据、适配及判定 | 验证与边界 |
| --- | --- | --- |
| R07：`createAlbumPrintMaterial`→`opaque.frag` | 源Lambert无specular/emission，alphaTest .025、toneMapped=false、先限制原图颜色。sRGBDecode、预乘滤波后恢复颜色、contain/inset与源3.06纸面几何保持；没有ROI系数。 | 16个黑/灰/彩/白×关闭灯光/0/.25/2入射探针独立推导线性结果，最大约.001792≤1/255；三种真实封面另有36状态，不以均匀墨色证明细节质量。 |
| R08：`shadePrint`、锁定Three Lambert/hemisphere/IBL→`paper-lighting.glsl`与CPU原点场 | 对照生产shadePrint替换顺序、Three `lights_lambert_pars_fragment`、hemi及getIBLIrradiance：direct/hemi除PI，IBL自身PI与Lambert BRDF相消；print field最后乘原曝光。entityLight0.xy提升只依赖origin/frame，C1源场核和轨迹独立回归保留。 | 全共享helper逐token核对uniform映射/原点提升，未拟合环境字节或图像。原PS生成器遗漏后续提升、会恢复逐片元重复计算，改为Python共同生成器；实际生成+格式化与提交shader逐字一致。 |
| R10A：`RhiAudit`的Quick3D/浏览器oracle、平面/法向、source shadow witness | 移除Quick3D实例化、原图载入/MAE及source shadow image比较；保留数学源位置/法向/深度、receiver已知strength。旧QML只留历史源码，不再链接或打包；旧verify-r10a历史流水线退役，当前独立CTest恢复。 | 36双主题/前低视角/三封面/三通道；深度约1.59e-8<2e-6，normal约.001961≤1/255，四角约1.822≤2px，六shadow最低.678431。近距fixture不触发源雾，不声称远景完整。 |
| R10B：RhiItem异步capture的重复纹理与值交接 | 显式capture原13字段经fallback常多次指向同一texture，却重复GPU readback和imageOf转换。现在按真实纹理指针去重、每唯一texture转换一次，字段共享不可变QImage；最后一组真实回调才deliver。没有改变pass/格式/采样。 | 无shell/post且captureAtlas=false时12字段=4真实readback；constBits验证别名共享；仍最多2批pending、按backend读回上限排空、QPointer排队、过期request丢弃。透明捕获与排序仍归B2。 |
| R10C：RGBA16F/法线与RGB24深度RGBA8、资源清单 | 浮点线性颜色与独立normal/depth、hardware depth保持实际Kit支持；读回解码有限值、Y方向/字节序与原语义一致。capture附加实际对象inventory，未知driver内存仍null，不用描述估算冒充显存测量。 | 每状态核对真实格式、1×、无mips及目标尺寸，full回归同时覆盖带shell/atlas的采集别名路径；完整AO/Bokeh/SMAA仍R19D。 |
| R10D：生产SceneItem/RhiItem共享边界 | 只在显式capture分支增加inventory/去重诊断；request=0生产不执行这些JSON或readback。新的RhiCaptureHarness是工具，不改变生产帧源、选型或输入状态。 | 同一次完整隐藏回归含材质、阵列、真实列、封面和界面；不宣称产品threaded/VSYNC或FPS提升。 |
| R11：cover snapshot/lease及same-request revision | 保留按cover cacheKey/content revision共享/隔离一次，不把更新绑定capture request。独立driver以同request替换纯红/蓝，检查自己最终输出各自响应；不是两幅源图趋同。 | sameRequestCoverReplacement通过；完整cover-cache/atlas queue/真实阵列GPU回归通过，个人音乐没有读取。 |
| R12：阵列/后续优化中的批次与读回所有权 | 采集只访问有界实体和最多13个字段，实际读回组按指针有界合并；无全库遍历或无界队列。resize旧资源的最后使用由QRhi完成约束，结果只持有值。 | 12有序尺寸/九种不同尺寸、30个真实帧驱动resize、过期回调及最终尺寸通过；逐帧只发一次状态，无processEvents手动pump。 |
| R13：有限列共享renderer采集 | 去重不改变实例ID/矩阵/封面区域、同深度键或真实列状态；generic字段通过Qt隐式共享保留相同语义图像。 | real-columns-gpu随完整33项通过；本项不新增有限列运动算法结论。 |
| R18：详情/归位及renderer重建 | 不改owned材质或快照；同RhiItem跨十次实际invalidation重建，当前renderer的局部generation仅作描述，实际SG事件作为生命周期证据。 | 十次隐藏同item重建及有效depth通过；原生窗口hide/restore和正常呈现仍R34。 |
| R16 macOS可见补验：`e98c798`→Target MSAA fallback/有界drain | 实际GL能力探测选择同格式renderbuffer代替不可用multisample texture，不减样本数或颜色精度；readback按MaxAsyncReadbackFrames+1继续真实帧。保留必要API适配，不因旧图分数回退。 | 本轮Metal隐藏同item/尺寸/有界读回实际通过；GL/MSAA跨后端及可见drawable完整验收仍R19D/R34，未宣称本轮执行。 |

COMMON-01–05/PERF-01–07与平台逐项符号/hash由`audit-rhi-opaque.py`写忽略目录，人工展开了源Lambert/场核、GLB/normal/clip转换、实际附件、唯一读回的命令/回调边和销毁顺序；字符串guard不替代推演/运行。当前macOS/arm64/Apple M4、最新自动Kit Qt6.12.0、AppleClang 21、Release、Metal/DPR1隐藏：独立入口9.57秒为一次功能运行耗时，不能作为性能比较；完整隐藏33/33、两组官方qmllint/check-format及paper/shared helper检查通过。没有像素配对、旧图评分或浏览器GPU witness。生成图仅为自身线性诊断目标，未据此调参或宣称抗锯齿/完整颜色输出达标。

已确认的跨轮差异：冻结scene.ts真实生产Fog按rendered camera distance及detail更新near/far，材质fog=true；B1复查时原生SceneFrame/RhiItem/shader无fog输入或合成，近距B1 fixture在源fog前不能关闭该缺口；现行修复与验证见下文R19D2，仍不作为BLUR-01正常设计或完整清晰度结论。目标D3D11/OpenGL、原生窗口与设备实机边界见R34/R19C2B1-P。

普通Release最终恢复Review OFF、可见测试OFF，构建与25/25快速CTest（含source-format/隐藏窗口策略）通过。生成器默认写入+Qt格式化后纸面shader与提交版本逐字一致，未改变已验证的C++/shader语义，因此复用上面的33项完整结果；PowerShell入口当前主机仅静态核对，目标解析/格式要求仍在R34。所有运行输出保持忽略，只有源码/工具和必要契约进入本轮提交。


## R19C2B2 光学算法、捕获与有效门禁复查

起点`3c90d2d`。恢复R10B原任务及`1f2352a`，沿R07/R08、R10A/B、R10C/D、R11–13、`5625d73`、`d03527e`、`75775b3`、`f7e423a`、`e98c798`、R19B/C1检查当前生产shader、uniform、附件与绘制。资产沿R03/R06追到锁定GLB、Three Room/DFG/LTC；历史manifest只保留来源，不以其中旧fixedOptics、uniform大小或shader hash覆盖现行源码。不重算旧截图、PMREM witness或ROI。

| 历史对象 / 当前调用链 | 源码判定及修改 | 有效验证 |
| --- | --- | --- |
| R07物理材质→case-glass / 四RHI shell变体 | 对照锁定Three BRDF_GGX_Multiscatter、DFG能量补偿、间接single/multi、diffuse守恒、clearcoat和反射pow4；系数/源材质不变。修复geometryRoughness：Three nonPerturbedNormal为view-space，max分量导数不具旋转不变性，必须先乘VIEW_MATRIX。 | 真正GPU调用生产helper；已知屏幕normal梯度在相机roll=0/45度分别应为.319984/.226263，实际.319824/.226196。旧world-space算法在45度仍为.319984，不能通过该独立数学门禁。 |
| R08音乐光场→direct/LTC | saturate点积与源一致；零入射先退出，避免相反light/view的零半向量。LTC正视时切向量为零，只有此奇点选择最不平行坐标轴构造有限正交基，其余算法不变；不称其为已确认阵列闪烁根因。 | 正视矩形LTC独立解析form factor误差.00041549≤.001；GGX、DFG多散射、shadow=0/1的diffuse/specular、相反向量共八项数值状态通过。非LTC容差max(1e-6,期望绝对值×.001)，实际最大.00019465，覆盖半精度附件量化，不读取旧渲染图。 |
| R07/R10B环境、透射、磨砂与色彩 | PMREM源生成器实际HalfFloatType，原生RGBA16F保持源类型；资产RGBA32F是序列化展开，不要求GPU字节相同。保留CubeUV方向、LTC查表、bicubic、IOR1.46及roughness×.92、局部zone .16、Beer、ACES/sRGB和paper toneMapped=false。 | 当前生产模型只有旋转/位移、scale=1，Three transmission ray的modelScale在此为恒等；未来引入缩放须重审。LOD来自实际捕获像素尺寸和源公式，是有意磨砂，不证明BLUR-01已解决。fog缺口当时交R19D，现见下文R19D2。 |
| R10B capture/resolve/壳体、R10C硬件depth适配 | 从实际QRhi对象检查采样binding4恰为opaque resolve和mip sampler、composite独立、同hardware depth且PreserveDepthStencilContents。真实命令记录opaque→resolve→mips→copy→visibility→shading；visibility写depth/LessOrEqual，颜色Equal只读、Back cull、无blend。 | 61源状态、两主题/视角/封面/邻盒/抽取/九实例，2捕获隔离和6倒序稳定键通过；没有把两图相同当门禁。法向/深度有限及投影≤2px；同深度ID及重复ID拒绝保留。 |
| R10B透明封面、R10D MSAA | 保留contain/inset、centroid、预乘滤波恢复、alphaTest .025；无模糊/阈值/透明度调参。 | 合成输入1×/4×：中心误差.00134241≤1/255，透明洞depth=1，4×真实部分覆盖1560点，黑边0。最初测试误将连续值界限与整数RGBA8比较，原48点错误失败；改用原门禁的整数码值qRound(expected×255)-1后通过，产品shader和一代码值容差均未改，失败日志留本地。 |
| R11–13实例/封面、R18生命周期、macOS后补适配 | 新诊断只在显式capture读取已提交有界uniform及真实pipeline/RT值；Review探针用保留paperSize.w，生产拒绝selector且不打包probe。最多2批pending，原读回去重/别名共享保持。 | 十次同item真实SG失效/重建恢复正常光学；数学模式/相机变化resourceGeneration不变。完整阵列/真实列/封面/详情/总览回归通过；单独稳定状态不能关闭连续闪烁。 |

事件驱动RhiCaptureHarness复用B1隐藏入口；61状态和十次重建没有processEvents轮询、Quick3D替代renderer、浏览器GPU witness或图像差分。工具新OpticsProbe只在构建目录将生产函数与测试main组合；qsb不支持此次尝试的绝对include，改为CMake显式内联两个受configure-depends跟踪的公共头，所有五目标编译通过。生成器现在同时维护early/basic四变体，生成+Qt格式化与提交shader完全一致，避免未来生成倒退修复。

COMMON-01–05/PERF-01–07及三平台符号/hash矩阵由`python3 tools/audit/audit-rhi-optics.py`生成，人工展开了实际生产函数、附件依赖、资源所有权与回调。无新增生产逐帧JSON/readback/队列，probe使用现有float读回、在RGBA8转换前解码；不以功能运行耗时宣称产品性能。当前macOS/arm64/Apple M4、最新自动发现Qt6.12.0、AppleClang21、Release、真正隐藏Metal/DPR1完整34/34、两组qmllint/check-format、六材质五目标qsb及四变体重建通过。普通配置已恢复Review OFF/可见OFF，构建及25/25快速回归通过。Windows/D3D11、Linux/OpenGL实机与PowerShell解析未执行，交R34/R19C2B2-P；正常VSYNC/窗口恢复仍按原专项。VIS-ARRAY-FLICKER-01连续定位紧随R19C2C，BLUR-01及后处理/fog仍R19D，不能用本轮通过销项。


## R19C2C 透明重叠连续漏画与位置不变性

起点`ebfb320`。追踪R10C的visibility→Equal shading优化、R10D生产SceneItem、R12/13有界批量实例、R18/19连续归位/总览以及`e98c798`跨后端适配；源Three的FrontSide、alpha=1/无blend、正常透明物理材质、排序、数量和动效均保持。用户原观察没有完整入口/设备记录，本轮建立匿名9×64库的生产路径复现，不冒称恢复用户当时的全部环境。

已确认首个异常在visibility之后的Equal颜色pass：原逐实体shader用CPU计算的`nativeVP*model`，批量shader在GPU结合矩阵；两个不同管线未声明位置不变性。同一源表面可以得到略异深度/覆盖，前pass已写深度，后pass却拒绝该片元，留下不透明捕获，随运动形成孔洞/闪烁。独立九实例/1×诊断原漏14646点；只统一GPU运算仍漏17171点；只声明invariant在简化输入为0，却在640×480生产状态177个可见实体中仍漏1191点。最后同时统一`p=model*vertex; gl_Position=nativeVP*p`并声明`invariant gl_Position`，以下完整连续样本均为0。这些控制变量日志保持忽略；未将LTC奇点或正常光场动效误称为本问题根因。

[GLSL不变性规范](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html)要求跨程序相同输入和相同计算；[Apple位置不变性说明](https://developer.apple.com/documentation/metal/mtlcompileoptions/preserveinvariance)解释多pass一致计算用途。qsb实际拒绝MSL1.2的invariant，故仅两个geometry顶点shader使用MSL2.1，SPIR-V/GLSL330/ES300/HLSL50仍生成，其他shader不改最低MSL。实际提取的MSL含`[[position, invariant]]`，HLSL50含precise SV_Position，GLSL330含invariant gl_Position。本机观察不外推为任意Metal编译器/设备保证，目标仍跑同一精确门禁。重用本来已计算的world position，删除每份uniform不再使用的CPU矩阵乘法；mvp槽保留零值以保持816-byte ABI，不搬动其他字段。

`rhi-flicker-audit`新增Review专属逐pass覆盖探针：visibility只在颜色附件写magenta，真实生产shell helper执行后，有限颜色片元写white，非有限颜色写green。Equal漏画会留下magenta，包括完全覆盖中的部分MSAA样本；不是两图相似/差分，不用正常输出推断阈值。默认产品拒绝selector，两个probe shader都不打包；SceneItem只在Review保留该selector以防异步封面polish覆盖。其他normal/depth、捕获/mips、材质、前后表面和资源边保持。每段前30帧诊断、后30帧普通输出，所有真正浮点读回仍经imageOf有限值检查。

实际Release/arm64/Apple M4/最新自动Qt6.12.0/Metal隐藏DPR1：1/7/8/9实体跨批量阈值×1×/4×共8项；生产640×480和1280×720、暖昼/深夜、静止/活动波浪/浏览/总览进入退出共16×60=960帧，其中480诊断、480正常光学。预期missing=0、nonfinite=0、有效shade>100，全部检查真实有界实体、上传预算、模式和resourceGeneration，静止之外以原FrameClock每次1/60推进、速度1/减少动态关闭。正常自身输出取有限代表帧检查；不读取桌面内容，不进行旧图配对。实际可见实体146–247，生产目标分别213×159/853×479；大小沿现有生产质量路径原样记录，小视口目标偏低及BLUR-01仍由R19D逐源判断，不能用本轮修正关闭模糊问题。正常VSYNC、窗口drawable与用户原设备未测。

COMMON-01–05/PERF-01–07/三平台符号/hash入口`python3 tools/audit/audit-rhi-flicker.py`。回放事件驱动、封面ready以帧事件等待、有界超时、读回串行；诊断切换会重建特化pipeline，仅Review测试成本，真实target generation每个序列保持不变。已有十次SG重建/尺寸/封面等随同完整隐藏回归，产品request=0不增加读回/逐帧图像分析。Windows/D3D11、Linux/OpenGL本机不可执行，保留R34/R19C2C-P；R19H继续核对本机已修项与其他后端未验范围。

本轮完整隐藏Release回归35/35（含960帧Metal及既有十次重建）、check-format、两组官方qmllint通过。生成器在临时隔离资源树执行并格式化后，instanced顶点shader与提交版本完全一致。额外本机OpenGL独立8项光学通过，但完整入口首个生产采集因R16F rawAO出现Nonfinite scalar AO pixel而终止，原失败与输入已完整写入紧接R19D主线；不是目标环境缺失，不能推到R34或记为全管线通过。工具显式`--optics-only`只关闭该专项的后处理捕获，用于独立透明路径定位，默认完整门禁不变。

OpenGL最初的optics-only尝试曾在后半程又读回AO：SceneItem异步polish重建Input恢复了后处理，诊断覆盖不完整；现在仅Review的动态属性在polish处保持专项配置，普通产品无此入口。连续逐帧切换诊断特化还触发AGX compiled-variants footprint日志，最终改为每段前30诊断、后30普通以有界切换完成相同480+480覆盖；该日志不归因于正常产品逐帧工作。默认完整入口仍保留AO有限值断言，未用optics-only修改或代替它。

本机隐藏OpenGL最终optics-only 960帧通过，覆盖/有限值均0；另以正常`rhi-shell-gpu`入口执行61源状态、数学探针及十次重建通过（11.46秒为该功能运行耗时，不是性能结论）。完整OpenGL生产AO失败仍未销项。Metal默认完整路径在最终诊断分段方式下另跑连续门禁，其他未改共享项复用上述35/35；所有采集物只留本地忽略目录。

最终默认Metal连续门禁72.52秒通过；恢复Review OFF/可见OFF后，普通Release构建和25/25快速CTest通过，source-format与隐藏窗口策略通过。所有日志/图像/矩阵输出保持忽略，未提交采集产物。


## R19D1 SSAO 算法、标量读回与常量数据复查

起点`2d3c9b1`。实施前按规模将R19D拆为D1（本节SSAO）、D2（fog/颜色）、D3（阴影）和D4（景深/AA/尺度/整体门禁），保持顺序和R20前全部完成约束。恢复R09 `3fc6a39`、R09B `522a3f3`、R10C `1f2352a`、R10D `75775b3`，沿`5b2df80`、`e98c798`及后续捕获/缓存修正追到实际生产。下列判断只覆盖SSAO及共享数据边，不表示Bokeh/SMAA、fog、shadow或BLUR-01完成。

| 历史对象 / 实际调用 | 源码依据与处理 | 验证 |
| --- | --- | --- |
| R09：scene构造→enableSelectionLighting→softenMusicContactShadows→setQuality再次soften | 实际半径.18、normalized orthographic depth min/max .001/.035、.22乘数；构造.38/.44与.09不覆盖最终音乐入口。完整壳体及纸面normal override保持，不受纸面透明contain裁切。 | 生产值保持；额外算法压力状态明确使用原fixture .1/100、radius .38，不改产品参数。 |
| R09：锁定Three SSAOShader、SSAOBlurShader→共享ssao及三RHI展开变体 | 源只unpackRGBToNormal，删除额外normalize；源depth==1哨兵改为存储RGB24三通道精确white判断，删除扩大的1e-6范围，并避免背景normal多余fetch。保留透视标量重建、view normal、scalar noise复制、TBN、全部样本与严格delta判定、5×5 box及.22合成。 | 三核各81个固定数值点从同次真实normal/depth输入执行独立源公式；有接触遮蔽点，raw最大误差≤.000491、blur≤.002059（均≤1/255）、composite≤.003137（≤2/255）。不删难点，不读取旧GPU图；合成容差含输入/输出两次RGBA8诊断量化及half rounding。 |
| R09/R10C：kernel/noise输入与固定索引 | 按计划seed417直接执行锁定SSAOPass/SimplexNoise生成16/32/64六份输入，临时目录与已有资产完全一致；只比较数学输入数据，不是GPU byte witness。RHI半径basis提升与固定索引展开保留，原累加顺序/样本数量不变。 | 无浏览器/GL上下文的源输入重建、三变体生成+格式化一致；公共Effect四snippet五目标qsb和RHI三变体编译。 |
| R10D：RGBA16F→标量R16F及诊断解码 | R16F生产raw/blur数学精度和消费者.r等价性保留。查明此前OpenGL错误发生在读回边：本机GL_PACK_ALIGNMENT=4，奇数宽R16F行需填充；当前Qt GL readPixels按紧密纹理字节数分配，未修改PACK_ALIGNMENT，可能越过逻辑缓冲边界。非有限字节不证明产品AO shader生成NaN。 | 原背景恒1输入在213×159出现错误，修复后奇偶宽/三倍率/三核27状态均为1，尺寸/格式仍为原R16F；没有通过RGBA8或R32F降格替代。原R19C2C失败保留。 |
| R10C/D及后续capture：行跨度、资源/别名 | 显式捕获且仅未对齐的GL R16F，通过QRhi同格式copy到缓存的补齐宽纹理；原区域逐值保留。按实际storage stride解码且只消费逻辑区域，未初始化尾列不扫描。最多2个AO缓存槽，实际清单含capture-aligned-scalar；同尺寸复用、renderer reset清理。只查询GL状态，不全局改pixel-store，不锁定旧Qt。 | 实际pack=4；213→214、129→130的诊断存储宽不改变产品目标、采样或清晰度。复采alignedReadbackRevision不变，真实十次SG失效/重建恢复。pending最多2、唯一纹理读回和GUI值交接保持。 |
| R10C/D：PostUniform及重复kernel上传 | RHI已把samplesMap改为uniform索引，却仍分配/上传未读取的kernel texture；每个pass还复制/上传整个1104-byte块。删除闲置纹理及资源计划条目，kernel只在SSAO pass创建时一次上传；每帧只更新80-byte变化前缀，保留1104-byte std140 ABI及kernel offset80。 | 真实命令计数postUniformUploadBytes：AO三pass常态240，原3312；不代表后端物理传输字节或FPS。三核源数学/重复捕获/长回放验证部分更新保留常量；无新增生产GPU读回，driver实际带宽/显存未知。 |
| macOS GL能力、窗口及目标平台 | 保留原MSAA等价renderbuffer及真实标准透视/行方向。bake-occlusion-shaders改用实际host的qsb路径/输出，Windows脚本未在本机执行。一次最终GL回放在14/16序列后报window policy failed，未报AO非有限；当时缺原生失败快照，原因未确认。 | 失败保留、不归因于Qt主动激活或用户操作；共享harness补失败时原生策略JSON且不放宽门禁，用相同非前台策略重跑。D3D11/Linux目标继续R34。 |

Qt读回边界依据[当前Qt GL实现](https://github.com/qt/qtbase/blob/6.12/src/gui/rhi/qrhigles2.cpp)的ReadPixels路径和实际pack查询；不是以Qt版本名决定退回其他Kit。补齐诊断只增加显式capture的同精度copy，正常request=0不会查询/分配/复制；持久缓存计入实际descriptor inventory，与生产preallocation plan范围区分。COMMON-01–05/PERF-01–07及目标平台逐符号/hash由`tools/audit/audit-rhi-ao.py`生成；人工核对源公式、same-format copy、弱pending、缓存销毁/Kit重建及GL/D3D/Metal路径。

当前macOS/arm64/Apple M4、自动最新Qt6.12.0、AppleClang21、Release、Metal/OpenGL真正隐藏DPR1。源SSAO/box/合成点与27状态/十次重建用`rhi-ao-gpu`执行；完整生产双主题/大小/静止/波浪/浏览/总览960帧由默认`rhi-flicker-gpu`执行。前一版完整Metal36/36通过；常量上传优化后执行最终完整回归。公共Effect只是五目标静态编译，不冒称独立Quick3D AO已实机通过，旧图像耦合入口仍由D4拆解。正常窗口/VSYNC/最终产品性能与不同设备按原专项；所有生成物保持本地忽略。

最终常量上传优化后的Metal完整隐藏36/36通过，OpenGL的27状态/源数学/十次重建与默认完整960帧两项均通过；未出现AO非有限或光学漏画，窗口策略亦通过。先前一次window-policy失败原样保留，未放宽断言，未确认该次前台变化原因。耗时与此前不同仅记录为功能运行事实，没有控制正常VSYNC/环境噪声，不能据此作性能优劣结论。最后恢复普通Review OFF、可见OFF配置并运行生产Release构建/快速回归。

最终已恢复Review OFF/可见OFF，普通Release构建及25/25快速CTest通过；最终check-format/两组官方qmllint与四公共Effect五目标bake通过，三RHI SSAO变体重建逐字一致，六源输入无需GPU重建一致。Windows/D3D11与Linux/OpenGL实机按R34/R19D1-P保留，不外推本机通过；下一主线R19D2继续生产fog/颜色，BLUR-01仍未销项。

## R19D2 生产雾、颜色域及捕获次序

起点`4ab1e73`。逐项恢复R07/R08材质与光场、R09B `522a3f3`、R10A `52f1278`、R10B `5625d73`、R10C `1f2352a`、R10D `75775b3`，沿 `f7e423a`、`e98c798` 与R19C1/C2修正追到生产SceneItem。锁定Three 0.183.2的源码来自冻结依赖，不以某次浏览器输出为依据。

| 原轮次 / 生产调用 | 源码判断与原生处理 | 门禁与边界 |
| --- | --- | --- |
| R07：MeshPhysical/meshlambert及颜色输出 | `meshphysical/meshlambert` 都按opaque→tonemapping→colorspace→fog顺序；`WebGLRenderer::setProgram` 对普通render target关闭逐材质tone mapping，输出域为LinearSRGB。Quick3D独立材质Review的本地ACES只服务非compositor探针，生产RHI的lightingEnabled始终为true、壳体只输出线性radiance。 | 保留独立Review兼容分支，不将它解释成生产逐物体ACES；生产最终统一输出。 |
| R08：纸面、光场及主题 | `createAlbumPrintMaterial` 的toneMapped=false/fog=true、Lambert限幅与printExposure保持；scene.setTheme同时过渡fog/background，实际音乐入口覆盖曝光为昼1.05/夜1.08。`WebGLMaterials::refreshFogUniforms` 在普通RT使用workingColorSpace。 | 直接复用SelectionLighting的线性background作fogColor，保留主题过渡而非读取静态常量；纸面toneMapped=false仍受到最后OutputPass影响。 |
| R09B：scene.ts的EffectComposer→RenderPass→SSAO→Bokeh→SMAA→OutputPass | composer及透射RT都是线性half颜色，颜色shader后的fog仍在线性域；最终OutputPass执行一次ACES/sRGB。保留原native输出系数和exposure/.6，不添加gamma、白雾或截图补偿。 | `color-output.glsl` 从现有output.frag原样抽取，Review直接执行同一生产helper与独立double数学比较；没有改变后处理顺序或采样。 |
| R10A：opaque与纸面捕获 | 此前遗漏fog。新增source `smoothstep(near,far,viewDepth)`，Lambert/场核完成后混入线性fogColor。background不再额外fog，normal/depth不fog，diagnostic shadow receiver不代表音乐产品floor。 | 实际提交字节记录paper kind=0的range/enable/color；保留RGBA16F、透明alpha测试、MSAA和resolve。 |
| R10B：透射capture、壳体及材质输出 | `renderTransmissionPass` 的opaque capture已含源fog，resolve→mips后供壳体采样；壳体完成BRDF/透射/Beer/场核后，再按自身viewDepth执行fog。两次发生在不同表面，不能为避免所谓“重复雾”删除任何一处。 | 四个shell变体与生成器同步；实际绑定/捕获隔离、Equal深度、源透明度和材质参数保持。 |
| R10C：后处理与输出 | production shader不执行逐物体ACES，fog先于SSAO/Bokeh/SMAA与最终ACES；outputOptions取自真正上传的数组。rawOutput仅供独立Review，生产false。 | 生产纸面和壳体提交uniform与snapshot逐项匹配；共享完整隐藏回归涵盖后处理、封面、资料/场景与二维界面，不执行旧图像分数。 |
| R10D：SceneItem→SceneFrame→RhiItem及缓存 | `SceneAdapter::publish` 依据实际rendered camera position/aim计算near=distance+lerp(5,-1,detail)、far=distance+lerp(25,12,detail)。新增detail-only失效，避免同相机姿态时沿用旧雾；SceneItem正常路径始终启用。 | 一次O(1)计算；std140末尾追加fog vec4到832 bytes，既有offset不变；只增加既有uniform前缀载荷，不建新纹理/pass/SRB/PSO，不增加生产readback或队列。 |
| 后续格式、平台、R19C修正 | `5b2df80`仅格式；`e98c798`必要GL多采样renderbuffer/上下文适配保持；R19C1删除的拟合材质系数没有恢复。geometry的现有负view-Z直接复用，位置不变性及相同计算顺序保持。 | Metal/本机OpenGL可隐藏验证，Windows/D3D11与Linux/OpenGL目标硬件不由本机推断；完整产品VSYNC/设备任务仍R34。 |

`scene`新增观察远离target的entry pose、连续详情/反向中断、detail-only发布测试；`rhi-fog-gpu`固定线性radiance在七个深度分数（含起止外侧）、启用/禁用、双主题下，共84个实际GPU数学状态，分别核对fog、fog→ACES、fog→ACES→sRGB。数值读取为RGBA16F原值，期望由独立公式与同次RGB24几何深度计算，容差.001覆盖half舍入和depth量化；这不是来源截图、浏览器GPU字节或图像拟合。

`rhi-flicker-gpu`使用真实SceneItem、匿名9×64库及实际封面，双主题/640×480与1280×720、静止/波浪/浏览/总览进退/详情.5→1→0共1200帧；逐帧核对生产fog开启、实际相机/详情公式及paper/shell真正上传range/color/outputOptions。独立`rhi-fog-gpu`还执行十次真实SG失效重建，确认有效几何与雾状态恢复。COMMON-01–05/PERF-01–07三平台符号/hash由`tools/audit/audit-rhi-fog.py`写本地忽略目录。

源码雾是有意的远距离颜色衰减，不改变纹理分辨率、LOD、景深或目标尺寸；不将它视作VIS-ARRAY-BLUR-01的结论或修复。剩余景深/抗锯齿/缩放/整体门禁继续R19D4，阴影继续R19D3；不以本轮有限值或源码通过关闭用户问题。

当前macOS/arm64/Apple M4、自动发现最新Qt6.12.0、AppleClang21、Release、真实隐藏DPR1：Metal完整37/37（包括1200帧生产回放）通过；补入雾启用的十次SG重建后，Metal单独fog门禁通过，OpenGL的fog与1200帧两项均通过。两后端84状态最大数学误差均为.000471473<.001，无异常覆盖/非有限值；fog参数变化不重建目标。已检查自身最终输出和生产颜色目标，未用源实机图或图像分数调参；最终输出探针与生产线性颜色捕获分别标明，不混称屏幕采集。两组官方qmllint/check-format通过，四个shell生成+format可逐字重现。最初受限进程无法连接WindowServer而无screens，原生会话下隐藏验证通过，未显示或激活测试窗口。Windows/D3D11、Linux/OpenGL目标检查静态通过、实机未执行，完整输入/入口/门槛交R34/R19D2-P；本机离屏不证明窗口/VSYNC/设备或完整产品性能。

已恢复普通Release的Review OFF、YALAHOO_MACOS_VISIBLE_TESTS=OFF；生产构建、最终25/25快速CTest（含source-format和默认隐藏窗口策略）、两组官方qmllint/check-format通过。验证生成物保持忽略，暂存仅源码/测试/必要契约；下一主线为R19D3。


## R19D3A 阴影投射、normalBias 与 PCF

起点`2f50812`。实施前发现源产品地面缺失：冻结 scene 构造 floor 为200×200、y=-4.63的 MeshStandardMaterial，原生只在 shadowDiagnostic 绘制12×12白色receiver。其 PBR、主题、fog、AO/DOF 依赖超出单一过滤修复边界，因此先将R19D3拆为连续A/B；本节不代表地面已实现，R19D3B保留完整实现和原接收验收。

| 历史轮次 / 实际路径 | 源码判断与处理 | 验证范围 |
| --- | --- | --- |
| R09 `3fc6a39`：GeometryScene公开 DirectionalLight / shadow probe | PCF16、shadowFactor32、shadowBias.035属于当时Quick3D公开hook的近似，不能作为源等价生产阴影。实际生产早由R10D替换；保留独立Review语义与局限，不把该诊断灯叠加到生产RHI。 | 核对CaseGeometry仅diffuser casts；Quick3D旧图耦合入口仍由D4拆解。 |
| R10A `52f1278`：RhiApi / caster / opaque receiver | 灯(-6,14,-5)→原点，ortho[-16,16,-15,15,.1,45]；FrontSide材质在source shadow pass用BackSide，故RHI Front cull正确。只有Optical_Diffuser/tray投射，纸面/壳体接收；源music入口将strength设为.32，不沿用默认1。 | 独立灯矩阵/已知接收平面和本次float depth的PCF数学；实际附件和提交参数检查。 |
| R10A `52f1278`：normalBias投影阶段 | 原在fragment对插值法向normalize后计算投影；源shadowmap_vertex按每顶点归一化世界法向偏移后插值。两个geometry变体增加shadowCoord，四shell变体及opaque共享消费，移除片元重复矩阵/normalize。 | 当前三源mesh所有三角形内部法向恒定，记录此边界；恢复源阶段，不声称它导致此前阵列闪烁。shader生成器同步。 |
| R10B `5625d73`：共享shadow-filter与physical/Lambert接收 | 壳体keyShadow作用于key diffuse/specular，纸面仅减去被遮挡key diffuse；其他灯/环境不受影响，保留nl>0后采样。radius4、.035 normalBias、-.0003 depthBias、frustum及.32强度没有图像拟合来源。 | 1200帧实际生产paper/shell上传值逐帧检查，原光学/覆盖/雾检查保持。 |
| R10C `1f2352a`：手动PCF→硬件sampler2DShadow，RGBA32F→R32F+D32F | 锁定Three0.183.2运行时将PCFSoft转为PCF；五Vogel点各硬件bilinear comparison。保留原生适配和R32F单通道诊断，不退回旧手动采样或降低精度。 | 当前深度上独立compare→bilinear→五点→mix数学，包含PCF边缘；检查真正D32F/R32F、single sample、Linear/LessOrEqual。 |
| R10D `75775b3`：SceneItem与缓存/计划 | 画质供给0/1024/2048/4096；变化实体/开关/尺寸/renderer generation失效，固定光相机无需随接收相机/颜色变动重绘。保留离屏caster，不用相机可见性裁掉可能投影到画面的盒体。 | 1/7/8/9批量边界、重复捕获、相机/灯色变化不重建，单caster变换重绘、关闭清空/重新开启及十次真实重建。 |
| 后续PERF/Metal/OpenGL及 `e98c798` | 固定有界pool、同Kit qsb、canonical/raster矩阵分离、UV/offset/IGN一致翻转和GL真实入口探测/renderbuffer保留；不新增depth offset、epsilon或阴影图像补偿。 | COMMON-01–05/PERF-01–07与三平台symbol/hash由audit-rhi-shadow.py生成；目标硬件与呈现结论分开。 |

PCF数值是同次真实几何深度上的独立源公式，非源实机图/浏览器GPU字节配对。显式捕获共享现有R32F QByteArray，不新增readback或生产图像分析；实际paper/shell uniform数值与receiver上传块分别核对。产品地面仍R19D3B；景深/AA/缩放/BLUR-01仍D4，正式产品性能仍R28/R34。


性能补修：未失效或关闭阴影时，非批量路径不再append/复制专用caster uniform；batch几何块仍供既有prepass/几何使用。原cursor范围上传随之缩短，map/光学/材质不变；实际separateCasterUniforms为0的检查与矩阵变化重绘一起验证，不把API请求字节节省写成FPS或物理总线带宽结论。

新增几何探针从源GLB的backface三角形计算light投影和同次R32F深度：最初将恒深纸面的2e-6直接用于倾斜面时失败约3.81e-6；源运算顺序/gl_FragCoord.z控制实验未消除差异，均未作为产品修复保留。全部已选采样继续保留，用解析平面每纹素depth梯度换算既有≤2物理像素投影门槛，恒深面仍depth<2e-6，同时报告未换算的depth误差；Metal最大约.00184纹素，不假定具体GPU子像素位数。


最终实际环境为macOS27.0.1/arm64/Apple M4、自动发现最新Qt6.12.0、AppleClang21、Release，真实隐藏DPR1。Metal最终完整38/38通过，包括1200帧生产回放；新增最终报告字段后的shadow专项单独通过。OpenGL实际backend=2，shadow/flicker/shell三项通过，包含相同1200帧；两后端各9组shadow/十次真实SG重建，PCF最大误差.00244663≤2/255，几何投影最大分别.00183449/.00184885阴影纹素≤2。自有接收图和代表性正常生产输出已检查；没有源实机图配对、present/VSYNC或正式性能结论。

实际API uniform请求：单caster首次6144、缓存5120 bytes；7caster首次18432、缓存11264 bytes，分别消除1024/7168 bytes专用阴影块；≥8实例复用既有batch几何块，此项为4096→4096。数值包含原其他draw，属于API请求范围而非驱动物理传输/显存或FPS。两组官方qmllint、check-format、四shell与instanced vertex生成+format逐字复现通过。COMMON-01–05/PERF-01–07/三平台symbol/hash及清单仅本地生成。Windows/D3D11、Linux/OpenGL实机未运行，R34/R19D3A-P保留；受限服务最初no-screens及两次仅foregroundUnchanged=false的失败与数值探针旧协议失败均保留，未宣称根因已修复、未放宽窗口断言。产品地面仍R19D3B，DOF/AA/尺度与BLUR-01仍D4。

已恢复普通Release的Review OFF、可见测试OFF配置；生产构建、最终25/25快速CTest（含source-format/默认隐藏窗口策略）、check-format及两组官方qmllint通过。采集生成物保持忽略，下一未完成主线为R19D3B。

## R19D3B 产品地面与接收闭包

起点 `e9360b7`。冻结来源 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 经历史映射读取 `scene.ts`，地面构造与工作树相同。以下仅关闭缺失地面的确定遗漏，BLUR-01、景深/AA/目标尺度总体审计仍属 R19D4。

| 原职责 / 有效调用 | 实现与判断 | 验证边界 |
| --- | --- | --- |
| R09/R10A：scene 构造与 shadow 接收 | 恢复 200×200 PlaneGeometry、rotation.x=-PI/2、y=-4.63、向上法向、FrontSide、receiveShadow=true/castShadow=false；保留旧12×12诊断receiver，两者独立。 | 解析相机射线、实际normal/RGB24 depth与原≤2物理px投影要求；caster数量仍仅盒体diffuser。 |
| R07/R08/R19D2：主题/光场/雾 | SelectionLighting 的不可变线性 palette 增加 floor，day/night/dusk原色分别为#d8c9b9/#0b1828/#a6b8c0。产品双主题沿原.65秒bezier、途中反向和减少动态更新；dusk只验证已有源颜色，不新增产品主题。 | 源色sRGB解码与中断状态单元检查；SceneItem实际提交颜色逐帧匹配当前palette，不从warmth反推地面颜色。 |
| R10A/B：不透明与透射capture | opaque kind=6执行Standard PBR：roughness=.95、metalness=0、dielectric f0=.04，无clearcoat/transmission/emission；方向/补光/半球/spot、LTC area、DFG multiscatter及Room PMREM按锁定Three源公式。地面不用musicShell、纸面限亮或printExposure。 | 独立CPU Standard BRDF、DFG/PMREM采样、同次R32F上的PCF与线性fog；地面写normal/depth，fog后resolve/mips供玻璃采样，后处理及ACES/sRGB顺序保持。 |
| R09B/R10C/D：AO与Bokeh几何 | 产品地面同时进入独立normal/depth几何pass。AO全分辨率时Bokeh复用；半分辨率AO时单独完整DOF depth也覆盖地面。 | 新captureFloorDepth仅显式Review读取实际DOF附件；最多14语义字段/2在途批，按唯一纹理读回及QImage隐式共享交付。产品默认关闭此读回。 |
| 后续性能/Metal/OpenGL | 地面共用既有opaque PSO/SRB及DFG/LTC纹理，增加4顶点+6索引共152 bytes的一次初始化上传；复用既有receiver uniform槽、每个适用pass一次O(1) draw，无新生产目标/纹理/队列。 | 颜色变化不重建目标或shadow，三map尺寸/开关、1×/4×MSAA、十次真实SG重建；资源描述与partial uniform规则保持，不将API字节或离屏耗时当作VSYNC性能。 |

有限参考 REFDB-01：读取外部 `Qt/src/RhiScene.cpp` 的 plane/prepare/render 与 `Resources/runtime.ts` 的floor分类/材质状态。仅确认“单个静态平面随不透明几何进入通道”的遗漏检查价值；本项目已有固定网格、输入值和SRB，未复制代码/架构/协议或数值，不迁入外部作者/许可未确定的自维护片段。实际读取SHA256分别为 `b2f21e3f7a4dc4e8268041ac3ff543dbcb8dc41615aa2fe78ada89c2fa8a0081`、`c86bdf93febae6a93fae2b2c4b195cedfdb822cf8ee99677c00c568e455a3fd5`；原文件保持原署名。新增floor helper沿既有Three MIT派生公式，许可证仍在resources/licenses/three-LICENSE；`audit-rhi-floor.py`独立核对LTC/GGX与现有源派生实现的token等价，未修改壳体shader。

复现：Review ON的Release构建后运行 `rhi-shadow-gpu`（内含地面专项）、`rhi-flicker-gpu`（真实SceneItem匿名9×64库、双主题/640×480与1280×720/静止/波浪/浏览/总览进退/详情共1200帧），再跑受影响完整隐藏回归。地面专项320×240/960×720，14组颜色/map数学场景及同状态缓存/改色/阴影关闭，另覆盖1×/4×采样、全/半AO、实际DOF附件和十次重建。小视口只是覆盖地面接入，不代替D4同状态/DPR/render scale的BLUR-01调查。

三平台COMMON-01–05/PERF-01–07与平台符号/hash/清单由 `python3 tools/audit/audit-rhi-floor.py` 生成到本地忽略目录。Windows/D3D11、Linux/OpenGL缺目标系统/设备，只能严格静态审计，实机任务在R34/R19D3B-P；本机离屏不证明原生窗口、正常VSYNC、物理输入或完整产品性能。验证产物与失败诊断均不提交。

本轮实际环境为macOS/arm64/Apple M4、自动发现Qt6.12.0、AppleClang21、Release、真正隐藏DPR1。Metal/OpenGL地面14组源数学各通过，最大PBR/fog误差均.00239867≤3/255、平面投影最大分别.00123643/.00119814物理px≤2，原始depth差最大约5.45e-5/5.11e-5世界单位；每个开启阴影的源数学场景均有真实遮挡点。十次地面SG重建、AO/full DOF附件、缓存/三map检查及两后端各1200帧生产回放通过；本机GL实际backend=2。自身地面/实际生产输出已检查，不进行源图比较，也未关闭BLUR-01。

完整Metal隐藏集首次36/38通过：source-format因最后补充探针尚未格式化失败，修正后按原门禁复验；shell仅前台PID变化触发策略失败，记录native visible/active/activationObserved=false，原因未确认，保留失败且不放宽策略。最终相同策略的Metal shadow/shell均通过；OpenGL shadow/flicker/shell三项通过。其余未受收尾影响的完整隐藏结果复用，不重复整套矩阵。三平台严格静态审计及helper等价、两组官方qmllint/check-format通过。最后恢复Review OFF/可见OFF，普通生产Release构建与25/25快速门禁实际通过（含source-format和默认隐藏窗口策略）；原始输出、失败及自有采集仅保存在本地忽略目录。

## R19D4A Bokeh 源码与真实生产采样

起点`f4e1610`。按实施前规模评估将原D4拆成连续A（Bokeh）、B（SMAA/MSAA及旧入口拆耦）、C（尺度/BLUR-01与集成收口），本轮仅A。冻结来源经history-rewrite读取且匹配原manifest；锁定Three0.183.2/BokehShader的SHA256为`bd8525158e3574a803f2e2ce5d5e263349a80ee41bf789e71446b9caa256aa9a`，MIT署名/许可证继续保留。R09B `522a3f3`、R10C `1f2352a`、R10D `75775b3`、e98c798的实际Bokeh数学体与当前一致（仅忽略格式空白）；无需增加补偿或更换RHI绘制路径。

| 生产边界 | 源码结论与本轮验证 |
| --- | --- |
| scene.ts → SceneAdapter → SceneItem | focalPoint为entity+(0,2,0)经实际view变换，focus=-viewZ；aperture=(.0003+.0005×detail)×强度/100，构造期.0018不是有效帧常量。1200帧真实SceneItem中，逐帧核对真正上传前缀与源公式、完整DOF depth尺寸、near/far/aspect/maxblur。 |
| R09B公共Effect → R10C/D生产Postprocess | RGB24线性depth、nearest深度/linear颜色、41点与.9/.7/.4环、±.011及aspect全部保留。独立double双线性/source求和读取最小源坐标JSON，直接执行生产qsb；不使用浏览器GPU数据或源截图。 |
| 缓存、资源与上传 | Bokeh-only一个RGBA16F ping目标及一个1104-byte容量uniform，实际每帧只上传80-byte前缀；改focus/aperture不重建，无input/output反馈，关闭释放目标。十次Postprocess真实reset/recreate验证恢复与另一组near/far；这是后处理资源生命周期，不是scene-graph/native窗口恢复。 |
| PERF-01–07与平台 | O(41×目标像素)，固定可见池不随全库增长；既有render所有权/预载缓存，不增加生产读回、队列、I/O或时钟。Review实际前缀保存仅在Review编译，序列化仅显式request。COMMON/PERF/三平台symbol/hash及历史matrix由audit-rhi-bokeh.py输出到本地忽略目录。未采用另一套渲染器，当前没有效果或成本依据要求替换。 |

当前实际macOS/arm64/Apple M4、自动最新Qt6.12.0、AppleClang21、Release：Metal与本机OpenGL（backend分别4/2）各46状态通过，含36组focus/强度/横竖/奇数目标和十次资源重建，每后端1799438个解析像素；最大绝对颜色误差均约.001953125≤既有颜色公式3/255门槛。输入含HDR>1与不连续RGB24深度，输出有限、alpha精确1、色域/尺寸/资源断言通过，实际topLevelWindows=0/visibleWindows=0且无focusWindow。自身线性裁幅预览与生产暖昼/深夜输出已查看，只用于质量检查，不以小目标缩略图关闭BLUR-01。

共享Metal Release完整隐藏39/39通过，包含1200帧生产回放及既有AO/阴影/光学/覆盖/资源/二维回归；两组官方qmllint与check-format通过。生产Bokeh qsb实际保留SPIR-V/GLSL330/ES300/HLSL50/MSL12五目标，跨目标翻译不等于目标GPU执行。没有改生产shader/采样/资源图，不声称FPS提升；本轮局部成本只验证80-byte API上传与附件/缓存，不将offscreen等待当作CPU提交或呈现性能。

受限进程最初无法获得MTLDevice，原生执行环境下真正隐藏验证通过，原失败日志保留本地；没有通过显示窗口绕过。Windows/D3D11、Linux/OpenGL缺实际系统/Kit/设备，只做严格静态审计，原门槛和生产回归交R34/R19D4A-P。AA、旧图像入口覆盖映射与目标尺度仍D4B/C；BLUR-01尚未关闭，正常VSYNC/物理输入/完整性能仍R28/R34。生成图/日志/逐帧与matrix不入Git。

最终普通Release已恢复Review OFF/可见OFF；生产构建、25/25快速CTest（含source-format/默认隐藏窗口策略）、两组官方qmllint及check-format通过。GL本轮实际执行独立46状态；没有修改生产shader/目标/事件驱动，GL生产1200帧与scene-graph边界复用R19D3B的有效事实，不将新增实际前缀断言记作本轮已在GL生产回放运行。原生可见窗口/正常VSYNC/物理设备矩阵本轮未选择，按原任务保留。Git仅新增最小41点数学输入、验证源码和必要契约。

## R19D4B1 SMAA 源码、查表与真正离屏门禁

起点`704a99d`。原D4B同时包含SMAA、MSAA/resolve与四套旧门禁覆盖迁移，实施前按一小时/单集成边界拆为B1与B2；本轮只处理B1，B2保留MSAA/resolve、真实生产组合及旧非像素检查逐项闭包。锁定Three0.183.2 SMAAShader/SMAAPass与既有manifest SHA256一致，两个lookup逐字等于源内嵌PNG；原MIT署名/许可保留。R09B `522a3f3`、R10C `1f2352a`、R10D `75775b3`及后续Metal/OpenGL适配`e98c798`的三个数学helper保持；当前canonical framebuffer适配由不同Y方向的真实后端独立验证。没有修改生产算法、阈值、搜索数、gamma或绘制路径。

| 边界 | 源码/实际检查 |
| --- | --- |
| 冻结scene.ts/RenderQuality → SceneItem → Postprocess | 源SMAA在Bokeh后、一次Output前；高/极高启用SMAA，普通antialias off不关闭几何MSAA。静态核对实际设置发布；生产组合的完整闭包仍B2/C。 |
| edges → weights → blend | 独立double像素坐标公式对应.1阈值/局部对比/八步搜索/crossing/area与search采样/gamma2.2/alpha；分别读回三个真实生产RGBA16F目标，无浏览器GPU、旧图或ROI评分。源`float(ivec2)`只取首分量再广播，CPU遵守真实展开语义；初期按二维offset转写的错误及失败日志保留本地，未改产品去追错误期望。 |
| 资源与上传 | 三个独立单采样RGBA16F目标、两张无mip RGBA8 lookup、三个1104-byte容量uniform；描述符载荷估算`24×目标像素+370424`bytes，不是实测VRAM。每次真实API前缀共240 bytes；输入内容/focus变化不重建，同资源回到flat无残留；十次真实Postprocess重建、关闭全部释放。 |
| PERF-01–07/COMMON-01–05及平台 | 固定三屏幕pass，最多八步搜索，O(目标像素)，不随曲库增长；既有render所有权/预载缓存，不增加产品I/O、队列、时钟、复制或读回。中间目标访问沿submittedBokeh的Review模式，普通构建返回nullptr，类声明跨编译单元一致。静态symbol/hash矩阵写本地忽略目录，目标真实GPU另列。 |

macOS27.0.1/arm64/Apple M4、自动最新Qt6.12.0、AppleClang21、Release：Metal与本机OpenGL各34状态、每后端277610个解析像素通过，实际topLevelWindows=visibleWindows=0且无focusWindow。64×64/129×73/73×129包含斜向、相交边、矩形、低对比、局部抑制、动态后flat清零、HDR/alpha和十次资源重建。最大edges误差均0；weights为Metal .0008544921875、GL .00054931640625；blend均.0009765625，低于既有公式门槛1/255与3/255。自身两向斜边预览已查看，无方向颠倒/漏画/错误合成；这是解析小目标SMAA质量，不是小窗口BLUR-01或屏幕显示验收。

三个生产qsb均核对含SPIR-V、GLSL330/ES300、HLSL50、MSL12；跨目标翻译不等于目标运行。报告中的局部CPU命令记录（含创建期lookup上传）只是一次功能回放诊断；没有三次成本比较，不推导GPU/VSYNC/FPS改善。资源/常态240-byte API请求为本轮成本结论，后端物理传输与driver分配未知。

沙箱初次无MTLDevice，原生服务权限下同一真正离屏入口有效；没有show/抢焦点。Windows/D3D11、Linux/OpenGL当前缺对应系统/Kit/设备，只做严格静态审计，原门槛/修复入口/受影响回归交R34/R19D4B1-P。本轮不运行可见窗口/正常VSYNC/物理输入，旧四套图像门禁保持disabled，非像素覆盖闭包及MSAA/resolve仍B2，尺度与BLUR-01仍C，完整产品性能仍R28/R34。生成图、逐状态JSON、日志与机器矩阵不入Git。

共享Release完整隐藏回归首轮39/40：`rhi-opaque-gpu`记录foregroundUnchanged=false，initial/current前台PID不同且均不是验证PID，同时native visible/active、activationObserved/focusObserved均false。原因未确认，不能归因于产品主动激活或用户操作，也不能写为完整40/40；原报告保留`verification/macos/rhi-smaa/r19d4b1/full-foreground-change.log`。最终相同渲染行为下单独复核opaque与SMAA 2/2通过，其他39项有效结果复用，包含1200帧生产光学及原AO/阴影/二维/资源回归，不放宽窗口策略。两组官方qmllint与check-format通过。

最终恢复普通Release `YALAHOO_REVIEW=OFF`、`YALAHOO_MACOS_VISIBLE_TESTS=OFF`，生产完整构建、25/25快速CTest（含source-format/默认隐藏策略）与两组官方qmllint/check-format通过。Git只保留验证源码、Review只读访问及必要契约；本轮代码规模约550行（新增/删除合计，不含审计生成脚本与文档），未实施B2或C。
