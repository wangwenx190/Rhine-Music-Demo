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
| R05 | `ec0af92`；`d03527e`、`ed5aaef` | `FrameClock`、阻尼/五次轨迹及改速/减少动态；待 R19B。 |
| R06 | `5102cb0`；`5b2df80`、`e98c798` | GLB 坐标/UV 的资产事实由 R19A 确认；`LayoutMetrics`、投影/拾取与 geometryOnly 像素保护待 R19B/D。 |
| R07 | `1556a55`；`52f1278`、`5625d73`、`ed5aaef` | `ShellMaterial`、主盖/侧脊/纸面、校准来源与有效 override 待 R19C；不能因历史 ROI 通过保留数值。 |
| R08 | `178ca45`；`52f1278`、`5625d73`、`e98c798` | `MusicLighting`、Room/PMREM、灯光方向/单位和平台导出差异待 R19C。 |
| R09 | `3fc6a39`；`1f2352a`、`75775b3` | shadow/SSAO/depth/normal、偏置与核/滤波待 R19D。 |
| R09B | `522a3f3`；`1f2352a`、`75775b3`、`e98c798` | Bokeh/SMAA/MSAA、曝光/ACES/sRGB 与目标尺度待 R19D。 |
| R10 | `d06e5f3` | Quick3D/RHI 能力探测和历史选型理由待 R19D；不恢复旧图像分数门禁。 |
| R10A | `52f1278`；`5b2df80`、`d03527e` | opaque/纸面/阴影的材质和 RHI 适配待 R19C，资源图/采样待 R19D。 |
| R10B | `5625d73`；`d03527e`、`e98c798` | 透明壳体捕获/排序与材质待 R19C；MSAA、平台附件、窗口生命周期待 R19D。 |
| R10C | `1f2352a`；`d03527e`、`f7e423a` | 图集租约事实由 R19A 确认；capture/mips/postprocess/render graph 待 R19D。 |
| R10D | `75775b3`；`b7f73d4`、`e98c798` | `SceneItem`/RHI 产品提交、目标/资源回归待 R19D；历史性能只记录的用户边界保留。 |
| R11 | `f7e423a`；`b7f73d4`、`ed5aaef` | `CoverCache`/`AtlasAllocator`/`SelectedCoverPool`/`AtlasUploadQueue`；尺寸、颜色、所有权与有界工作有效；修正缺图脚注字重。R19A 审查；最终采样/合成待 R19C/D。 |
| R12 | `b7f73d4`；`0d06b7b`、`e98c798`、`ed5aaef` | 432 实例、波浪/rebase、选中/归位、合并图集；待 R19B，透明与后处理联动 R19C/D。 |
| R13 | `0d06b7b`；`2ee2294`、`1c9b8c6` | 真实列/填充、列记忆、库替换与导航目标；待 R19B。 |
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
| R19B | `SceneReview`、archive/real-column/navigation Review 中的图像附带路径 | x/v/a、矩阵、投影、拾取、边界、两模式/改速/减少动态及资源 |
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
