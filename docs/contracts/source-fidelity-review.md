# 历史源码复查索引与判定

本文只保存历史轮次/提交→实际对象→适配或补偿判定；具体接口、算法参数、组件检查入口各归迁移计划职责索引中的组件契约。未完成任务/平台回归/BLUR与FLICKER退出条件只在迁移计划维护；运行数据、实验过程和成绩不在此追加。

冻结身份及Git映射由web-baseline.md维护。表中R19编号表示复查责任归属，不能仅凭索引行判为通过；当前结论须读对应专题，R17展示由R19G2确认、R19H核对全量提交和实际子轮次，完整索引见末节。

## R19A 范围与来源

技术规范与检查入口：[web-baseline.md](web-baseline.md)。

| 原轮次 | 引入及后续历史 | 审查对象与责任归属 |
| --- | --- | --- |
| R01 | `a48211c`；`21ef751`、`c7e9b61`、`e98c798` | `source-map.json`、`export-web-baseline.mjs`、`review-hook.txt`、冻结映射；源码清单/隔离匿名输入有效，旧 GPU/图像采集不再约束基础检查。R19A 审查，组件参数继续下列各轮。 |
| R02 | `24b125a`；`6740abc`、`c7e9b61`、`e98c798`、`1c9b8c6` | `QtKit.cmake`、`Cxx23.cmake`、`ApplicationIdentity.h`、模块/qsb/presets；最新原生 Kit、强制 C++23、同 Kit 工具与默认隐藏策略保留。R19A 审查；渲染 API 适配归 R19D，文字私有 API 归 R19F。 |
| R03 | `83f933c`；`5b2df80`、`6740abc`、`c7e9b61` | `PrepareAssets`、`GeometryAudit`、`AssetCatalog` 与资源清单；原始资产/字体表/模型几何未发现图像驱动变形；补齐 POSIX/直接调用的写入前来源校验。R19A 审查。 |
| R03B | `598c4a8`；`c7e9b61` | 真 SVG 主/小稿、揭示映射、平台容器；原创品牌与小尺寸光学校准来自用户明确要求，保留。衍生资产 hash/容器检查有效，R19A 审查；页眉/开场容器归 R19E/R20。 |
| R04 | `739ee97`；`d03527e`、`b7f73d4`、`0d06b7b`、`1a082f2`、`2ee2294`、`2abb4f6`、`33e8260`、`1c9b8c6` | `MusicTypes`、`LibraryModels`、`LibrarySession`、`OfflineMetadataProvider`；保留本地 URL、字段/稳定 ID、排序/搜索、导航与播放隔离和异步离线结果。R19A 审查；布局/导航状态消费者归 R19B/E/G。 |
| R05 | `ec0af92`；`d03527e`、`ed5aaef` | `FrameClock`、阻尼/五次轨迹、改速/减少动态与 `cancelTravel`；R19B 已按原公式和实际调用顺序审查，结论见下节。 |
| R06 | `5102cb0`；`5b2df80`、`e98c798` | GLB 坐标/UV 的资产事实由 R19A 确认；`LayoutMetrics`、相机/投影/拾取由 R19B 审查；geometryOnly 像素保护属于设备分配适配，MSAA/resolve由R19D4B2确认，目标尺度由R19D4C按最终音乐宿主复查。 |
| R07 | `1556a55`；`52f1278`、`5625d73`、`ed5aaef` | `ShellMaterial`、主盖/侧脊参数和有效override由R19C1复查，ROI系数移除；shader/捕获/LOD剩余项归R19C2。 |
| R08 | `178ca45`；`52f1278`、`5625d73`、`e98c798` | `MusicLighting`的主题/单位/轨迹参数由R19C1复查；Room/PMREM/BRDF/方向的shader实现与平台采样归R19C2。 |
| R09 | `3fc6a39`；`1f2352a`、`75775b3` | SSAO/depth/normal、核/box与数据边由R19D1复查；阴影/偏置/滤波由R19D3A/B复查。 |
| R09B | `522a3f3`；`1f2352a`、`75775b3`、`e98c798` | Bokeh→R19D4A，SMAA→R19D4B1，MSAA/旧门禁由R19D4B2复查并完成覆盖映射，尺度由R19D4C复查；颜色→R19D2。 |
| R10 | `d06e5f3` | Quick3D/RHI 能力与历史选型由R19D4B2按可控阴影/全局排序/独立捕获边界确认；不恢复旧图像分数门禁。 |
| R10A | `52f1278`；`5b2df80`、`d03527e` | opaque/纸面的入射参数与toneMapped接口由R19C1复查；最终shader语义归R19C2，阴影/资源图/采样归R19D。 |
| R10B | `5625d73`；`d03527e`、`e98c798` | 材质参数迁移及R18独立clarity由R19C1复查；透明捕获/排序和完整R10B归R19C2，MSAA/平台附件归R19D。 |
| R10C | `1f2352a`；`d03527e`、`f7e423a` | 图集租约由R19A确认，光学capture/mips由R19C2B/C复查，AO及常量数据由R19D1复查；MSAA/旧门禁由D4B2确认，尺度由D4C复查。 |
| R10D | `75775b3`；`b7f73d4`、`e98c798` | `SceneItem`透明重叠由R19C2C复查，标量AO/读回由R19D1复查；MSAA/组合/旧门禁由D4B2确认，目标尺度由D4C复查，历史性能只记录的用户边界保留。 |
| R11 | `f7e423a`；`b7f73d4`、`ed5aaef` | `CoverCache`/`AtlasAllocator`/`SelectedCoverPool`/`AtlasUploadQueue`；尺寸、颜色、所有权与有界工作有效；修正缺图脚注字重。R19A 审查；最终采样/合成由 R19C/D 专题确认。 |
| R12 | `b7f73d4`；`0d06b7b`、`e98c798`、`ed5aaef` | 432 实例、波浪/rebase、选中/归位坐标与稳定物理格 ID 由 R19B 审查；合并图集来源见 R19A，透明与后处理由 R19C/D 专题确认。 |
| R13 | `0d06b7b`；`2ee2294`、`1c9b8c6` | 真实列/填充、列记忆、库替换与导航目标由 R19B 审查；R19 的总览插值消费者由R19G1确认。 |
| R14 | `1a082f2`；`2abb4f6`、`e98c798` | R19E已逐对象复查品牌容器/控件/运输信息、遮罩及Canvas合成；移除四倍图标采样，恢复最终竖屏歌名cap。 |
| R15 | `2ee2294`；`2abb4f6` | R19E已复查滑尺/数字轨迹和Pointer/InputRouter；保留源数学与有界所有权，移除全部浏览器图像门禁。 |
| R16 | `2abb4f6` | R19F确认整行shaping/分数缩放/源拟合，移除无独立必要性的两倍DPR槽层及图像门禁。 |
| R16B1 | `6636ddb` | R19F保留完整字形描述符复用、隐藏停止和polish后页眉测量；inkEnd同时截文字与advance。 |
| R16B2 | `509afe8` | R19F保留Canvas有效可见性/窗口断连和三次成本采样；候选逐面重shaping的语义限制明确。 |
| R16B2V | `07514a5` | R19F以QTextLine实际混合ascent确认主字体基线修正；恢复标题尺寸保留，撤回的采样不恢复。 |
| R16B2V-P | 任务见 `07514a5`；未找到独立完成提交 | Windows 未收口像素任务已被后续契约替代；共享字体/基线代码由 R19F 追踪，不恢复原图门禁。 |
| R16B2W | 任务见 `509afe8`；未找到独立完成提交 | 没有独立生产选型；已有共享 DPR/生命周期和工具改动归 R19F，D3D11 目标实机仍归 R34。 |
| R16B3 | 任务见 `2abb4f6`；未找到独立完成提交 | Windows OpenGL 未独立完成；共享 GL 适配及候选代码归 R19F/D，目标实机仍归 R34。 |
| R16M1 | `c7e9b61`；前置 `6740abc` | R19F保留RenderControl真实target DPR、Review隐藏状态消费和字体head身份；不外推屏幕呈现。 |
| R16M2 | `b9a5ea7` | R19F以源PostScript面与实际run head/advance确认AppleColorEmoji公开选择；缺字体回滚，保留。 |
| R16M2P | `9470126` | R19F用DOM rangeTop与Canvas字体框独立核对主基线至1/64逻辑px，保留macOS floor；不依赖旧分数。 |
| R16M3 | `2840110` | R19F保留GL4.1/CoreProfile、宽度changed-only、对象所属合并finish及恢复取消；离屏保守资源估算保留。 |
| R17 | `33e8260`；`e98c798`、`ed5aaef` | R19G2复核ListView模型/复用/顶部定位和详情展示；修正最终CSS几何、sticky tab/滚动、曲目字体框与本地资料，保留状态/所有权适配。 |
| R18 | `ed5aaef` | 详情抽取/旋转/归位、副本 clarity、封面 ready 身份和完整根窗口重建；运动/所有权来源由R19G1确认。 |
| R19 | `1c9b8c6` | 总览相机/居中/折叠/标签及两步进入；R19G1确认，产品渲染质量联动 R19C/D。 |

非编号提交归属：cedd2a4/835f0b4归档/品牌→R19A；5b2df80格式→各组件语义；d03527e性能→R19A数据封面、R19C/D渲染；21ef751/391decc来源/产物边界→R19A；8dcd4f6 Canvas计划→R19E/F实际代码；6740abc/b08e94e平台接续→M轮及R19D/F；e98c798跨组件→R19B/D/E/F/G。R19H核对所有原轮次/子轮次及后续提交，不以该索引代替审查。

## R19A 资产与数据判定

技术规范与检查入口：[assets.md](assets.md)。

| 路径/符号、来源与有效调用 | 原理、适配与结论 |
| --- | --- |
| `PrepareAssets::verifyFrozenInputs`；R03 `add/copy` 前 | 旧 PowerShell 在外层检查冻结输入，C++/POSIX 直接入口没有同等约束。现在读取冻结映射，同时比对 Git blob、归档 bytes 和 manifest 的 24 个输入；冲突/缺失在任何资源写入前失败。`--verify-inputs` 无生成写入；GUI 构造前统一非激活策略。 |
| `music-case-v040.glb` → `GeometryAudit::compareGeometry` → qrc/生产 GLB | 三壳体 1000 三角形；保留属性/绕序、禁 LOD/全局缩放/优化。Qt mesh V 翻转为 glTF V 的补坐标适配，非截图调参。生产 RHI 从原 GLB 提取几何，导入 QML 材质不是有效产品材质。 |
| WOFF2 → SFNT → `initializeAssetFonts`/`assetFont` | DirectWrite 只解容器；原 WOFF2 和 OTF、OS/2/CFF 与 CSS 300/400/600/700 映射明确。CoreText 命名字重允许不同，按物理字形表选择；没有替换字体或按截图修改轮廓。 |
| `music-app.ts::svg`/`music.css` → 功能 SVG | 八个生产路径、24 viewBox、1.5 描边及 round 端点，昼夜颜色从实际 CSS 提取；属于源码设计。原创品牌小稿的光学改绘是用户允许的品牌差异，不能外推给原模型或封面。 |
| `CoverCache::decode/paint` ← `cover-atlas.ts::paintCover/containCover` | worker 的 autoTransform 和有效 ICC→sRGB 是图像解释适配；256/1024 仅缩大图、contain、1/64 留边、透明 RGBA premultiplied 与逻辑 1024 缺图画布有源码依据。原首行 500、脚注默认 400；旧 C++ 脚注继承 Medium，现重设 Normal。没有锐化、按截图扩大封面或预放大小图。 |
| `AtlasAllocator::publish/uploads` → `AtlasUploadQueue::expect/enqueue/take` | generation/identity/lease/serial 先验证再共享/复制；上传预算来自冻结 `flushUploads`。小设备分页及生产合并图集是存储适配；ready 在相同命令提交后有效。 |
| `SceneItem::updatePolish`/`SelectedCoverPool` → `RhiItem` | selected/returning 独立 lease、不可变像素与低清到高清升级；生产无需整页重绘。atlas/cover 无 mip、线性 clamp；opaque shader 在颜色转换前除 alpha，避免把 premultiplied 当 straight。 |
| `MusicTypes`/`LibraryModels` ← `music-types/order/search.ts`/`data.ts` | 本地 QUrl/媒体 Unknown、未知字段保留属于原生离线接口；QCollator 后端与 Web ICU 可不同，匿名视觉列顺序固定。规范化键与 ID tie-break、列索引、搜索索引不改变字段/顺序；搜索不改变播放。 |
| `OfflineMetadataProvider::request`/`LibrarySession` 的 queued 结果过滤 | 对请求捕获 const 快照，QObject 上下文事件投递，token/generation/kind 过滤；无网络对象/请求、无缓存写入。缓存旧状态原样返回，无缓存 Deferred。数据/Provider 不因外观复查改变。 |

## 输入与旧图像门禁归属

| 后续轮次 | 必须分离的现存入口 | 保留的有效检查 |
| --- | --- | --- |
| R19B | `SceneReview` 仅按自身渲染检查模型存在，不比较来源图；`ArchivePoolAudit` 填充/真实列已移除全部 SSIM/RGB 比较和替代矩阵图 | x/v/a、矩阵、投影、拾取、边界、两模式/改速/减少动态、稳定身份、实际 fallback 区域及资源 |
| R19C | `verify-r07.ps1`/`verify-r08.ps1`、`calibrate-materials.py`、`RhiShellAudit`/`RhiAudit` 的原图/ROI/环境 witness | 源公式、有效 override、真实格式/通道/透明合成、双主题质量和资源 |
| R19D | `postprocess-audit`/`occlusion-audit`、`RenderBackendAudit`、`RhiPostprocessAudit`、`prepare-*-capture.py` 与 render/RHI witness 耦合 | 真实附件/采样/resolve、shader 执行、实际质量、上传、生命周期与局部成本 |
| R19E | `compare-browse-ui.ps1`、`verify-r14.ps1`/`verify-r15.ps1`、`UiAudit`/`RulerAudit` 的旧图门槛 | 控件几何/状态/交互、Canvas 隐藏工作、数字轨迹及合成质量 |
| R19F | `verify-title.mjs`/`TitleAudit`、`verify-r16.ps1`/`audit-title.mjs` 的旧 SSIM/RGB/边缘带及两倍DPR槽层 | 字体物理身份、整行 shaping/布局、时序/宽度、真实清晰度、隐藏绘制/生命周期与成本 |
| R19G | presentation/detail/overview Review 及导出器中的原图/ROI 残留 | 展示布局/虚拟化、场景轨迹、相机/投影/状态、资源及实际质量 |

## R19B 运动、相机与阵列复查

技术规范与检查入口：[archive-pool.md](archive-pool.md)。

| 原轮次 / 生产源及原生符号 | 依据、判定与适配边界 |
| --- | --- |
| R05：`motion.ts::damp`、`MusicArchiveMotion`、`MusicRealColumnScalar` → `ScalarSpring`、`ArchiveMotion`、`ColumnScalar` | 保持解析临界阻尼、五次 x/v/a、1.4–2.4 秒时长、observe/retarget/translate/finish 顺序；C++ 保存精确加速度。同目标 archive 保护是计划明确的固定差异，不由截图决定。R18 `cancelTravel` 只在详情内部从原显示导数交给 rate=9，不重启 browse 轨迹。 |
| R05：`scene.ts` 帧循环 / `music-boot-timing.ts` → `FrameClock`、`App.qml::FrameAnimation` | 生产只有一个帧源，dt 钳制后只乘一次速度；开场时间独立，恢复丢弃首次间隔，减少动态执行完成。没有因截图调参引入的额外时钟或媒体依赖。 |
| R06：`viewport-layout.ts::viewportLayout/archiveFraming`、`scene.ts` 音乐相机目标块、`MusicCameraMotion` → `LayoutMetrics`、`SceneAdapter::target/advance` | 1.05 严格分界、1100/coarse 短屏、音乐 `.25` detailX、560/1080 detailY、preview `.36`、59°/25°→8°/20°、140→72、7.33→5.9 及固定检查槽均是有效生产值。轨道在 navigation 与 detail 插值各衰减一次，是原调用链语义。QMatrix4x4 float 是 Qt 交付适配；canonical GL z 仅用于投影/拾取，GPU 使用 QRhi clip 修正。无额外取景补偿。 |
| R06：`music-model.ts` / `scene.ts::model.rotation` → `SelectionController::transform` / GLB | 模型高度 3.35、底边 .175、厚度 .14 与导入资产一致；音乐 inspection lift=3.47，4.05 只为通用 primitive 压力测试。XYZ 旋转用 double 展开再转 float，独立低 lift/同时 pitch+yaw 输入证明必要顺序，不回退为误差更大的 quaternion 乘法。geometryOnly 目标保护保持原请求精度并显式报告 limited，不是截图缩放。 |
| R12：`archive-loop.ts` / `scene.ts::field` → `ArchivePool`、`SelectionController::field/rebase/publish` | 9×48 顺序、半周期 floor、5.2/.62 间距、最多六个 3.2 秒脉冲、.24 clamp、2.5 秒 idle 与波浪原点均来自源码。按 selected 相对原点求位移、静止八 ulp 算术残差界和跨 rebase 的有限物理格 ID 是精度/身份适配；不改导数/几何/透明参数。八 ulp 仅覆盖平移、乘法和相消的有界舍入，不是可见像素偏移。 |
| R13：`MusicRealColumns`、`scene.ts::musicColumnRowOffset/musicRealColumnField`、`music-app.ts` 列记忆 → `RealColumns`、`ColumnMemory`、`SelectionController` | 保留真实行 12…11+count、独立中点/列活动度、五次归中及折返导数、目标实际显示深度、记忆开关无播放副作用。按时间惰性取样是解析算法调度适配，避免每帧遍历全部列；减少动态 epoch 同步未观察列。R19 overview插值消费者已由R19G1沿同一progress/相机/投影复查，普通浏览结果不替代总览门禁。 |

## R19C1 材质与光场参数复查

技术规范与检查入口：[materials.md](materials.md)。

| 原轮次 / 源生产路径 → 当前符号 | 判定与历史依据 |
| --- | --- |
| R07：`scene.ts::configureMusicGlass → CardAppearance.register/setTheme → setMusicGlassTheme/setMusicGlassClarity` → `GlassMaterial.qml`、`ShellMaterial` | 三表面transport、coat、线性tint/attenuation及IOR=1.46均有冻结源码。R07在`1556a55`通过ROI坐标搜索引入environment/roughness=1.01、thickness=.91809、distance=1.0201；没有灯强单位或BRDF接口依据，现全部移除。主盖clarity独立，侧脊/后托保持原finish。 |
| R08：`scene.ts::setTheme`与`MusicSelectionLighting.setTheme/update` → `LightingPalette::theme`、`SelectionLighting`、`RowLightController` | 音乐有效环境.25/.16覆盖隔离R07的.48/.68；Three在material.envMap为空时以scene.environmentIntensity更新uniform，不额外乘configure的.65。key .45/.30、hemi .32/.21、fill .045、spot 180/130×64、area 34/24与曝光1.05/1.08保留；GLSL沿用源单位，没有Qt内置brightness换算。650ms曲线、double follow/同一FrameClock与相机局部offset保持。 |
| R10A：`createAlbumPrintMaterial`及`MusicSelectionLighting.shadePrint` → `opaque.frag/paper-lighting.glsl`的参数接口 | 纸面alphaTest=.025、Lambert无specular/emission、先限制source再乘fieldExposure、最终toneMapped=false及线性入射接口有源码依据；qrc/GLB/UV/原生clip适配保留。当前参数变更不改纸面shader、颜色转换、附件或采样。 |
| R10B参数迁移：`5625d73::Renderer::uniformTemplate`、R18 `ed5aaef::Renderer::uniform` | 两处把R07 ROI系数再次施加到实际uniform：基础transport/coat及逐实体lid roughness。现在transport/coat直接复制；lid按实体clarity计算后直接提交，其余缓存和材质不变。原R10B manifest及旧审计脚本的fixedOptics只描述历史实现，不能覆盖当前源码。 |

## R19C2A 材质/灯光 Review 与原图门禁拆耦

技术规范与检查入口：[materials.md](materials.md)。

| 原轮次 / 实际审查对象 | 源码依据与结论 |
| --- | --- |
| R07：MaterialReview、verify-r07的90状态、9纸面探针、标定与ROI门禁 | 投影/三个表面参数有源定义；R07用浏览器像素作纸面期望并计算ROI/SSIM，无独立数学必要性。删除图像读取/分数/diff及标定调度；源公式改用均匀测试墨色推导线性入射/限制/编码，3码值容差保留。 |
| R08：MaterialReview的48灯光状态、verify-r08的场核与图像混合条件 | 保留实际光场模式、detail、主题、相机与源表面参数；删除五ROI的6/.97及固定D3D11 API编号假设。924轨迹/392场核继续由LightingTest验证，脚本只等待实际CTest/进程退出。 |
| R09：MaterialReview的AO/阴影诊断调用与目标尺寸断言 | 共用驱动不再加载原图；保留channel、shadows、near/far、aoRadius与实际effectiveTextureSize检查。原occlusion GPU evaluator仍标旧门禁disabled。 |
| R09B：MaterialReview的Bokeh/SMAA/AA与focus/aperture断言 | 保留有效质量设置、目标大小、focus/aperture参数检查；移除共享ROI/full-image比较。未改任何核/曝光/采样。 |
| R10：MaterialReview的instance/atlas probe路径与颜色输出 | Probe的实例、深度排序、atlas phase及字节元数据保留；报告不再输出edgeDistance/ROI。原render-backend GPU evaluator不自动启用。 |
| R10B：VerificationWindow接入、MaterialReview首次show与引擎所有权 | 新CLI在ApplicationWindow加载前分派；独立GeometryScene在macOS使用真正隐藏RenderControl，普通产品窗口及生产RHI不变。 |
| R19C1：拒绝factors/calibration与参数核对 | 两类输入仍在创建window/采集之前退出12；不把expected乘同一拟合系数，不放宽源参数容差。 |

## R19C2B1 不透明RHI源数学、资源与门禁复查

技术规范与检查入口：[rhi-opaque.md](rhi-opaque.md)。

| 原轮次 / 实际审查对象 | 源码依据、适配及判定 |
| --- | --- |
| R07：`createAlbumPrintMaterial`→`opaque.frag` | 源Lambert无specular/emission，alphaTest .025、toneMapped=false、先限制原图颜色。sRGBDecode、预乘滤波后恢复颜色、contain/inset与源3.06纸面几何保持；没有ROI系数。 |
| R08：`shadePrint`、锁定Three Lambert/hemisphere/IBL→`paper-lighting.glsl`与CPU原点场 | 对照生产shadePrint替换顺序、Three `lights_lambert_pars_fragment`、hemi及getIBLIrradiance：direct/hemi除PI，IBL自身PI与Lambert BRDF相消；print field最后乘原曝光。entityLight0.xy提升只依赖origin/frame，C1源场核和轨迹独立回归保留。 |
| R10A：`RhiAudit`的Quick3D/浏览器oracle、平面/法向、source shadow witness | 移除Quick3D实例化、原图载入/MAE及source shadow image比较；保留数学源位置/法向/深度、receiver已知strength。旧QML只留历史源码，不再链接或打包；旧verify-r10a历史流水线退役，当前独立CTest恢复。 |
| R10B：RhiItem异步capture的重复纹理与值交接 | 显式capture原13字段经fallback常多次指向同一texture，却重复GPU readback和imageOf转换。现在按真实纹理指针去重、每唯一texture转换一次，字段共享不可变QImage；最后一组真实回调才deliver。没有改变pass/格式/采样。 |
| R10C：RGBA16F/法线与RGB24深度RGBA8、资源清单 | 浮点线性颜色与独立normal/depth、hardware depth保持实际Kit支持；读回解码有限值、Y方向/字节序与原语义一致。capture附加实际对象inventory，未知driver内存仍null，不用描述估算冒充显存测量。 |
| R10D：生产SceneItem/RhiItem共享边界 | 只在显式capture分支增加inventory/去重诊断；request=0生产不执行这些JSON或readback。新的RhiCaptureHarness是工具，不改变生产帧源、选型或输入状态。 |
| R11：cover snapshot/lease及same-request revision | 保留按cover cacheKey/content revision共享/隔离一次，不把更新绑定capture request。独立driver以同request替换纯红/蓝，检查自己最终输出各自响应；不是两幅源图趋同。 |
| R12：阵列/后续优化中的批次与读回所有权 | 采集只访问有界实体和最多13个字段，实际读回组按指针有界合并；无全库遍历或无界队列。resize旧资源的最后使用由QRhi完成约束，结果只持有值。 |
| R13：有限列共享renderer采集 | 去重不改变实例ID/矩阵/封面区域、同深度键或真实列状态；generic字段通过Qt隐式共享保留相同语义图像。 |
| R18：详情/归位及renderer重建 | 不改owned材质或快照；同RhiItem跨十次实际invalidation重建，当前renderer的局部generation仅作描述，实际SG事件作为生命周期证据。 |
| R16 macOS可见补验：`e98c798`→Target MSAA fallback/有界drain | 实际GL能力探测选择同格式renderbuffer代替不可用multisample texture，不减样本数或颜色精度；readback按MaxAsyncReadbackFrames+1继续真实帧。保留必要API适配，不因旧图分数回退。 |

## R19C2B2 光学算法、捕获与有效门禁复查

技术规范与检查入口：[rhi-shell.md](rhi-shell.md)。

| 历史对象 / 当前调用链 | 源码判定及修改 |
| --- | --- |
| R07物理材质→case-glass / 四RHI shell变体 | 对照锁定Three BRDF_GGX_Multiscatter、DFG能量补偿、间接single/multi、diffuse守恒、clearcoat和反射pow4；系数/源材质不变。修复geometryRoughness：Three nonPerturbedNormal为view-space，max分量导数不具旋转不变性，必须先乘VIEW_MATRIX。 |
| R08音乐光场→direct/LTC | saturate点积与源一致；零入射先退出，避免相反light/view的零半向量。LTC正视时切向量为零，只有此奇点选择最不平行坐标轴构造有限正交基，其余算法不变；不称其为已确认阵列闪烁根因。 |
| R07/R10B环境、透射、磨砂与色彩 | PMREM源生成器实际HalfFloatType，原生RGBA16F保持源类型；资产RGBA32F是序列化展开，不要求GPU字节相同。保留CubeUV方向、LTC查表、bicubic、IOR1.46及roughness×.92、局部zone .16、Beer、ACES/sRGB和paper toneMapped=false。 |
| R10B capture/resolve/壳体、R10C硬件depth适配 | 从实际QRhi对象检查采样binding4恰为opaque resolve和mip sampler、composite独立、同hardware depth且PreserveDepthStencilContents。真实命令记录opaque→resolve→mips→copy→visibility→shading；visibility写depth/LessOrEqual，颜色Equal只读、Back cull、无blend。 |
| R10B透明封面、R10D MSAA | 保留contain/inset、centroid、预乘滤波恢复、alphaTest .025；无模糊/阈值/透明度调参。 |
| R11–13实例/封面、R18生命周期、macOS后补适配 | 新诊断只在显式capture读取已提交有界uniform及真实pipeline/RT值；Review探针用保留paperSize.w，生产拒绝selector且不打包probe。最多2批pending，原读回去重/别名共享保持。 |

## R19C2C 透明重叠与位置不变性

| 历史对象 | 判定 |
| --- | --- |
| visibility→Equal颜色pass及geometry两个顶点变体 | 位置运算顺序与跨pipeline不变性必须同时满足；统一model→world→nativeVP并声明invariant，仅两个顶点使用MSL2.1，保留mvp ABI槽。未改源数量/透明度/动效或增加depth epsilon。 |
| R10D/R12/R13生产消费者 | 连续正常光学与覆盖探针沿真实SceneItem检查；不能用孤立单盒或零warnings关闭重叠问题。组件规范及入口归[rhi-shell.md](rhi-shell.md)。 |

## R19D1 SSAO 算法、标量读回与常量数据复查

技术规范与检查入口：[occlusion.md](occlusion.md)。

| 历史对象 / 实际调用 | 源码依据与处理 |
| --- | --- |
| R09：scene构造→enableSelectionLighting→softenMusicContactShadows→setQuality再次soften | 实际半径.18、normalized orthographic depth min/max .001/.035、.22乘数；构造.38/.44与.09不覆盖最终音乐入口。完整壳体及纸面normal override保持，不受纸面透明contain裁切。 |
| R09：锁定Three SSAOShader、SSAOBlurShader→共享ssao及三RHI展开变体 | 源只unpackRGBToNormal，删除额外normalize；源depth==1哨兵改为存储RGB24三通道精确white判断，删除扩大的1e-6范围，并避免背景normal多余fetch。保留透视标量重建、view normal、scalar noise复制、TBN、全部样本与严格delta判定、5×5 box及.22合成。 |
| R09/R10C：kernel/noise输入与固定索引 | kernel/noise由锁定SSAOPass/SimplexNoise生成；确定性数学输入核对有效，不能扩展成GPU byte witness。RHI半径basis提升与固定索引展开保留，原累加顺序/样本数量不变。 |
| R10D：RGBA16F→标量R16F及诊断解码 | R16F生产raw/blur数学精度和消费者.r等价性保留。查明此前OpenGL错误发生在读回边：本机GL_PACK_ALIGNMENT=4，奇数宽R16F行需填充；当前Qt GL readPixels按紧密纹理字节数分配，未修改PACK_ALIGNMENT，可能越过逻辑缓冲边界。非有限字节不证明产品AO shader生成NaN。 |
| R10C/D及后续capture：行跨度、资源/别名 | 显式捕获且仅未对齐的GL R16F，通过QRhi同格式copy到缓存的补齐宽纹理；原区域逐值保留。按实际storage stride解码且只消费逻辑区域，未初始化尾列不扫描。最多2个AO缓存槽，实际清单含capture-aligned-scalar；同尺寸复用、renderer reset清理。只查询GL状态，不全局改pixel-store，不锁定旧Qt。 |
| R10C/D：PostUniform及重复kernel上传 | RHI已把samplesMap改为uniform索引，却仍分配/上传未读取的kernel texture；每个pass还复制/上传整个1104-byte块。删除闲置纹理及资源计划条目，kernel只在SSAO pass创建时一次上传；每帧只更新80-byte变化前缀，保留1104-byte std140 ABI及kernel offset80。 |
| macOS GL能力、窗口及目标平台 | 保留原MSAA等价renderbuffer及真实标准透视/行方向。bake-occlusion-shaders改用实际host的qsb路径/输出，Windows脚本未在本机执行。前台PID误报的源码判定及监测修正见R19D4B2；原生窗口/目标实机要求仍归R34，不将本机结果外推目标平台。 |

## R19D2 生产雾、颜色域及捕获次序

技术规范与检查入口：[postprocess.md](postprocess.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| R07 MeshPhysical/meshlambert；WebGLRenderer::setProgram | 普通render target关闭逐材质tone mapping，故源fog应在线性域；独立材质Review的本地ACES不能进入生产compositor。 |
| R08 createAlbumPrintMaterial / WebGLMaterials::refreshFogUniforms | 纸面toneMapped=false仍启用fog；音乐入口主题曝光覆盖隔离材质默认值，保留该有效调用。 |
| R09B EffectComposer / OutputPass；R10C Postprocess | 最终颜色转换只执行一次；不增添gamma、白雾或截图补偿。 |
| R10A opaque；R10B renderTransmissionPass | 补齐原生遗漏的雾。capture表面与壳体表面各自加雾属于源语义，不因捕获已经有雾而删掉壳体雾。 |
| R10D SceneAdapter::publish → SceneFrame → RhiItem | 按实际渲染相机计算雾范围，并补齐detail-only失效；不采用目标相机或构造期常量。 |
| 5b2df80 / e98c798 / R19C1 | 格式整理、必要GL多采样适配及无源材质系数的移除均保留。 |

## R19D3A 阴影投射、normalBias 与 PCF

技术规范与检查入口：[rhi-opaque.md](rhi-opaque.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| R09 3fc6a39：GeometryScene DirectionalLight | Quick3D公开hook的近似只供独立Review，已由R10D生产RHI替换，不作为源等价阴影。 |
| R10A 52f1278：caster / receiver / shadowmap_vertex | 源shadow pass把FrontSide材质转为BackSide，支持原生Front cull；修正normalBias由片元重算法向投影为源顶点偏移后插值。 |
| R10B 5625d73：shared shadow-filter / Lambert / physical | 源只衰减key分量，不能压暗环境或整幅透射；有效音乐入口强度覆盖默认值，无图像拟合依据。 |
| R10C 1f2352a：sampler2DShadow / R32F+D32F | 锁定Three运行时将PCFSoft转为PCF；保留硬件比较及canonical诊断适配，不退回旧手动滤波。 |
| R10D 75775b3：SceneItem / 缓存；后续PERF与e98c798 | 保留离屏caster、按真实依赖失效和canonical/raster坐标分离；平台翻转与GL能力适配不构成新增偏置或效果补偿。 |

## R19D3B 产品地面与接收闭包

技术规范与检查入口：[rhi-opaque.md](rhi-opaque.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| R09/R10A scene构造 / 接收阴影 | 补回源产品地面；原诊断receiver不是产品替代物，两者保持独立。 |
| R07/R08/R19D2 主题 / Standard / fog | 地面采用源Standard材质及主题曲线，不能套用纸面限亮或壳体场核；dusk仅为已有源色检查，不新增产品主题。 |
| R09B/R10C/D normal/depth / capture | 地面必须覆盖AO及完整Bokeh几何，并出现在供壳体采样的不透明捕获中。 |
| 后续PERF / Metal / OpenGL | 复用既有opaque管线、光学纹理和receiver槽位；新增仅为固定地面几何，不引入新生产目标或队列。 |

## R19D4A Bokeh 源码与真实生产采样

技术规范与检查入口：[postprocess.md](postprocess.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| scene.ts → SceneAdapter → SceneItem | focus/aperture采用每帧实际相机与detail公式，构造值不能覆盖有效帧参数。 |
| R09B公共Effect → R10C/D Postprocess | 保留锁定BokehShader的深度域、完整采样核、边界与过滤；数学期望直接由源输入推导，不采用浏览器GPU图或截图。 |
| Postprocess资源 / 上传 / Review | 复用兼容目标与变化前缀；组件reset/recreate只证明后处理资源生命周期，不等同SG或原生窗口恢复。源适配/历史入口为audit-rhi-bokeh.py。 |

## R19D4B1 SMAA 源码、查表与真正离屏门禁

技术规范与检查入口：[postprocess.md](postprocess.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| 冻结scene.ts / RenderQuality → SceneItem → Postprocess | 保留源启用条件及Bokeh→SMAA→Output次序；生产组合实际RT/PSO/SRB闭包由R19D4B2确认。 |
| SMAAShader edges / weights / blend；SMAAPass lookup | 保留源Medium、lookup行序/过滤、gamma及alpha语义；源float(ivec2)取首分量后广播，数学期望按真实GLSL展开。 |
| Postprocess资源 / 上传 / Review | 三个生产pass直接离屏执行源数学检查；Review中间目标访问不改变普通构建ABI，不新增产品读回或队列。源适配/历史入口为audit-rhi-smaa.py。 |

## R19D4B2 MSAA/resolve与旧门禁覆盖

技术规范与入口归[postprocess.md](postprocess.md)、[rhi-postprocess.md](rhi-postprocess.md)和[test-gates.md](test-gates.md)。以下按旧工具的实际断言映射；历史图像残差仍是历史事实，不改成通过或复跑。

| 历史对象 / 引入来源 | 源码判定及现行覆盖 |
| --- | --- |
| R09B `522a3f3`：GeometryScene MSAA/SSAA诊断、Bokeh/SMAA组合 | 冻结scene构造WebGLRenderer antialias=true，quality.antialias只控制SMAA。保留生产四样本及高/极高SMAA；旧SSAA为Review诊断，不能作为产品预设或与源图评分。三个固定Quick3D Effect的原生调度适配保留；生产组合以RHI显式资源图验证。 |
| R10 `d06e5f3`：后端能力选择 | Quick3D公开接口无法控制源shadow/跨实体透明表面全局排序/独立捕获resolve，RHI具明确控制边界，因此保留唯一RHI生产路径。旧残差是否非空、平均损失是否缩小和Quick3D/浏览器谁更接近不是选型或通过条件。 |
| R10C `1f2352a` / R10D `75775b3` / `b7f73d4`：附件与生产接线 | 保留RGBA16F颜色、RGBA8法线/RGB24距离、相同采样的depth、独立resolve与共享保留depth。实际RT/PSO/SRB逐项检查，不以设置值或名称证明。源算法不变；图集/数值更新不重建兼容目标。 |
| `e98c798`：macOS GL MSAA renderbuffer fallback | 仅当实际context缺多样本texture storage时使用同格式、同样本renderbuffer，resolve仍单样本。属于必要API适配，不降到RGBA8或少样本；真实GL检查读取backingFormat/样本/目标和解析部分覆盖，Metal仍使用texture。 |
| 默认隐藏策略的前台PID误报 | 初始/当前PID均为其他进程且无自身激活/焦点历史时，旧foregroundUnchanged仍阻断采集。沿AppKit自身通知增加NSWorkspace工作区激活历史监测；任一初始/当前PID属于验证进程、任一自身激活/焦点均继续失败。外部应用切换单列计数和原布尔值；回归覆盖未知PID、切入验证进程及切出后保留失败，不屏蔽自身短暂抢前台。 |
| `RhiPostprocessAudit`恢复后的可选租约采集 | 空primary lease是既有合法输入，诊断原先无条件索引导致越界。缺租约的metadata现在为null；带fallback必须有完整primary/fallback lease，提交边界先拒绝。该修正不改变生产采样/图集预算。 |

| 退役入口及原断言 | 独立非像素覆盖 / 明确边界 |
| --- | --- |
| `postprocess-audit`：105状态完整性、backend/DPR、focus/aperture、启用条件、目标尺寸/上限 | 不再依赖旧runtime/image文件完整性。`render-quality`核对源四预设、归一化、DPR/尺寸限制；`rhi-bokeh-gpu`核对实际核/参数，`rhi-flicker-gpu`核对SceneItem实际Bokeh上传及original/high的SMAA启用/组合图；`rhi-postprocess-gpu`覆盖三尺寸×1/4样本×八组合、单样本后处理/实际绑定/80-byte前缀/关闭释放。九尺寸与真实DPR/最终合成缩放由R19D4C的`rhi-scale-gpu`接续，输入保持`postprocess/visual*.json`和匿名库；具体规范归rhi-integration.md。 |
| `postprocess-audit`：23组AA off/MSAA/SSAA差异、至少六组边缘分数改善、Bokeh/SMAA changedPixels、background ROI、MAE/SSIM、已知残差白名单及放大strip | 源图评分/改善数/SSAA排名/残差白名单/放大拼图全部退役。`rhi-shell-gpu`以解析线性墨色、透明洞不写depth、原一码值黑边上界和四样本partial>100验证实际覆盖；`rhi-bokeh-gpu`/`rhi-smaa-gpu`用独立公式验证效果而非“像素有变化”。背景/最终色变换由`rhi-fog-gpu`的84源helper状态和`rhi-postprocess-gpu`实际最终合成的独立公式/原一码值上界核验，生产输出绑定/开关由实际graph/outputOptions核验。自身正常输出仅供真实质量检查。 |
| `occlusion-audit`：88旧文件/固定尺寸、normal/depth全图误差、单盒四ROI、SSIM、AO背景白/接触非白、阴影开关changedPixels | 文件数/截图误差/源GPU witness退役。`render-quality`维护尺寸保护；`rhi-ao-gpu`验证RGB24背景恒1、三核/倍率/奇偶尺寸、243接触点、5×5/.22和缓存/重建；`rhi-opaque-gpu`/`rhi-shell-gpu`验证解析投影/normal/depth；`rhi-shadow-gpu`验证源投射/偏置/PCF、实际接收阴影occluded>0和地面/AO/DOF。不是以背景全白掩盖无接触。 |
| `RenderBackendAudit`：70旧文件/hash/backend/DPR/frames、sourceParameterMatch、projection≤2、normal/depth、阴影开关、排序组、atlas替换/静止/归还 | 旧文件/GPU来源/hash完整性不再作为产品门槛。冻结输入来源由各源审计维护；`scene`/`rhi-opaque-gpu`/`rhi-shell-gpu`检查实际投影/通道/稳定排序，`rhi-shadow-gpu`检查阴影，`rhi-postprocess-gpu`保留atlas GPU区域/还原、上传、稳定复用和容量检查。目标API实际执行仍按R34，不接受仅字符串命中。 |
| `RenderBackendAudit`：残差必须非空、equalWeightLoss、五ROI/edgeDistance、浏览器变化与拼图 | 全部退役。后端能力的技术判定见上表；材料/光场、输出和连续透明质量分别归R19C/D各独立测试及真实输出，不据图调参数。 |
| `RhiPostprocessAudit`：50组最终图/Quick3D/浏览器平均损失、AO81/82/84源GPU oracle、效果changedPixels | 图像评分及GPU oracle删除。源SMAA/Bokeh/AO/颜色分别由独立公式门禁验证；新组合检查读取真正RT/PSO/SRB，核对resolve→AO→Bokeh→SMAA→一次output绑定，无反馈或跨帧旧目标。`rhi-flicker-gpu`覆盖完整SceneItem匿名9×64连续输入，不能用孤立三pass替代。 |
| `RhiPostprocessAudit`：cover副本、64tile初始化、432实例、1/7/8/31/32/33/64/432容量、全可见池、单tile/还原/逆序、resize、generation拒绝、九tile背压 | 原非像素断言保留到新`rhi-postprocess-gpu`，读回只比较自身可控RGBA数据/更新区域/稳定状态，拒绝浏览器图像oracle。D3D11另要求实际native transfer boxes/bytes；其他后端仅QRhi请求与自身读回。元数据明确未知实测显存为null。 |
| `RhiPostprocessAudit`：手动processEvents等待、profile/120秒、frameSwapped/CPU/GPU、record-only | 等待改为正常事件循环和有界实际帧回调；`--profile`保留三次独立局部分通道CPU提交诊断，不把RenderControl完成当物理呈现或GPU完成。原Windows18.35/20.98ms及record-only事实不变；完整产品120秒/VSYNC/设备测量由R28/R34原任务承担，未运行不标通过。旧`--record-performance`/`--diagnostics`不再接受。 |
| `prepare-{postprocess,occlusion}-capture.py`及其旧visual/render-backend导出器、verify-r09/r09b/r10/r10c | Fail-closed兼容入口在任何采集/改写之前拒绝执行；算法输入导出器`tools/assets/`保持。三套旧聚合C++入口只返回退役说明，CTest保持disabled；只恢复已独立的`rhi-postprocess-gpu`。 |

R19D4B2的MSAA/resolve和组合资源图检查本身不关闭VIS-ARRAY-BLUR-01。R19D4C的独立尺度判定见下节；原生同窗口hide/restore、物理输入/显示、正常VSYNC、最终产品性能与缺目标后端按既有R34专项和本轮目标移交执行。

## R19D4C 目标尺度、封面与景深单位

| 历史对象 / 生产来源 | 源码判定 |
| --- | --- |
| 冻结music-app → fit/scene.resize → resizeQuality；music.css最终override | `#stage.music-app`宽高100%、`transform:none`，实际host bounding/client比例为1。archive-main的参考舞台缩放属于另一入口，不能套入当前音乐产品。 |
| R09 `3fc6a39` RenderQuality；R09B `522a3f3` GeometryScene | RenderQuality的源DPR cap、scale归一化、floor与像素保护保留；直接使用显示逻辑尺寸的View3D移除重复参考舞台缩放。它仍是Review，不改变生产RHI选型。 |
| R10D `75775b3` SceneItem；`e98c798`诊断保护 | product以实际item尺寸再乘`min(width/1920,height/1080)`造成非预期降采样，小窗口最终放大。修正为源宿主比例1；geometryOnly的真实DPR及像素保护保留。960×640/DPR1原先请求480×320，源音乐入口应为960×640；此为调用链推导，非截图追分。原用户所见窗口未能追溯，复现明确使用新的匿名生产阵列入口。 |
| R10C `1f2352a`、R10D补丁`b7f73d4`、`e98c798` | 显式物理附件、MSAA/resolve、独立全尺寸DOF、传输捕获mip和有界资源所有权是必要原生适配，保留；没有引入整体倍采样、锐化或新模糊。窗口DPR变化重新提交，换窗口断开旧信号。 |
| 封面解码 → pool/atlas → opaque/shell | 保留256缩略图、1024选中独立租约、contain/留白与无跨格mip；linear/clamp采样及透射独立trilinear捕获各有用途。封面高清不由小窗口降级；源磨砂/粗糙度与传输LOD保留。 |
| R19D1 `4ab1e73`；D2 `2f50812`；D3A `f45024d`/`e9360b7`；D3B `f4e1610` | D1的三核/标量精度/常量前缀、D2线性雾/最终颜色次序、D3投射偏置/过滤和产品地面均继续使用既有生产资源图；尺寸修正只改变源公式应有的物理目标。逐项数学与实际附件规范引用对应组件，完整历史hash由audit-rhi-scale输出。 |
| D4A `704a99d`；D4B1 `5e333ec`；D4B2 `4e86136` | 保留view空间focus、完整RGB24 depth、nearest深度/linear颜色、41点UV CoC/.011与aspect；核本身不随窗口像素加倍。保留SMAA源核/lookup、4×几何与单样本resolve/后处理，不以关效果改善清晰度。 |
| 质量/资源/绘制路径选择 | RHI继续承担源全局透明排序/阴影与显式捕获，Quick3D只Review。新独立入口检查实际DPR/物理附件/最终合成和源数学，连续入口保留数量/动效/重叠与恢复；运行结果留本地，物理屏幕、正常VSYNC、完整产品性能及缺设备平台按原专项。 |


## R19E 浏览界面、滑尺与数字轨迹

规范归[browse-ui.md](browse-ui.md)、[navigation.md](navigation.md)、[canvas-painter.md](canvas-painter.md)。`tools/audit/audit-browse-navigation.py`读取冻结源及实际历史对象，输出逐符号/hash的COMMON/PERF/三平台矩阵；该脚本固定审查边界，运行报告与实际质量单列。

| 实际历史对象与调用链 | 来源与判定 |
| --- | --- |
| R14 `1a082f2` Header/品牌容器、ArchiveButton、ThemeSwitch | 源`music.css`及最后加载的`music-theme-switch.css`定义容器锚点、竖屏网格、17px SVG/圆端点、主题pill；新品牌图形由用户差异替换，容器位置保留。设置图标的两条线/两个圆用Curve Shape保留原矢量几何/层级，原生可访问焦点框是必要交互适配。其余SVG及R16新增play/pause四倍栅格+mip没有独立必要性，移除，按所属窗口实际DPR栅格化；动画scale保留。 |
| R14 BrowseInfo/ArchiveText/NavigationFrame/Footer、R15数字接入 | 源最终callout CSS覆盖早期57%定位，采用68.633333%与竖屏clamp；navigation高度驱动callout-bottom。保留CSS normal ascent/descent分别取整、16px父inline strut、1.7行高、眉题span边距/空格advance、按钮继承16px及Basic padding清零；这些是文字度量和盒模型适配，分数字号使用局部矢量scale而非增加缓存字号/离屏层。`ArchiveText`通用文字语义及标题专用基线进一步由R19F复查，不能由本节静态锚点关闭。 |
| R14 DesignTokens/ReadingVeil、R19 `1c9b8c6`界面连接 | 650ms颜色/.4,0,.2,1、380ms pill/.22,1,.36,1及源渐变停点有明确CSS依据。直接Quick渐变节点按原层级位于三维与页眉之间，无额外全屏采样；稳定透明层停绘。R19的overview opacity/offset/enabled只是状态委托，实际总览时序由R19G1复查。 |
| R16 `2abb4f6` Header/TransportTitle接入 | song slot增长推动前置控件而右侧play/stop不动，退出保宽由既有TransportMotion管理。该提交将竖屏cap误替换为横屏宽度断点，现恢复最后生产CSS的`clamp(0,width-320,280)`优先级；横屏760/610/340规则及间距保留。标题shaping、退字与采样归R19F。 |
| R15 `2ee2294` RulerController→RulerInk/Quick clip→固定Button池 | 12+2×2槽、anchor5、rate18、560ms ripple/24ms展开及完整刻线/边缘alpha均来自`music-ruler.ts`/CSS；nearest槽匹配与循环occurrence保留。三份actual-size路径按geometry失效，动画高度用rect而不拉伸AA fringe；透明单采样附件固定130×30/96×21，动画宽度仅改变Quick clip。Canvas2D原`antialias=1/DPR`只存在于图像对照路径，没有独立于旧图像门禁的必要性，移除；两API使用[Qt默认antialias=1](https://doc.qt.io/qt-6/qcanvaspainter.html#setAntialias)，DPR由所属目标处理，不取浏览器栅格损失为目标。 |
| R15 InputRouter→SelectionController→SceneFrame、OrdinalMotion→OrdinalReel | Qt像素/角度单位与Y反号、控件/祖先所有权和窗口内焦点是输入API适配；源48px/三张/200ms、36px/1.3/1400ms边界保留。最多一个可替换目标服务既有四封面背压，真实提交revision与新输入receipt分开。49点/460ms轨迹来自锁定rolling-number实际函数，保留重定向速度/方向/终点归一、十位/两面和唯一FrameClock；RenderControl专用offscreen开关接通数字/滑尺，普通隐藏窗口仍停进度。 |
| R14/R15 Review、`c7e9b61`隐藏适配、`e98c798`窗口适配 | 删除跨API/浏览器SSIM、RGB、边缘、coverage envelope及恢复图像分数，导出器仅交付最终CSS结构与冻结数学。保留自身纹理、revision、有内容、固定位置/键盘路由、静止/隐藏零paint、真实surface失效/恢复/释放及CPU成本。滑尺复用OffscreenSurface，单读回/有界续帧；不把离屏frameRendered当VSYNC或物理输入。PowerShell审计改为共同源码入口，产品GUI包装读取Process.ExitCode及报告。 |

Canvas Painter的采用依据为源矩形/路径表达、固定值同步、renderer所有权和三份路径缓存；API间像素相似度与旧benchmark成绩不参与选型。实际隐藏质量检查保留正常轮廓/裁切/渐变/文本/图标输出；物理屏幕、正常VSYNC和缺少目标设备的项目只在R34维护。

## R19F 标题字轮、字体与采样复查

技术规范与检查入口：[title-reels.md](title-reels.md)。

| 原轮次 / 来源与当前符号 | 判定与必要性 |
| --- | --- |
| R16 `2abb4f6`：`music-title.ts`/最终CSS → `TextLayoutAdapter` | 原MiSans字重、粗搜/细搜、主标题两行/译名比例、line-height、下伸留白和省略保留。整数Qt字号加局部矢量scale保留分数字号，整行QTextLayout的string index合并连字/组合字，避免拆字破坏上下文。歌名源flex grapheme宽度可能不同于整行kerning；原生优先保留整行字形语义，以真实自然宽度省略，并保持源slot cap与退出保宽。 |
| R16/R16B1：`TitleReel.qml`/`ReelTextNode`/`ArchiveText` | CurveRendering提交矢量普通glyph，Qt Quick Text承担系统彩色字体，未发现现行NativeRendering或逐case渲染分支。两倍DPR层没有独立源调用/技术必要性，移除并同步ResourcePlan；保留实际DPR向上取整容量、线性过滤、逐面/槽裁切和源边缘淡化。歌名局部速度blur为有界Gaussian采样适配，不用于掩盖整行清晰度。 |
| R16B1 `6636ddb` / B2 `509afe8` | 同字体head/ID/位置/scale/框才复用节点，inkEnd截尾部空白，字体数据库变化清缓存，均有语义依据。GUI值描述符到render独立QRawFont、隐藏停止/窗口替换断连、固定容量和缓存上界保留；Canvas/Quick Text候选重shaping不能凭共用controller认作等价。 |
| R16B2V `07514a5` / M2P `9470126` | 混合fallback不应移动主字体，按实际QTextLine.ascent定位；彩色字面保留其inline字体基线。macOS半行距floor由相同主字体的DOM rangeTop/Canvas ascent/descent独立证明，字体框而非图像偏移；分数字号、行高、整行shaping及其他平台分支不改。 |
| R16M1 `c7e9b61` / M2 `b9a5ea7` | 实际源AppleColorEmoji与Qt默认UI面不同；公开应用emoji选择、Regular回退及缺字体回滚保留。RenderControl、target DPR、Review隐藏消费与字体物理表身份检查是线程/平台适配；不能证明原生屏幕/跨屏事件。 |
| R16M3 `2840110` / `e98c798`、R17字体失效接线 | naturalWidth仅变化通知、内容/退出motion决定caption可见性、hide立即停advance并合并queued finish，restore取消陈旧完成，避免绑定重入且保留退出宽度。GL4.1/CoreProfile/basic、后端有界readback drain和有效context释放保留，不改源时间。 |
| B2V-P/B2W/B3及`6740abc`/`b08e94e`交接 | Git中无独立完成的生产实现；现有共享GL/DPR/候选/生命周期代码已按上述归属审查，不能用未完旧任务掩盖遗漏。旧高DPR像素失败保留历史，目标D3D11/Linux GL、真实显示及最终选型仍由R34原专项处理。 |
| `TitleAudit`/`export-title`/`verify-title`与两个PowerShell审计入口 | 删除SSIM/RGB/边缘带、八倍轮廓导出、候选pixelPassed及诊断容错。按可见次序覆盖全部字面（含裁切后的省略号），不因内部slot key间断跳过几何；保留自身真实输出、结构≤1逻辑px、数值轨迹≤1e-6、字体框/物理身份、18页眉、三路径三次成本、隐藏零工作、十次恢复和最终释放；自动通过不替代实际质量检查。 |


## R19G1 详情运动与总览来源复查

技术规范及入口归[detail-motion.md](detail-motion.md)、[overview.md](overview.md)；`audit-detail-overview-source.py`输出冻结/历史/当前逐符号hash和三平台COMMON/PERF矩阵。本节仅负责运动/总览；R17详情展示判定见下节R19G2。

| 实际历史对象 / 调用链 | 来源判定与必要性 |
| --- | --- |
| R18 `ed5aaef` DetailPorts → DetailMotion → SelectionController → SceneAdapter | 保留冻结music-camera/scene的C2抽取、相机/placement共享progress、80ms实际ready、rate9换片与4.2/4.5普通归位。额外layoutRevision为Qt异步布局门禁，不能提前打开文字或改变播放。 |
| R18 drag / ReturningCasePool / CaseClarity → SceneFrame/SceneItem | 保留源水平.004/±.8/2.8跟随、旋转对齐前冻结世界高度、相邻五行净空和逐副本clarity。三副本加选中、满池最后目标和同cell反向回收是有界所有权适配；同矩阵用于绘制/光照/逆射线，运动只更新值与既有封面租约。 |
| R19 `1c9b8c6` OverviewController → Selection/RealColumns → SceneAdapter | 保留源6.5临界阻尼、方向归一化混合/172距离/span/aim、rate9实际相机、±4.5邻域和旋转后Y折叠/.175底边补偿。全API共用投影；extent缓存和一次VP乘积是等价计算提升。R19D4C的实际viewport/DPR修正继续适用，不另加取景/采样补偿。 |
| R19 label admission / OverviewLabelsModel / SurfaceTransition | 保留源独立CSS≤600与碰撞函数<600断点、选中/展开优先、.06/.12滞回、当帧投影、460/240ms卡片和420/320ms入口。十八槽代替源动态DOM；卡片已退出时必须清入口残值，防止240ms退出早于420ms收拢而污染下一列。退场中反转仍复用原槽与显示透明度。 |
| R19 Overview.qml / OverviewBackdrop / overview-blur | 源87%surface及12px有意背景模糊保留；sigma12/radius36、归一化成对采样和固定padding属于有界局部合成适配。只有背景采用逻辑网格，文字保持输出DPR。计数small恢复源block宽度；500字重按源已注册400/600字体匹配规则使用Regular，无合成字重或截图偏移。 |
| R19E/F Header / ArchiveText及后续R19C/D | 页眉各兄弟层独立透明度保持运输按钮位置，字体框/向量字形沿共有实现。全局透明排序/深度一致性、四样本resolve、DOF/雾/AA和尺度由既有专题维护；总览未降低数量/冻结动效/关闭效果。 |
| R18/R19 Review与新结构入口 | 原数值golden直接执行冻结类/生产表达式；新增冻结CSS/DOM结构及同输入visibleOverviewLabels准入。自身Qt事件、投影/纹理/退场/重建与正常输出质量分别证明。320px某输入源函数可准入零标签，不伪造展开成功；没有呈现样本的原生回放保留无效结论。 |

此范围未发现需要按旧截图保留的外观补偿。背景模糊与盒体磨砂/景深属于源设计；物理屏幕、正常VSYNC、原生同窗口恢复和缺目标平台仍按R34原任务，G1不关闭用户两个阵列问题的全历史退出条件。

## R19G2 详情展示来源复查

规范与复现入口归[presentation.md](presentation.md)；`audit-presentation.py`输出冻结、R17及后续提交的逐符号/hash和三平台COMMON/PERF矩阵，独立结构读取实际生产函数与最终CSS，不运行图像评分。

| 原实现/后续改动 | 来源与当前判定 |
| --- | --- |
| R17 `33e8260` Detail/TrackList/DetailModel | content right/bottom漏紧凑覆盖、caption忽略自身高度/提示及aspect锚点、facts断点间距/字号、曲目border-box/小字与emoji normal行高均属遗漏，按最终生产CSS/DOM和实际字体框修正。formatList恢复codec和去重分隔，valueRange恢复两端单位。thin滚动条占位源自实际CSS布局，由布局后合并更新避免绑定循环。 |
| R17状态机、`e98c798` ListView拆分/顶部保持 | 双surface时序、layoutRevision/场景双ready、完成revision、一次pending及退出禁用均保留；类型稳定的两ListView是必要原生适配，保留可见复用池。prepare保持顶部；切tab改为保留/钳制旧距离，移除无源回顶。 |
| `ed5aaef` 详情导航与满池backpressure；`1c9b8c6` optional enter curve | 保留实际navigationEnabled/canSelect到原ports，不能越过副本净空。overline按真实按钮字体框居中；共享SurfaceTransition新曲线不改变详情原时序。运动、材质与总览来源由G1负责。 |
| sticky tabs/indicator、ContentTransition | 原实现只有普通header按钮/下划线，缺源sticky/背景/动态indicator。恢复自然位置吸附、Accessible/左右键与原Bezier数字插值，沿唯一FrameClock；局部sigma18高斯由Qt材质适配，不对正文或三维整体模糊。 |
| albumAbout/producerBlock与离线Provider边界 | 独立离线badge；保留介绍/备注/目录/流派/来源/许可。已有制作资料按源人名/来源分组、trackId索引、角色/URL与未细分署名可读，行虚拟化避免全量委托；展开的多曲目署名使用有界高度的嵌套ListView，这是保留全部本地资料的原生有界适配。查询不执行，原lookup JSON和未知状态仍展示；这是当前离线产品边界，联网实现仍归R26。 |
| 原生文字/资源适配 | 公共ArchiveText分数字号/整行shaping保留；曲目CSS normal以实际resolved ascent/descent分别取整，缓存有界，不加按截图调出的偏移。局部backdrop附件随可见Loader销毁，尺寸/DPR不改变3D目标键。 |

macOS Metal/OpenGL隐藏RenderControl的九尺寸/双主题/三target DPR结构、万曲有界委托、Qt tab输入/滚动钳制、退出/重开及十次恢复已实际验证；自身正常输出无异常裁切或额外模糊。物理屏幕DPR/键鼠触控、原生同窗口/系统焦点与Windows/D3D11的目标补验归R34/R17-P，Linux/OpenGL归R35的R17-P，不外推为全平台通过。

## R19H 全历史闭包与保留适配

`tools/audit/audit-migration-history.py`从Git恢复迁移入口至R19G2的每次提交、每个变更路径/补丁hash及计划历史标题，逐项绑定本索引；它拒绝漏掉的提交/轮次及生产/Review图像评分调用。输出只留本地。专题来源判定仍以本文既有表格为准，自动枚举不替代人工调用链审查。

| 后续提交 / 实际子轮次 | 复查对象与最终判定 |
| --- | --- |
| `4f785b1` | 初始任务/冻结来源/约束，无生产实现；其R01–R19原任务与后续真实子轮次通过Git计划标题恢复。 |
| `279d9a7`、`e9360b7` | 仅计划补充源码优先规则和收口边界，无产品改动；不当成独立已实现轮次。 |
| `8e6c5a3` R19A、`ae60ff8` R19B | 来源保护/脚注字重、源运动/布局/相机/填充与真实列；判定见R19A/B。 |
| `48a9495` R19C1、`75b5543` R19C2A | 最终参数/override去补偿；Quick3D review完整root重建，非生产后端替换；见材料/光场专题。 |
| `3c90d2d` R19C2B1、`ebfb320` R19C2B2、`2d3c9b1` R19C2C | 不透明、完整光学和连续深度一致性；全局排序/独立捕获/显式resolve为源语义所需，跨pass invariant修复保留。 |
| `4ab1e73` R19D1、`2f50812` R19D2 | AO核/标量读回跨度、80byte动态常量前缀和雾/线性颜色顺序；精度与通道适配不改变源算法。 |
| `f45024d` R19D3A、`f4e1610` R19D3B | 源阴影caster/normalBias/PCF与产品地面接收，诊断相机仅Review；不改变生产取景。 |
| `704a99d` R19D4A、`5e333ec` R19D4B1 | 源41点Bokeh及SMAA标量转换完整展开，独立解析输入验证，无浏览器GPU oracle。 |
| `d268570` | 文档唯一职责和经验去重，未变生产代码；被精简的有效门槛仍由对应组件及R34承接。 |
| `4e86136` R19D4B2、`5454fb2` R19D4C | 组合资源图/resolve及生产显示viewport→DPR闭包；旧评分门禁fail-closed，去除重复参考舞台缩放；见D4B2/C专题。 |
| `cfce597` R19E、`a04db48` R19F | 浏览最终CSS、矢量实际DPR/滑尺、整行字体shaping与自然基线；删除四倍图标/两倍槽采样及图像评分。 |
| `3b1f89b` R19G1、`dd5f959` R19G2 | 详情运动/总览状态与详情最终CSS/虚拟列表/本地资料；判定见G1/G2专题。 |

R19C、R19C2、R19C2B、R19D、R19D3、R19D4、R19D4B、R19G是历史拆分父任务，由上列实际子轮次覆盖；没有额外完成的生产提交。R16B2V-P/B2W/B3及R16M3-S等目标交接继续按R34原项验收，不把计划标题当作实机通过。

跨组件提交额外核对：`5b2df80`按词法/字面量与分支顺序检查格式整理，展开短句未改变算法；`d03527e`的预排序键、可见实体/深度预计算与generation先拒绝保留，比较器/帧路径不读盘或遍历曲库。`e98c798`的Cocoa窗口策略、GL4.1/basic、MSAA renderbuffer、RenderControl/DPR、surface恢复与清理均有平台API/资源依据；恢复输出与原生物理呈现继续分开证明。其后生产覆写由B–G专题逐项追踪。

封面/旧工具漏项：R11独立`CoverAudit`移除raw-vs-painted SSIM，contain/inset由CoverCache解析点与源UV公式验证，实际上传尺寸/载荷、lease/generation/背压/图集GPU读回/稳定与SG重建保留；自身输出用于质量检查。`compare-images`及旧材料/光照报告拒绝执行；ImageComparison仅合成算法单元测试链接，不进入产品/Review验证。旧R07–R10C（含R10A）图像聚合审计拒绝，R12/R13审计改读真实矩阵、身份/UV、draw/上传/恢复字段，不再要求已删除的图像成绩。

必要适配归类：布局/文字使用原生字体框、整行glyph与彩色字体；运动/输入使用唯一帧源、原生Qt事件和有限副本；渲染使用同一SceneFrame、显式通道/resolve/排序/clip与资源所有者；数据/封面使用稳定ID、值快照、有界worker/缓存/generation淘汰。已去除的ROI系数、重复viewport缩放、额外图标/标题倍采样没有独立源或技术依据，不保留为画质特性。

原生运动回放的最小化恢复不按固定墙钟时刻结束，而等待两次真实恢复帧及场景终态；有界看门狗仍将缺帧判失败，暂停/首帧dt=0/终点与完成revision断言不变。这是验证驱动的平台适配，没有改变产品帧源或源运动时序。

Quick3D Review补漏：`MaterialReview → sceneGraphInitialized → ReviewGlCapabilities`按实际GL函数能力修正Qt对多样本纹理的误报，走Quick3D原有同格式4×renderbuffer/resolve；源参数、投影、正常输出与完整root重建门槛保留。私有API归RHI适配头，配置期按最新Kit探测；`afterRendering`只在render线程读取真实描述符，再以值快照交GUI。该问题属于Qt后端兼容性，不归原Web项目缺陷。公共Effect的跨目标bake脚本同步改为按宿主选择同Kit的qsb和报告目录，不把macOS静态翻译标为Windows运行。

原生可见驱动补漏：Quick3D Review与R10D集成的“重建”改为销毁完整验证窗口（集成同时重建QML根），重新绑定当前窗口/遥测并按epoch丢弃旧回调，要求真实SG失效及资源/输出恢复；basic的同surface复用仍是另一事实，不再冒充新renderer。场景探针依据其实际shader的packed unit view-normal识别所有面，保留量化误差界、非背景/标签隔离和原数量门槛，避免只识别绿色而漏掉紫色法线面；没有根据旧截图调色或降低门槛。窗口测试同样复用产品的后端初始化以满足Cocoa GL主线程约束。

连续光学原生采集在读取生产输入前确保polish并核对SceneFrame revision，避免把旧帧完成通知与最新相机/景深混读；仍用独立源公式精确检查实际Bokeh上传、全部附件和连续覆盖，不放宽数值门槛。

共享RHI采集器只在完成输入/尺寸配置并连接帧回调后开始显示；不在构造未配置SceneItem时触发basic同步首帧。超时/错误同时保留实际帧数、窗口可见/曝光及renderer错误；不把无效准备帧当作当前状态回放。

原生frameSwapped的同一排队事件批次可在quit之后继续交付；共享采集器只接收所请求的帧前缀，完成后拒绝多余通知，避免再次prepare或把有效回放误判为超额帧失败。精确请求帧数、renderer错误和超时断言保留。

滑尺三路径的生命周期同样使用完整窗口/根重建，controller保留源状态；basic先用有效上下文清理帧退役旧renderer。读回完成强制queued交付，避免同线程RenderControl在endFrame内部销毁自身RHI；每次重新绑定帧/读回且以epoch拒绝旧结果，保留初始化/失效门槛、33次数字/输出恢复、隐藏零工作及最终renderer归零。共享RHI采集器的native rebuild改为完整窗口所有权边界，并重绑item/读回guard；RenderControl仍直接失效。两者都按真实失效事件验收，不以surface复用冒充。

本机实际质量边界：BLUR-01的额外降采样来自生产viewport重复舞台比例，D4C已按最终宿主CSS消除；目标尺寸、DPR与附件链由独立源公式验证。FLICKER-01的已确认根因为visibility/Equal颜色pass位置不一致，C2C统一顶点顺序及invariant；当前正常光学与诊断连续覆盖静止、波浪、浏览、总览进退和详情转换，保留原实体数量、源透明度、景深/磨砂及后处理。本机Metal/OpenGL正常输出未见漏画、额外模糊或重叠闪烁；结构/数学、实际输出和资源恢复分别验证，没有用图像相似度关闭问题。Windows/D3D11、Linux/OpenGL、物理屏幕/输入及后续完整播放器范围按计划后置专项分工验收，不能外推为全平台理想渲染或完整产品性能通过。

正常VSYNC补验覆盖现有R10D单盒生产bootstrap：两后端各三次独立120秒、双主题/输入提交、九尺寸/四画质与十次完整窗口重建，原始序列和各次分布只留本地；采样完整性、资源回落与正常输出分别检查。该入口明确不覆盖完整阵列/搜索/展示/扫描，不能据此宣称完整播放器预算、GPU完成、物理显示或实测显存已通过；后续入口扩展与正式产品矩阵仍归R28及计划后置专项。

## R20 · 序章与交接来源

| 冻结生产来源 | 原生归属与判定 |
| --- | --- |
| music-opening.ts / music-boot.css | OpeningFrame逐参数采样，Opening.qml保留容器/字幕/圆环/纸面/跳过按钮；原创品牌几何只来自branding清单。二维cubic smooth独立于三维quintic ease。 |
| music-boot-timing.ts / music-boot.ts | FrameClock独立片钟、OpeningController事件准备/跳过/重播与720ms错峰；不引入CSS动画或第二帧源，Header复用原品牌节点。 |
| scene.ts的musicIntro / music-camera / viewport-layout | OpeningCamera与SelectionController消费同一shot，覆盖响应式相机、重定位波浪、entryZ/lift及row-light intro；末段保留精确浏览姿态后交给既有弹簧。 |
| REFDB-02 ItemRenderer::render | 采用完全遮挡期在生产提交入口早退的思路；本项目按自身准备/遮挡状态、计数和token接线，未复制参考代码、时间阈值、素材或调度器。 |

原生实际路径与缺环境目标分别由opening.md及迁移计划R20-P管理；OpenGL动态验证归R35。无新增冻结源缺陷例外。
