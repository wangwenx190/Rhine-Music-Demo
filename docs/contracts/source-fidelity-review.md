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
| R09 | `3fc6a39`；`1f2352a`、`75775b3` | shadow/SSAO/depth/normal、偏置与核/滤波待 R19D。 |
| R09B | `522a3f3`；`1f2352a`、`75775b3`、`e98c798` | Bokeh/SMAA/MSAA、曝光/ACES/sRGB 与目标尺度待 R19D。 |
| R10 | `d06e5f3` | Quick3D/RHI 能力探测和历史选型理由待 R19D；不恢复旧图像分数门禁。 |
| R10A | `52f1278`；`5b2df80`、`d03527e` | opaque/纸面的入射参数与toneMapped接口由R19C1复查；最终shader语义归R19C2，阴影/资源图/采样归R19D。 |
| R10B | `5625d73`；`d03527e`、`e98c798` | 材质参数迁移及R18独立clarity由R19C1复查；透明捕获/排序和完整R10B归R19C2，MSAA/平台附件归R19D。 |
| R10C | `1f2352a`；`d03527e`、`f7e423a` | 图集租约事实由 R19A 确认；capture/mips/postprocess/render graph 待 R19D。 |
| R10D | `75775b3`；`b7f73d4`、`e98c798` | `SceneItem`/RHI 产品提交、目标/资源回归待 R19D；历史性能只记录的用户边界保留。 |
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

已确认的跨轮差异：冻结scene.ts真实生产Fog按rendered camera distance及detail更新near/far，材质fog=true；当前原生SceneFrame/RhiItem/shader无fog输入或合成。近距B1 fixture在源fog前，不能关闭该缺口。具体参数、生产调用和Three颜色/雾次序已写入R19D明确修复任务，保持R20前主线，不当作BLUR-01正常设计或将缺陷移到R34。目标D3D11/OpenGL、原生窗口与设备实机边界见R34/R19C2B1-P。

普通Release最终恢复Review OFF、可见测试OFF，构建与25/25快速CTest（含source-format/隐藏窗口策略）通过。生成器默认写入+Qt格式化后纸面shader与提交版本逐字一致，未改变已验证的C++/shader语义，因此复用上面的33项完整结果；PowerShell入口当前主机仅静态核对，目标解析/格式要求仍在R34。所有运行输出保持忽略，只有源码/工具和必要契约进入本轮提交。
