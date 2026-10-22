# 历史源码复查索引与判定

本文只保存历史轮次/提交→实际对象→适配或补偿判定；具体接口、算法参数、组件检查入口各归迁移计划职责索引中的组件契约。未完成任务/平台回归/BLUR与FLICKER退出条件只在迁移计划维护；运行数据、实验过程和成绩不在此追加。

冻结身份及Git映射由web-baseline.md维护。表中R19编号表示复查责任归属，不能仅凭索引行判为通过；当前结论须读对应专题，R17展示由R19G2确认、R19H核对全量提交和实际子轮次，完整索引见末节。

## R19A 范围与来源

技术规范与检查入口：[web-baseline.md](web-baseline.md)。

| 原轮次 | 引入及后续历史 | 审查对象与责任归属 |
| --- | --- | --- |
| R01 | `a48211c`；`21ef751`、`c7e9b61`、`e98c798` | `source-map.json`、匿名输入与冻结映射；源码清单/隔离匿名输入有效，R19A 审查，组件参数继续下列各轮。 |
| R02 | `24b125a`；`6740abc`、`c7e9b61`、`e98c798`、`1c9b8c6` | `QtKit.cmake`、`Cxx23.cmake`、`ApplicationIdentity.h`、模块/qsb/presets；最新原生 Kit、强制 C++23、同 Kit 工具与默认隐藏策略保留。R19A 审查；渲染 API 适配归 R19D，文字私有 API 归 R19F。 |
| R03 | `83f933c`；`5b2df80`、`6740abc`、`c7e9b61` | `PrepareAssets`、`GeometryAudit`、`AssetCatalog` 与资源清单；原始资产/字体表/模型几何未发现图像驱动变形；补齐 POSIX/直接调用的写入前来源校验。R19A 审查。 |
| R03B | `598c4a8`；`c7e9b61` | 真 SVG 主/小稿、揭示映射、平台容器；原创品牌与小尺寸光学校准来自用户明确要求，保留。衍生资产 hash/容器检查有效，R19A 审查；页眉/开场容器归 R19E/R20。 |
| R04 | `739ee97`；`d03527e`、`b7f73d4`、`0d06b7b`、`1a082f2`、`2ee2294`、`2abb4f6`、`33e8260`、`1c9b8c6` | `MusicTypes`、`LibraryModels`、`LibrarySession`、`OfflineMetadataProvider`；保留本地 URL、字段/稳定 ID、排序/搜索、导航与播放隔离和异步离线结果。R19A 审查；布局/导航状态消费者归 R19B/E/G。 |
| R05 | `ec0af92`；`d03527e`、`ed5aaef` | `FrameClock`、阻尼/五次轨迹、改速/减少动态与 `cancelTravel`；R19B 已按原公式和实际调用顺序审查，结论见下节。 |
| R06 | `5102cb0`；`5b2df80`、`e98c798` | GLB 坐标/UV 的资产事实由 R19A 确认；`LayoutMetrics`、相机/投影/拾取由 R19B 审查；geometryOnly 像素保护属于设备分配适配，MSAA/resolve由R19D4B2确认，目标尺度由R19D4C按最终音乐宿主复查。 |
| R07 | `1556a55`；`52f1278`、`5625d73`、`ed5aaef` | `ShellMaterial`、主盖/侧脊参数和有效override由R19C1核对；shader/捕获/LOD剩余项归R19C2。 |
| R08 | `178ca45`；`52f1278`、`5625d73`、`e98c798` | `MusicLighting`的主题/单位/轨迹参数由R19C1复查；Room/PMREM/BRDF/方向的shader实现与平台采样归R19C2。 |
| R09 | `3fc6a39`；`1f2352a`、`75775b3` | SSAO/depth/normal、核/box与数据边由R19D1复查；阴影/偏置/滤波由R19D3A/B复查。 |
| R09B | `522a3f3`；`1f2352a`、`75775b3`、`e98c798` | Bokeh→R19D4A，SMAA→R19D4B1，MSAA/资源图由R19D4B2复查并完成覆盖映射，尺度由R19D4C复查；颜色→R19D2。 |
| R10 | `d06e5f3` | Quick3D/RHI 能力与历史选型由R19D4B2按可控阴影/全局排序/独立捕获边界确认。 |
| R10A | `52f1278`；`5b2df80`、`d03527e` | opaque/纸面的入射参数与toneMapped接口由R19C1复查；最终shader语义归R19C2，阴影/资源图/采样归R19D。 |
| R10B | `5625d73`；`d03527e`、`e98c798` | 材质参数迁移及R18独立clarity由R19C1复查；透明捕获/排序和完整R10B归R19C2，MSAA/平台附件归R19D。 |
| R10C | `1f2352a`；`d03527e`、`f7e423a` | 图集租约由R19A确认，光学capture/mips由R19C2B/C复查，AO及常量数据由R19D1复查；MSAA/资源图由D4B2确认，尺度由D4C复查。 |
| R10D | `75775b3`；`b7f73d4`、`e98c798` | `SceneItem`透明重叠由R19C2C复查，标量AO/读回由R19D1复查；MSAA/组合资源图由D4B2确认，目标尺度由D4C复查，历史性能只记录的用户边界保留。 |
| R11 | `f7e423a`；`b7f73d4`、`ed5aaef` | `CoverCache`/`AtlasAllocator`/`SelectedCoverPool`/`AtlasUploadQueue`；尺寸、颜色、所有权与有界工作有效；修正缺图脚注字重。R19A 审查；最终采样/合成由 R19C/D 专题确认。 |
| R12 | `b7f73d4`；`0d06b7b`、`e98c798`、`ed5aaef` | 432 实例、波浪/rebase、选中/归位坐标与稳定物理格 ID 由 R19B 审查；合并图集来源见 R19A，透明与后处理由 R19C/D 专题确认。 |
| R13 | `0d06b7b`；`2ee2294`、`1c9b8c6` | 真实列/填充、列记忆、库替换与导航目标由 R19B 审查；R19 的总览插值消费者由R19G1确认。 |
| R14 | `1a082f2`；`2abb4f6`、`e98c798` | 品牌容器/控件/运输信息、遮罩及Canvas合成采用实际窗口DPR与最终竖屏歌名cap，见R19E。 |
| R15 | `2ee2294`；`2abb4f6` | 滑尺/数字轨迹和Pointer/InputRouter沿源数学与有界所有权，见R19E。 |
| R16 | `2abb4f6` | 整行shaping、分数缩放与源拟合使用实际窗口DPR，见R19F。 |
| R16B1 | `6636ddb` | R19F保留完整字形描述符复用、隐藏停止和polish后页眉测量；inkEnd同时截文字与advance。 |
| R16B2 | `509afe8` | R19F按完整shaping结果提交生产字形，窗口可见性与连接寿命由各实际资源所有者维护。 |
| R16B2V | `07514a5` | R19F以QTextLine实际混合ascent确认主字体基线修正；恢复标题尺寸保留，撤回的采样不恢复。 |
| R16B2V-P | 任务见 `07514a5`；未找到独立完成提交 | Windows 未收口像素任务已被后续契约替代；共享字体/基线代码由 R19F 追踪，不恢复原图门禁。 |
| R16B2W | 任务见 `509afe8`；未找到独立完成提交 | 没有独立生产选型；已有共享 DPR/生命周期和工具改动归 R19F，D3D11 目标实机仍归 R36。 |
| R16B3 | 任务见 `2abb4f6`；未找到独立完成提交 | Windows OpenGL 未独立完成；共享 GL 适配归 R19F/D，目标实机仍归 R36。 |
| R16M1 | `c7e9b61`；前置 `6740abc` | R19F保留RenderControl真实target DPR、Review隐藏状态消费和字体head身份；不外推屏幕呈现。 |
| R16M2 | `b9a5ea7` | R19F以源PostScript面与实际run head/advance确认AppleColorEmoji公开选择；缺字体回滚，保留。 |
| R16M2P | `9470126` | R19F用DOM rangeTop与Canvas字体框独立核对主基线至1/64逻辑px，使用macOS floor。 |
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
| `CoverCache::decode/paint` ← `cover-atlas.ts::paintCover/containCover` | worker 的 autoTransform 和有效 ICC→sRGB 是图像解释适配；contain、1/64 留边、透明 RGBA premultiplied 与缺图画布设计有源码依据；缓存层和过滤按用户要求由covers.md维护。原首行 500、脚注默认 400；旧 C++ 脚注继承 Medium，现重设 Normal。没有锐化、按截图扩大封面或预放大小图。 |
| `AtlasAllocator::publish/uploads` → `AtlasUploadQueue::expect/enqueue/take` | generation/identity/lease/serial 先验证再共享/复制；上传预算来自冻结 `flushUploads`。小设备分页及生产合并图集是存储适配；ready 在相同命令提交后有效。 |
| `SceneItem::updatePolish`/`SelectedCoverPool` → `RhiItem` | selected/returning 独立 lease、不可变像素与低清到高清升级；生产无需整页重绘。封面按用户要求采用独立线性光mip与足迹过滤，保留图集区域隔离；opaque shader 对原始texel先除alpha并解码sRGB，再在线性光下预乘、插值和积分，最终仅还原线性颜色；没有编码空间插值或重复解码。 |
| `MusicTypes`/`LibraryModels` ← `music-types/order/search.ts`/`data.ts` | 本地 QUrl/媒体 Unknown、未知字段保留属于原生离线接口；QCollator 后端与 Web ICU 可不同，匿名视觉列顺序固定。规范化键与 ID tie-break、列索引、搜索索引不改变字段/顺序；搜索不改变播放。 |
| `OfflineMetadataProvider::request`/`LibrarySession` 的 queued 结果过滤 | 对请求捕获 const 快照，QObject 上下文事件投递，token/generation/kind 过滤；无网络对象/请求、无缓存写入。缓存旧状态原样返回，无缓存 Deferred。数据/Provider 不因外观复查改变。 |

## 输入与源码验证职责

| 后续轮次 | 必须分离的现存入口 | 保留的有效检查 |
| --- | --- | --- |
| R19B | `SceneTest`核对源相机/投影；`RhiOpaqueChecks`与`ArchivePoolChecks`核对生产几何及两阵列状态 | x/v/a、矩阵、投影、拾取、边界、两模式/改速/减少动态、稳定身份、实际 fallback 区域及资源 |
| R19D | `RhiPostprocessChecks`与实际RHI通道 | 真实附件/采样/resolve、shader 执行、实际质量、上传、生命周期与局部成本 |
| R19E | 生产GUI/Navigation回放、`RulerSourceProbe` | 控件几何/状态/交互、Canvas 隐藏工作、数字轨迹及合成质量 |
| R19F | `verify-title.mjs`/`TitleSourceProbe` | 字体物理身份、整行 shaping/布局、时序/宽度、真实清晰度、隐藏绘制/生命周期与成本 |
| R19G | presentation/detail/overview Review、CSS导出器与状态输入 | 展示布局/虚拟化、场景轨迹、相机/投影/状态、资源及实际质量 |

## R19B 运动、相机与阵列复查

技术规范与检查入口：[archive-pool.md](archive-pool.md)。

| 原轮次 / 生产源及原生符号 | 依据、判定与适配边界 |
| --- | --- |
| R05：`motion.ts::damp`、`MusicArchiveMotion`、`MusicRealColumnScalar` → `ScalarSpring`、`ArchiveMotion`、`ColumnScalar` | 保持解析临界阻尼、五次 x/v/a、1.4–2.4 秒时长、observe/retarget/translate/finish 顺序；C++ 保存精确加速度。同目标 archive 保护是计划明确的固定差异，不由截图决定。R18 `cancelTravel` 只在详情内部从原显示导数交给 rate=9，不重启 browse 轨迹。 |
| R05：`scene.ts` 帧循环 / `music-boot-timing.ts` → `FrameClock`、`App.qml::FrameAnimation` | 生产只有一个帧源，dt 钳制后只乘一次速度；开场时间独立，恢复丢弃首次间隔，减少动态执行完成。没有因截图调参引入的额外时钟或媒体依赖。 |
| R06：`viewport-layout.ts::viewportLayout/archiveFraming`、`scene.ts` 音乐相机目标块、`MusicCameraMotion` → `LayoutMetrics`、`SceneAdapter::target/advance` | 1.05 严格分界、1100/coarse 短屏、音乐 `.25` detailX、560/1080 detailY、preview `.36`、59°/25°→8°/20°、140→72、7.33→5.9 及固定检查槽均是有效生产值。轨道在 navigation 与 detail 插值各衰减一次，是原调用链语义。QMatrix4x4 float 是 Qt 交付适配；canonical GL z 仅用于投影/拾取，GPU 使用 QRhi clip 修正。无额外取景补偿。 |
| R06：`music-model.ts` / `scene.ts::model.rotation` → `SelectionController::transform` / GLB | 模型高度 3.35、底边 .175、厚度 .14 与导入资产一致；音乐 inspection lift=3.47，4.05 只为通用 primitive 压力测试。XYZ 旋转用 double 展开再转 float，独立低 lift/同时 pitch+yaw 输入证明必要顺序，不回退为误差更大的 quaternion 乘法。geometryOnly使用实际物理输出尺寸，设备不支持时显式失败。 |
| R12：`archive-loop.ts` / `scene.ts::field` → `ArchivePool`、`SelectionController::field/rebase/publish` | 9×48 顺序、半周期 floor、5.2/.62 间距、最多六个 3.2 秒脉冲、.24 clamp、2.5 秒 idle 与波浪原点均来自源码。按 selected 相对原点求位移、静止八 ulp 算术残差界和跨 rebase 的有限物理格 ID 是精度/身份适配；不改导数/几何/透明参数。八 ulp 仅覆盖平移、乘法和相消的有界舍入，不是可见像素偏移。 |
| R13：`MusicRealColumns`、`scene.ts::musicColumnRowOffset/musicRealColumnField`、`music-app.ts` 列记忆 → `RealColumns`、`ColumnMemory`、`SelectionController` | 保留真实行 12…11+count、独立中点/列活动度、五次归中及折返导数、目标实际显示深度、记忆开关无播放副作用。按时间惰性取样是解析算法调度适配，避免每帧遍历全部列；减少动态 epoch 同步未观察列。R19 overview插值消费者已由R19G1沿同一progress/相机/投影复查，普通浏览结果不替代总览门禁。 |

## R19C1 材质与光场参数复查

技术规范与检查入口：[materials.md](materials.md)。

| 原轮次 / 源生产路径 → 当前符号 | 判定与历史依据 |
| --- | --- |
| R07：`scene.ts::configureMusicGlass → CardAppearance.register/setTheme → setMusicGlassTheme/setMusicGlassClarity` → `GlassMaterial.qml`、`ShellMaterial` | 三表面transport、coat、线性tint/attenuation及IOR=1.46沿冻结生产公式。主盖clarity独立，侧脊/后托使用原finish；跨API适配依据见materials契约。 |
| R08：`scene.ts::setTheme`与`MusicSelectionLighting.setTheme/update` → `LightingPalette::theme`、`SelectionLighting`、`RowLightController` | 音乐有效环境.25/.16覆盖隔离R07的.48/.68；Three在material.envMap为空时以scene.environmentIntensity更新uniform，不额外乘configure的.65。key .45/.30、hemi .32/.21、fill .045、spot 180/130×64、area 34/24与曝光1.05/1.08保留；GLSL沿用源单位，没有Qt内置brightness换算。650ms曲线、double follow/同一FrameClock与相机局部offset保持。 |
| R10A：`createAlbumPrintMaterial`及`MusicSelectionLighting.shadePrint` → `opaque.frag/paper-lighting.glsl`的参数接口 | 纸面alphaTest=.025、Lambert无specular/emission、先限制source再乘fieldExposure、最终toneMapped=false及线性入射接口有源码依据；qrc/GLB/UV/原生clip适配保留。当前参数变更不改纸面shader、颜色转换、附件或采样。 |
| R10B参数迁移：`5625d73::Renderer::uniformTemplate`、R18 `ed5aaef::Renderer::uniform` | 两处把R07 ROI系数再次施加到实际uniform：基础transport/coat及逐实体lid roughness。现在transport/coat直接复制；lid按实体clarity计算后直接提交，其余缓存和材质不变。原R10B manifest及旧审计脚本的fixedOptics只描述历史实现，不能覆盖当前源码。 |

## R19C2A 材质/灯光 Review 与原图门禁拆耦

技术规范与检查入口：[materials.md](materials.md)。

| 原轮次 / 实际审查对象 | 源码依据与结论 |
| --- | --- |
| R07：MaterialReview的90状态与9纸面探针 | 投影与三个表面参数按源公式核对；均匀墨色推导纸面的线性入射、限制与编码，容差3码值。 |
| R08：MaterialReview的48灯光状态与场核 | 实际光场模式、detail、主题、相机及源表面参数独立核对；LightingTest覆盖924轨迹与392场核。 |
| R10B：VerificationWindow接入、MaterialReview首次show与引擎所有权 | 新CLI在ApplicationWindow加载前分派；独立GeometryScene在macOS使用真正隐藏RenderControl，普通产品窗口及生产RHI不变。 |

## R19C2B1 不透明RHI源数学、资源与门禁复查

技术规范与检查入口：[rhi-opaque.md](rhi-opaque.md)。

| 原轮次 / 实际审查对象 | 源码依据、适配及判定 |
| --- | --- |
| R07：`createAlbumPrintMaterial`→`opaque.frag` | 源Lambert无specular/emission，alphaTest .025、toneMapped=false、先限制原图颜色。sRGBDecode、预乘滤波后恢复颜色、contain/inset与源3.06纸面几何保持；没有ROI系数。 |
| R08：`shadePrint`、锁定Three Lambert/hemisphere/IBL→`paper-lighting.glsl`与CPU原点场 | 对照生产shadePrint替换顺序、Three `lights_lambert_pars_fragment`、hemi及getIBLIrradiance：direct/hemi除PI，IBL自身PI与Lambert BRDF相消；print field最后乘原曝光。entityLight0.xy提升只依赖origin/frame，C1源场核和轨迹独立回归保留。 |
| R10A：`RhiOpaqueChecks`平面、法向与阴影数学 | 核对源位置/法向/深度和receiver strength，独立CTest检查实际输出。 |
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
| R09：锁定Three SSAOShader、SSAOBlurShader→共享ssao及三RHI展开变体 | 使用unpackRGBToNormal、RGB24三通道精确white背景判断、透视标量重建、view normal、scalar noise复制、TBN、全部样本与严格delta判定、5×5 box及.22合成。 |
| R09/R10C：kernel/noise输入与固定索引 | kernel/noise由锁定SSAOPass/SimplexNoise生成；确定性数学输入核对有效，不能扩展成GPU byte witness。RHI半径basis提升与固定索引展开保留，原累加顺序/样本数量不变。 |
| R10D：RGBA16F→标量R16F及诊断解码 | R16F生产raw/blur数学精度和消费者.r等价性保留。查明此前OpenGL错误发生在读回边：本机GL_PACK_ALIGNMENT=4，奇数宽R16F行需填充；当前Qt GL readPixels按紧密纹理字节数分配，未修改PACK_ALIGNMENT，可能越过逻辑缓冲边界。非有限字节不证明产品AO shader生成NaN。 |
| R10C/D及后续capture：行跨度、资源/别名 | 显式捕获且仅未对齐的GL R16F，通过QRhi同格式copy到缓存的补齐宽纹理；原区域逐值保留。按实际storage stride解码且只消费逻辑区域，未初始化尾列不扫描。最多2个AO缓存槽，实际清单含capture-aligned-scalar；同尺寸复用、renderer reset清理。只查询GL状态，不全局改pixel-store，不锁定旧Qt。 |
| R10C/D：PostUniform | kernel在SSAO pass创建时一次上传；动态状态只更新80-byte前缀，std140 ABI为1104bytes、kernel offset为80。 |
| macOS GL能力、窗口及目标平台 | 保留原MSAA等价renderbuffer及真实标准透视/行方向。前台PID误报的源码判定及监测修正见R19D4B2；原生窗口/目标实机要求仍按计划后置专项分工，不将本机结果外推目标平台。 |

## R19D2 生产雾、颜色域及捕获次序

技术规范与检查入口：[postprocess.md](postprocess.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| R07 MeshPhysical/meshlambert；WebGLRenderer::setProgram | 普通render target关闭逐材质tone mapping，故源fog应在线性域；独立材质Review的本地ACES不能进入生产compositor。 |
| R08 createAlbumPrintMaterial / WebGLMaterials::refreshFogUniforms | 纸面toneMapped=false仍启用fog；音乐入口主题曝光覆盖隔离材质默认值，保留该有效调用。 |
| R09B EffectComposer / OutputPass；R10C Postprocess | 最终颜色转换只执行一次；不增添gamma、白雾或截图补偿。 |
| R10A opaque；R10B renderTransmissionPass | 补齐原生遗漏的雾。capture表面与壳体表面各自加雾属于源语义，不因捕获已经有雾而删掉壳体雾。 |
| R10D SceneAdapter::publish → SceneFrame → RhiItem | 按实际渲染相机计算雾范围，并补齐detail-only失效；不采用目标相机或构造期常量。 |
| 5b2df80 / e98c798 / R19C1 | Qt格式、GL多采样能力适配及源材质参数直接上传。 |

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
| Postprocess资源 / 上传 / Review | 复用兼容目标与变化前缀；组件reset/recreate只证明后处理资源生命周期，不等同SG或原生窗口恢复。 |

## R19D4B1 SMAA 源码、查表与真正离屏门禁

技术规范与检查入口：[postprocess.md](postprocess.md)。

| 历史对象 / 来源 | 判定 |
| --- | --- |
| 冻结scene.ts / RenderQuality → SceneItem → Postprocess | 保持Bokeh→SMAA→Output次序；当前生产启用条件由occlusion.md/postprocess.md维护；生产组合实际RT/PSO/SRB闭包由R19D4B2确认。 |
| SMAAShader edges / weights / blend；SMAAPass lookup | 保留源helper、lookup行序/过滤、gamma及alpha语义；生产质量参数按用户要求由postprocess.md维护；源float(ivec2)取首分量后广播，数学期望按真实GLSL展开。 |
| Postprocess资源 / 上传 / Review | 三个生产pass直接离屏执行源数学检查；Review中间目标访问不改变普通构建ABI，不新增产品读回或队列。 |

## R19D4B2 MSAA/resolve与旧门禁覆盖

技术规范与入口归[postprocess.md](postprocess.md)、[rhi-postprocess.md](rhi-postprocess.md)和[test-gates.md](test-gates.md)。组件按独立源公式、实际描述符与资源生命周期验证。

| 历史对象 / 引入来源 | 源码判定及现行覆盖 |
| --- | --- |
| R09B `522a3f3`：GeometryScene MSAA/SSAA诊断、Bokeh/SMAA组合 | 冻结scene构造WebGLRenderer antialias=true，quality.antialias只控制SMAA。生产固定四样本及SMAA；旧SSAA为Review诊断，不能作为产品预设或与源图评分。三个固定Quick3D Effect的原生调度适配保留；生产组合以RHI显式资源图验证。 |
| R10 `d06e5f3`：后端能力选择 | Quick3D公开接口无法控制源shadow/跨实体透明表面全局排序/独立捕获resolve，RHI具明确控制边界，因此保留唯一RHI生产路径。旧残差是否非空、平均损失是否缩小和Quick3D/浏览器谁更接近不是选型或通过条件。 |
| R10C `1f2352a` / R10D `75775b3` / `b7f73d4`：附件与生产接线 | 保留RGBA16F颜色、RGBA8法线/RGB24距离、相同采样的depth、独立resolve与共享保留depth。实际RT/PSO/SRB逐项检查，不以设置值或名称证明。源算法不变；图集/数值更新不重建兼容目标。 |
| `e98c798`：macOS GL MSAA renderbuffer fallback | 仅当实际context缺多样本texture storage时使用同格式、同样本renderbuffer，resolve仍单样本。属于必要API适配，不降到RGBA8或少样本；真实GL检查读取backingFormat/样本/目标和解析部分覆盖，Metal仍使用texture。 |
| 默认隐藏策略的前台PID误报 | 初始/当前PID均为其他进程且无自身激活/焦点历史时，旧foregroundUnchanged仍阻断采集。沿AppKit自身通知增加NSWorkspace工作区激活历史监测；任一初始/当前PID属于验证进程、任一自身激活/焦点均继续失败。外部应用切换单列计数和原布尔值；回归覆盖未知PID、切入验证进程及切出后保留失败，不屏蔽自身短暂抢前台。 |
| `RhiPostprocessChecks`恢复后的可选租约采集 | 空primary lease是既有合法输入，诊断原先无条件索引导致越界。缺租约的metadata现在为null；带fallback必须有完整primary/fallback lease，提交边界先拒绝。该修正不改变生产采样/图集预算。 |

组合通道由`rhi-postprocess-gpu`核对资源图，Bokeh/SMAA/AO/雾/阴影/透明覆盖由各自组件验证，显示尺度由`rhi-scale-gpu`验证。实际门槛与输入归对应专项契约，自身输出供质量检查；source-math、目标GPU、原生窗口和正常呈现分别记录。

## R19D4C 目标尺度、封面与景深单位

| 历史对象 / 生产来源 | 源码判定 |
| --- | --- |
| 冻结music-app → fit/scene.resize → resizeQuality；music.css最终override | `#stage.music-app`宽高100%、`transform:none`，实际host bounding/client比例为1。archive-main的参考舞台缩放属于另一入口，不能套入当前音乐产品。 |
| R09 `3fc6a39` RenderQuality；R09B `522a3f3` GeometryScene | RenderQuality按用户要求直接使用当前逻辑尺寸×实际DPR；View3D填满当前内容。它仍是Review，不改变生产RHI选型。 |
| R10D `75775b3` SceneItem；`e98c798`诊断保护 | product以实际item尺寸再乘`min(width/1920,height/1080)`造成非预期降采样，小窗口最终放大。修正为源宿主比例1；geometryOnly使用真实DPR及设备能力检查。960×640/DPR1原先请求480×320，源音乐入口应为960×640；此为调用链推导，非截图追分。原用户所见窗口未能追溯，复现明确使用新的匿名生产阵列入口。 |
| R10C `1f2352a`、R10D补丁`b7f73d4`、`e98c798` | 显式物理附件、MSAA/resolve、独立全尺寸DOF、传输捕获mip和有界资源所有权是必要原生适配，保留；没有引入整体倍采样、锐化或新模糊。窗口DPR变化重新提交，换窗口断开旧信号。 |
| 封面解码 → pool/atlas → opaque/shell | 保留256缩略图、1024选中独立租约、contain/留白与无跨格mip；linear/clamp采样及透射独立trilinear捕获各有用途。封面高清不由小窗口降级；源磨砂/粗糙度与传输LOD保留。 |
| R19D1 `4ab1e73`；D2 `2f50812`；D3A `f45024d`/`e9360b7`；D3B `f4e1610` | D1的三核/标量精度/常量前缀、D2线性雾/最终颜色次序、D3投射偏置/过滤和产品地面均继续使用既有生产资源图；物理目标和采样质量按用户要求由occlusion.md维护。逐项数学与实际附件规范引用对应组件。 |
| D4A `704a99d`；D4B1 `5e333ec`；D4B2 `4e86136` | 保留view空间focus、完整RGB24 depth、nearest深度/linear颜色、41点UV CoC/.011与aspect；核本身不随窗口像素加倍。保留SMAA源核/lookup、4×几何与单样本resolve/后处理，不以关效果改善清晰度。 |
| 质量/资源/绘制路径选择 | RHI继续承担源全局透明排序/阴影与显式捕获，Quick3D只Review。新独立入口检查实际DPR/物理附件/最终合成和源数学，连续入口保留数量/动效/重叠与恢复；运行结果留本地，物理屏幕、正常VSYNC、完整产品性能及缺设备平台按原专项。 |

## R19E 浏览界面、滑尺与数字轨迹

规范归[browse-ui.md](browse-ui.md)、[navigation.md](navigation.md)、[canvas-painter.md](canvas-painter.md)。

| 实际历史对象与调用链 | 来源与判定 |
| --- | --- |
| R14 `1a082f2` Header/品牌容器、ArchiveButton、ThemeSwitch | 源`music.css`及最后加载的`music-theme-switch.css`定义容器锚点、竖屏网格、17px SVG/圆端点、主题pill；新品牌图形由用户差异替换，容器位置保留。设置图标的两条线/两个圆用Curve Shape保留原矢量几何/层级，原生可访问焦点框是必要交互适配。SVG与play/pause采用CurveRenderer矢量路径，动画scale独立应用。 |
| R14 BrowseInfo/ArchiveText/NavigationFrame/Footer、R15数字接入 | 源最终callout CSS覆盖早期57%定位，采用68.633333%与竖屏clamp；navigation高度驱动callout-bottom。保留CSS normal ascent/descent分别取整、16px父inline strut、1.7行高、眉题span边距/空格advance、按钮继承16px及显式源码padding；这些是文字度量和盒模型适配，分数字号使用局部矢量scale而非增加缓存字号/离屏层。`ArchiveText`通用文字语义及标题专用基线进一步由R19F复查，不能由本节静态锚点关闭。 |
| R14 DesignTokens/ReadingVeil、R19 `1c9b8c6`界面连接 | 650ms颜色/.4,0,.2,1、380ms pill/.22,1,.36,1及源渐变停点有明确CSS依据。直接Quick渐变节点按原层级位于三维与页眉之间，无额外全屏采样；稳定透明层停绘。R19的overview opacity/offset/enabled只是状态委托，实际总览时序由R19G1复查。 |
| R16 `2abb4f6` Header/TransportTitle接入 | song slot增长推动前置控件而右侧play/stop不动，退出保宽由既有TransportMotion管理。该提交将竖屏cap误替换为横屏宽度断点，现恢复最后生产CSS的`clamp(0,width-320,280)`优先级；横屏760/610/340规则及间距保留。标题shaping、退字与采样归R19F。 |
| R15 `2ee2294` RulerController→RulerInk/Quick clip→固定Button池 | 12+2×2槽、anchor5、rate18、560ms ripple/24ms展开及完整刻线/边缘alpha均来自`music-ruler.ts`/CSS；nearest槽匹配与循环occurrence保留。三份actual-size路径按geometry失效，动画高度用rect而不拉伸AA fringe；透明单采样附件固定130×30/96×21，动画宽度仅改变Quick clip。两API使用[Qt默认antialias=1](https://doc.qt.io/qt-6/qcanvaspainter.html#setAntialias)，DPR由所属目标处理，不取浏览器栅格损失为目标。 |
| R15 InputRouter→SelectionController→SceneFrame、OrdinalMotion→OrdinalReel | Qt像素/角度单位与Y反号、控件/祖先所有权和窗口内焦点是输入API适配；源48px/三张/200ms、36px/1.3/1400ms边界保留。最多一个可替换目标服务既有四封面背压，真实提交revision与新输入receipt分开。49点/460ms轨迹来自锁定rolling-number实际函数，保留重定向速度/方向/终点归一、十位/两面和唯一FrameClock；RenderControl专用offscreen开关接通数字/滑尺，普通隐藏窗口仍停进度。 |
| R14/R15 Review与窗口适配 | 导出器交付最终CSS结构与冻结数学；实际纹理、revision、固定位置/键盘路由、静止/隐藏零paint、surface失效/恢复/释放和CPU成本分别验证。滑尺使用OffscreenSurface与有界读回；产品GUI包装读取Process.ExitCode及报告。 |

Canvas Painter的采用依据为源矩形/路径表达、固定值同步、renderer所有权和三份路径缓存；API间像素相似度与旧benchmark成绩不参与选型。实际隐藏质量检查保留正常轮廓/裁切/渐变/文本/图标输出；物理屏幕、正常VSYNC和缺少目标设备的项目只在计划后置专项维护。

## R19F 标题字轮、字体与采样复查

技术规范与检查入口：[title-reels.md](title-reels.md)。

| 原轮次 / 来源与当前符号 | 判定与必要性 |
| --- | --- |
| R16 `2abb4f6`：`music-title.ts`/最终CSS → `TextLayoutAdapter` | 原MiSans字重、粗搜/细搜、主标题两行/译名比例、line-height、下伸留白和省略保留。整数Qt字号加局部矢量scale保留分数字号，整行QTextLayout的string index合并连字/组合字，避免拆字破坏上下文。歌名源flex grapheme宽度可能不同于整行kerning；原生优先保留整行字形语义，以真实自然宽度省略，并保持源slot cap与退出保宽。 |
| R16/R16B1：`TitleReel.qml`/`ReelTextNode`/`ArchiveText` | CurveRendering提交矢量普通glyph，Qt Quick Text承担系统彩色字体，未发现现行NativeRendering或逐case渲染分支。ResourcePlan使用实际DPR向上取整容量、线性过滤、逐面/槽裁切和源边缘淡化。歌名局部速度blur为有界Gaussian采样适配，不用于掩盖整行清晰度。 |
| R16B1 `6636ddb` / B2 `509afe8` | 同字体head/ID/位置/scale/框才复用节点，inkEnd截尾部空白，字体数据库变化清缓存，均有语义依据。GUI值描述符到render独立QRawFont、隐藏停止/窗口替换断连、固定容量和缓存上界保留。 |
| R16B2V `07514a5` / M2P `9470126` | 混合fallback不应移动主字体，按实际QTextLine.ascent定位；彩色字面保留其inline字体基线。macOS半行距floor由相同主字体的DOM rangeTop/Canvas ascent/descent独立证明，字体框而非图像偏移；分数字号、行高、整行shaping及其他平台分支不改。 |
| R16M1 `c7e9b61` / M2 `b9a5ea7` | 实际源AppleColorEmoji与Qt默认UI面不同；公开应用emoji选择、Regular回退及缺字体回滚保留。RenderControl、target DPR、Review隐藏消费与字体物理表身份检查是线程/平台适配；不能证明原生屏幕/跨屏事件。 |
| R16M3 `2840110` / `e98c798`、R17字体失效接线 | naturalWidth仅变化通知、内容/退出motion决定caption可见性、hide立即停advance并合并queued finish，restore取消陈旧完成，避免绑定重入且保留退出宽度。GL4.1/CoreProfile/basic、后端有界readback drain和有效context释放保留，不改源时间。 |
| B2V-P/B2W/B3及`6740abc`/`b08e94e`交接 | Git中无独立完成的生产实现；现有共享GL/DPR/生命周期代码已按上述归属审查，不能用未完旧任务掩盖遗漏。旧高DPR像素失败保留历史，目标D3D11/Linux GL、真实显示仍按计划后置专项处理。 |
| `TitleSourceProbe`/`export-title`/`verify-title` | 按可见次序覆盖全部字面及省略号；核对结构≤1逻辑px、轨迹≤1e-6、物理字体/字体框、18页眉、生产glyph复用、隐藏零工作、恢复与最终释放；实际质量独立检查。 |

## R19G1 详情运动与总览来源复查

技术规范及入口归[detail-motion.md](detail-motion.md)、[overview.md](overview.md)；本节仅负责运动/总览；R17详情展示判定见下节R19G2。

| 实际历史对象 / 调用链 | 来源判定与必要性 |
| --- | --- |
| R18 `ed5aaef` DetailPorts → DetailMotion → SelectionController → SceneAdapter | 保留冻结music-camera/scene的C2抽取、相机/placement共享progress、80ms实际ready、rate9换片与4.2/4.5普通归位。额外layoutRevision为Qt异步布局门禁，不能提前打开文字或改变播放。 |
| R18 drag / ReturningCasePool / CaseClarity → SceneFrame/SceneItem | 保留源水平.004/±.8/2.8跟随、旋转对齐前冻结世界高度、相邻五行净空和逐副本clarity。三副本加选中、满池最后目标和同cell反向回收是有界所有权适配；同矩阵用于绘制/光照/逆射线，运动只更新值与既有封面租约。 |
| R19 `1c9b8c6` OverviewController → Selection/RealColumns → SceneAdapter | 保留源6.5临界阻尼、方向归一化混合/172距离/span/aim、rate9实际相机、±4.5邻域和旋转后Y折叠/.175底边补偿。全API共用投影；extent缓存和一次VP乘积是等价计算提升。R19D4C的实际viewport/DPR修正继续适用，不另加取景/采样补偿。 |
| R19 label admission / OverviewLabelsModel / SurfaceTransition | 保留源独立CSS≤600与碰撞函数<600断点、选中/展开优先、.06/.12滞回、当帧投影、460/240ms卡片和420/320ms入口。十八槽代替源动态DOM；卡片已退出时必须清入口残值，防止240ms退出早于420ms收拢而污染下一列。退场中反转仍复用原槽与显示透明度。 |
| R19 Overview.qml / OverviewBackdrop / overview-blur | 源87%surface及12px有意背景模糊保留；sigma12/radius36、归一化成对采样和固定padding属于有界局部合成适配。只有背景采用逻辑网格，文字保持输出DPR。计数small恢复源block宽度；500字重按源已注册400/600字体匹配规则使用Regular，无合成字重或截图偏移。 |
| R19E/F Header / ArchiveText及后续R19C/D | 页眉各兄弟层独立透明度保持运输按钮位置，字体框/向量字形沿共有实现。全局透明排序/深度一致性、四样本resolve、DOF/雾/AA和尺度由既有专题维护；总览未降低数量/冻结动效/关闭效果。 |
| R18/R19 Review与新结构入口 | 原数值golden直接执行冻结类/生产表达式；新增冻结CSS/DOM结构及同输入visibleOverviewLabels准入。自身Qt事件、投影/纹理/退场/重建与正常输出质量分别证明。320px某输入源函数可准入零标签，不伪造展开成功；没有呈现样本的原生回放保留无效结论。 |

此范围未发现需要按旧截图保留的外观补偿。背景模糊与盒体磨砂/景深属于源设计；物理屏幕、正常VSYNC、原生同窗口恢复和缺目标平台仍按计划后置专项，G1不关闭用户两个阵列问题的全历史退出条件。

## R19G2 详情展示来源复查

规范与复现入口归[presentation.md](presentation.md)。

| 原实现/后续改动 | 来源与当前判定 |
| --- | --- |
| R17 `33e8260` Detail/TrackList/DetailModel | content right/bottom漏紧凑覆盖、caption忽略自身高度/提示及aspect锚点、facts断点间距/字号、曲目border-box/小字与emoji normal行高均属遗漏，按最终生产CSS/DOM和实际字体框修正。formatList恢复codec和去重分隔，valueRange恢复两端单位。thin滚动条占位源自实际CSS布局，由布局后合并更新避免绑定循环。 |
| R17状态机、`e98c798` ListView拆分/顶部保持 | 双surface时序、layoutRevision/场景双ready、完成revision、一次pending及退出禁用均保留；类型稳定的两ListView是必要原生适配，保留可见复用池。prepare保持顶部；切tab保留并钳制当前距离。 |
| `ed5aaef` 详情导航与满池backpressure；`1c9b8c6` optional enter curve | 保留实际navigationEnabled/canSelect到原ports，不能越过副本净空。overline按真实按钮字体框居中；共享SurfaceTransition新曲线不改变详情原时序。运动、材质与总览来源由G1负责。 |
| sticky tabs/indicator、ContentTransition | 原实现只有普通header按钮/下划线，缺源sticky/背景/动态indicator。恢复自然位置吸附、Accessible/左右键与原Bezier数字插值，沿唯一FrameClock；局部sigma18高斯由Qt材质适配，不对正文或三维整体模糊。 |
| albumAbout/producerBlock与离线Provider边界 | 独立离线badge；保留介绍/备注/目录/流派/来源/许可。已有制作资料按源人名/来源分组、trackId索引、角色/URL与未细分署名可读，行虚拟化避免全量委托；展开的多曲目署名使用有界高度的嵌套ListView，这是保留全部本地资料的原生有界适配。原lookup JSON和未知状态仍展示；联网结果按稳定身份更新当前资料，设置与异步路由见library-data.md。 |
| 原生文字/资源适配 | 公共ArchiveText分数字号/整行shaping保留；曲目CSS normal以实际resolved ascent/descent分别取整，缓存有界，不加按截图调出的偏移。局部backdrop附件随可见Loader销毁，尺寸/DPR不改变3D目标键。 |

macOS Metal/OpenGL隐藏RenderControl的九尺寸/双主题/三target DPR结构、万曲有界委托、Qt tab输入/滚动钳制、退出/重开及十次恢复已实际验证；自身正常输出无异常裁切或额外模糊。物理屏幕DPR/键鼠触控、原生同窗口/系统焦点与Windows/D3D11的目标补验归R36/R17-P，Linux/OpenGL归R36的R17-P，不外推为全平台通过。

## R19H 全历史闭包与保留适配

来源责任按下表Git提交与本文件各专题判定对应；运行、画质及目标设备事实由迁移计划R36维护。

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
| `d268570` | 文档唯一职责和经验去重，未变生产代码；被精简的有效门槛仍由对应组件及计划后置专项承接。 |
| `4e86136` R19D4B2、`5454fb2` R19D4C | 组合资源图/resolve及生产显示viewport→DPR闭包；去除重复参考舞台缩放；见D4B2/C专题。 |
| `cfce597` R19E、`a04db48` R19F | 浏览最终CSS、矢量实际DPR/滑尺、整行字体shaping与自然基线。 |
| `3b1f89b` R19G1、`dd5f959` R19G2 | 详情运动/总览状态与详情最终CSS/虚拟列表/本地资料；判定见G1/G2专题。 |

R19C、R19C2、R19C2B、R19D、R19D3、R19D4、R19D4B、R19G是历史拆分父任务，由上列实际子轮次覆盖；没有额外完成的生产提交。R16B2V-P/B2W/B3及R16M3-S等目标交接继续按计划后置专项验收，不把计划标题当作实机通过。

跨组件提交额外核对：`5b2df80`按词法/字面量与分支顺序检查格式整理，展开短句未改变算法；`d03527e`的预排序键、可见实体/深度预计算与generation先拒绝保留，比较器/帧路径不读盘或遍历曲库。`e98c798`的Cocoa窗口策略、GL4.1/basic、MSAA renderbuffer、RenderControl/DPR、surface恢复与清理均有平台API/资源依据；恢复输出与原生物理呈现继续分开证明。其后生产覆写由B–G专题逐项追踪。

`cover-gpu`按contain/inset、解析点与源UV公式核对采样，检查实际上传尺寸/载荷、lease/generation/背压、图集GPU读回及SG重建。矩阵、身份、draw、上传与恢复分别有实际检查，自身输出供质量检查。

必要适配归类：布局/文字使用原生字体框、整行glyph与彩色字体；运动/输入使用唯一帧源、原生Qt事件和有限副本；渲染使用同一SceneFrame、显式通道/resolve/排序/clip与资源所有者；数据/封面使用稳定ID、值快照、有界worker/缓存/generation淘汰。已去除的ROI系数、重复viewport缩放、额外图标/标题倍采样没有独立源或技术依据，不保留为画质特性。

原生运动回放的最小化恢复不按固定墙钟时刻结束，而等待两次真实恢复帧及场景终态；有界看门狗仍将缺帧判失败，暂停/首帧dt=0/终点与完成revision断言不变。这是验证驱动的平台适配，没有改变产品帧源或源运动时序。

Quick3D Review补漏：`MaterialReview → sceneGraphInitialized → ReviewGlCapabilities`按实际GL函数能力修正Qt对多样本纹理的误报，走Quick3D原有同格式4×renderbuffer/resolve；源参数、投影、正常输出与完整root重建门槛保留。私有API归RHI适配头，配置期按最新Kit探测；`afterRendering`只在render线程读取真实描述符，再以值快照交GUI。该问题属于Qt后端兼容性，不归原Web项目缺陷。公共Effect由同Kit的CMake shader目标编译；静态翻译不代替目标运行。

原生可见驱动补漏：Quick3D Review与R10D集成的“重建”改为销毁完整验证窗口（集成同时重建QML根），重新绑定当前窗口/遥测并按epoch丢弃旧回调，要求真实SG失效及资源/输出恢复；basic的同surface复用仍是另一事实，不再冒充新renderer。场景探针依据其实际shader的packed unit view-normal识别所有面，保留量化误差界、非背景/标签隔离和原数量门槛，避免只识别绿色而漏掉紫色法线面；没有根据旧截图调色或降低门槛。窗口测试同样复用产品的后端初始化以满足Cocoa GL主线程约束。

连续光学原生采集在读取生产输入前确保polish并核对SceneFrame revision，避免把旧帧完成通知与最新相机/景深混读；仍用独立源公式精确检查实际Bokeh上传、全部附件和连续覆盖，不放宽数值门槛。

共享RHI采集器只在完成输入/尺寸配置并连接帧回调后开始显示；不在构造未配置SceneItem时触发basic同步首帧。超时/错误同时保留实际帧数、窗口可见/曝光及renderer错误；不把无效准备帧当作当前状态回放。

原生frameSwapped的同一排队事件批次可在quit之后继续交付；共享采集器只接收所请求的帧前缀，完成后拒绝多余通知，避免再次prepare或把有效回放误判为超额帧失败。精确请求帧数、renderer错误和超时断言保留。

生产滑尺的生命周期同样使用完整窗口/根重建，controller保留源状态；basic先用有效上下文清理帧释放旧renderer。读回完成强制queued交付，避免同线程RenderControl在endFrame内部销毁自身RHI；每次重新绑定帧/读回且以epoch拒绝旧结果，保留初始化/失效门槛、数字/输出恢复、隐藏零工作及最终renderer归零。共享RHI采集器的native rebuild改为完整窗口所有权边界，并重绑item/读回guard；RenderControl仍直接失效。两者都按真实失效事件验收，不以surface复用冒充。

本机实际质量边界：BLUR-01的额外降采样来自生产viewport重复舞台比例，D4C已按最终宿主CSS消除；目标尺寸、DPR与附件链由独立源公式验证。FLICKER-01的已确认根因为visibility/Equal颜色pass位置不一致，C2C统一顶点顺序及invariant；当前正常光学与诊断连续覆盖静止、波浪、浏览、总览进退和详情转换，保留原实体数量、源透明度、景深/磨砂及后处理。本机Metal/OpenGL正常输出未见漏画、额外模糊或重叠闪烁；结构/数学、实际输出和资源恢复分别验证，没有用图像相似度关闭问题。Windows/D3D11、Linux/OpenGL、物理屏幕/输入及后续完整播放器范围按计划后置专项分工验收，不能外推为全平台理想渲染或完整产品性能通过。

完整阵列/搜索/展示/扫描使用`StartupChecks`的MusicStage入口。VSYNC请求按performance.md移除禁用变量并核对requestedFormat，Qt呈现事件不等于屏幕显示；单盒样本不能推断完整播放器预算、GPU完成或实测显存。

## R20 · 序章与交接来源

| 冻结生产来源 | 原生归属与判定 |
| --- | --- |
| music-opening.ts / music-boot.css | OpeningFrame逐参数采样，Opening.qml保留容器/字幕/圆环/纸面/跳过按钮；原创品牌几何只来自branding清单。二维cubic smooth独立于三维quintic ease。 |
| music-boot-timing.ts / music-boot.ts | FrameClock独立片钟、OpeningController事件准备/跳过/重播与720ms错峰；不引入CSS动画或第二帧源，Header复用原品牌节点。 |
| scene.ts的musicIntro / music-camera / viewport-layout | OpeningCamera与SelectionController消费同一shot，覆盖响应式相机、重定位波浪、entryZ/lift及row-light intro；末段保留精确浏览姿态后交给既有弹簧。 |
| REFDB-02 ItemRenderer::render | 采用完全遮挡期在生产提交入口早退的思路；本项目按自身准备/遮挡状态、计数和token接线，未复制参考代码、时间阈值、素材或调度器。 |

原生实际路径与缺环境目标分别由opening.md及迁移计划R20-P管理；OpenGL动态验证归R36。无新增冻结源缺陷例外。

## R21 · 本地扫描与缓存来源

冻结`scripts/music-library.mjs`的`AUDIO_EXTENSIONS/safeRootList/walkAlbums/albumEntryId/readAlbum`提供扩展名、根限制/分组、路径哈希与size:mtimeMs语义；`writeJsonAtomic/scan`提供原子替换、整根失败保留旧索引和只读源文件边界。原生实现使用DirectoryScanner的ID/路径索引、可取消迭代遍历及LibraryStore的单worker/最后意图合并；GUI接受const快照后一次应用到LibrarySession，JSON用QSaveFile提交，不迁入Web服务、在线补全或旧PCM路线。相同fingerprint保留已有字段，元数据/封面交R22；具体存储和检查入口归library-data.md，目标文件系统欠验归R21-P。

本机macOS/Apple Silicon已真实执行Release扫描/缓存/模型、默认隐藏窗口策略及适用Metal隐藏回归。扫描事件循环心跳不代替完整产品输入/呈现性能；Windows/Linux原生文件系统和编译器回归仍按R21-P执行。

## R22 · 无声元数据、封面优先级与旧数据

冻结`scripts/music-library.mjs::readAlbum/walkAlbums/resolveGenre/init`提供目录图/单曲内嵌图优先级、元数据回退、v3/v4兼容、制作与未知字段保留的来源。原生通过Qt公开元数据API、两个复用player的专用事件线程、原生稳定ID和一次快照交接接入；原生来源统一为本地QUrl。Loaded与可实际播放分开，具体状态与资源保护仅由library-data.md维护。

REFDB-03只参考`Qt/tests/UiChecks.cpp::cachedSelection`附近三个断言的意图：重复命中资源稳定、同封面换曲不重传、仅更新实际采样绑定。实际读取文件SHA256为`b7a3878617aab8125dd43b0978d9c3bc0b29ad2501c445b5b21092a3538e56af`；仓库LICENSE为MIT、Copyright (c) 2026 LBEILC，未把NOTICE视为全部代码授权。没有复制其实现、测试代码、资产、24MiB预算或逐draw纹理架构；本项目的CoverCache/AtlasAllocator/SelectedCoverPool及生产SceneItem自行补齐同封面、内容revision、阵列/选中/归位覆盖。单身份失效由不可变内容URL和有界池对比触发，无关图集/目标保持原所有权；原来的整池重置属于原生遗漏，未判为冻结源缺陷。

本机macOS/Apple Silicon已实际通过Release无声媒体/封面/导入与缓存检查、默认隐藏Metal生产图集/光学回归；收尾只重跑受影响项，其余有效结果复用。入口由library-data.md/covers.md维护；Windows/Linux实际媒体后端和导入文件系统欠验集中于R22-P，OpenGL共享图形回归仍归R36。无新增冻结源缺陷例外。

## R23 · 音乐库面板与歌曲定位来源

冻结music-app的renderLibraryPanel/renderSearchPanel/revealAlbum、music-search、music-track-focus、music.css与ui-transitions映射至LibraryPanel/SearchPanel/PanelController/TrackFocusController。搜索预处理与增量结果、模态Qt焦点、异步目录选择及虚拟化是原生所有权/工作量适配；匹配语义、稳定ID、面板几何/逻辑像素模糊、归架定位、同专辑复用、滚动与高亮算法不取截图参数。大流派列表保留源换行宽度并在有限区域虚拟滚动，避免源DOM数量随曲库增长；没有加入网络、播放副作用或第二取消机制。

Release本机Metal隐藏入口分别验证模型/状态/实际MusicStage输出；修正缺封面直接走占位，避免空路径文件探测。结构/搜索/焦点与自身输出分开，正常VSYNC、物理输入/原生目录对话框与跨屏DPR仍由R23-P及既有产品实机专项验收。具体接口/资源/检查入口只在library-data.md维护；平台审计与原始报告留本地。

## R24 · 普通歌曲播放与设备来源

冻结music-app的playAlbum/按钮/Space及music-player的toggle/previous/next/stop/seek映射至PlaybackController与实际MusicStage控件。按用户指定差异使用单个公开Qt歌曲player/output，保留队列、>3秒上一首归零、暂停/停止/定位隔离；原无缝、歌曲淡变、浏览器/桥接输出不进入产品。REFDB-04只适配公开API的待播/意图/有效位置思路，正常切设备不重建会话，未引入私有声道补丁、在线规则或模拟描述符；hash/作者/MIT随包保留。

本机Release实际Qt匿名WAV播放/进度/暂停/seek/停止、自然换曲/格式错误，以及Metal隐藏MusicStage/输出面板/定位隔离分别验证。空设备列表/重复与晚到通知只验证状态分支，不冒充物理拔插或实际听感；其他系统/后端、真实输入/设备与格式覆盖按R24-P及公共后置分工保留。当前运行支持状态与无声元数据历史索引分开，不以Loaded或扩展名宣称播放能力；技术规范与检查入口只在playback.md维护。

## R26 · 设置与离线资料来源

冻结music-app::renderSettingsPanel、music.css最终theme-cards/settings-row/quality覆盖、quality-settings及music-motion-settings分别映射至SettingsPanel/ThemeCard/SettingChoice/SettingRange、既有RenderQuality和唯一FrameClock。主题卡保持两列，景深强度可调、MusicBrainz可折叠，设置面板沿用既有源几何/遮罩/背景；原生Qt控件负责编辑/焦点/Accessible。资料查询采用原生Provider，页面reload仅重建视觉树，普通媒体对象/艺术参数保留；已排除的产品能力不进入设置。

REFDB-05只参考Host::acceptCookies与cover完成回调中代次/订阅者检查的局部思路；既有LibrarySession的requestId/generation已足够，补齐不同代订阅者晚到、批量串行与能力通知，未采用其Host、网络、账号或队列架构。实际读取文件hash/作者/MIT随包参考NOTICE保留；设置和资料技术规范仅由library-data.md维护。普通产品注册OnlineMetadataProvider；演示/验证使用OfflineMetadataProvider，测试注入不产生外部网络请求。

本机Release偏好/Provider/配置及Metal隐藏设置输出按对应入口验证；Qt事件、显式target DPR与隐藏App视觉树重建分别陈述，不外推物理输入、跨屏、正常呈现或听感。其余目标与原生产品事项集中保留于R26-P及最终矩阵。

## R27 · 光照/速度面板与匿名入口

| 冻结生产来源 → 当前入口 | 判定与边界 |
| --- | --- |
| `music-lighting-lab.ts`/`music-row-lighting.ts` → `LightingPanel`、`SelectionLighting::configure/setComparing/resetSettings` | 保留四模式、五参数范围、guided预设、按住原版/释放恢复、窄屏折叠与输入隔离；只更新已有光场参数，保持跟随x/v。Qt控件承担焦点与无障碍，不引入浏览器URL状态。 |
| `music-motion-lab.ts`/`music-motion-settings.ts` → `MotionPanel`、SettingsController/FrameClock | 保留.25–3倍率、.05步进与1×复位；沿唯一时钟保持轨迹与片头/歌曲独立。 |
| `music-lighting-lab.css` → `MusicStage`实验容器/共享ScrollView | 保留左侧响应式位置、272/254/248宽度、页眉/底栏避让；源文字主题色直接绑定项目自绘控件；窗口太窄时限定可用宽度，滚动限于实验面板。 |
| 音乐生产入口 → 原生启动参数与run-native脚本 | 普通入口、匿名demo和匿名lighting-lab分别路由；用户指定演示不读取真实库/自动播放优先，产品资产和匿名标题不带私人路径。 |

诊断用实际单调提交/原生Qt呈现事件和既有上传/解码计数，统计范围以performance.md为准。

## R28 · 启动快照与异步生命周期

StartupCoordinator把异步扫描/模型快照与既有OpeningController的真实渲染就绪事件相接；Preparing重确认最新快照，Film到handedOff之间固定展示状态。它是原生快照/线程交接适配，开场数学、片长、材质、画质和普通媒体语义沿原契约。减少动态静止时由同一FrameClock消费失效，帧内/片尾回调产生的新失效保留；普通波浪和活动过渡继续使用原时间数学。隐藏解码/提交门槛、剩余tile续帧与退出前decoder join属于原生所有权收口。

REFDB-06仅参考UiChecks中静止、绘制后失效、隐藏恢复、缓存回选和重建保留歌曲的场景意图；作者LBEILC、MIT，采用边界仅为这些场景意图。未复制实现/素材、其资源释放策略或截图比较。具体启动状态、资源边界、统计与入口分别由opening.md、covers.md、rhi-integration.md和performance.md维护；目标欠验集中在迁移计划R36。

本机Release已实际执行启动/帧源/封面/开场/实验状态和隐藏Metal生产图集、片头、诊断与完整舞台生命周期回归；三次串行120秒隐藏成本回放保留独立原始样本。初始化、准备时延、GUI/同步/Quick提交/endFrame等待、RHI提交及资源描述符分别记账；不将其解释为原生VSYNC、物理输入/显示或driver显存验收。

当前非OpenGL阶段的提交链判定如下。

| 当前源码边界 | 静态判定 |
| --- | --- |
| StartupCoordinator::schedule/applyPending → LibrarySession | 单个queued通知与一个最新不可变快照；没有pending时不进入库替换。Film冻结、隐藏与恢复门槛不引入周期等待或逐帧扫描。 |
| MusicStage.FrameAnimation → FrameClock::advance → SceneAdapter::publish → SceneItem::schedule/updatePolish | 原生唯一帧源、GUI失效合并与最新revision提交；正常波浪的实体工作集至多436，不读取封面文件或等待解码。不能从这条源码链推断Qt原生曝光和驱动调度的实际时序。 |
| RhiItem::submit → Renderer::synchronize/renderFrame → SceneItem::configureTelemetry | render仅使用同步快照，上传有界；无捕获稳态不做GPU读回/finish。遥测邮箱至多128个样本，queued通知合并且epoch拒绝旧结果，GUI接收时刻与render提交时刻是不同事实。 |
| StartupChecks::Driver::wait / AuditWindow::record | 原生等待由真实frameSwapped排队评估，FrameAnimation继续由Qt驱动；曝光、GUI帧、同步、RHI提交和Qt呈现分别记录，GUI接收时的clockFrame不能代替独立帧时间戳。 |
| StartupChecks原生observe分支 → verify-startup | 只缩小诊断驱动与匿名输入，复用同一生产MusicStage/RHI；独立时间流和失败证据保留不改变源运动、画质、渲染图或正常播放器行为。 |

## R29 · 整场GUI来源与状态闭包

| 生产范围 | 来源与当前判定 |
| --- | --- |
| Header / BrowseInfo / 字轮 / 滑尺 | 冻结music-app及最终music/theme/transport/navigation CSS、music-title/text-motion/ruler；布局、字体整行shaping、数字与唯一帧源的逐项依据沿R19E/F及共用控件契约，不以截图反推数值。 |
| 阵列 / 盒体 / 光带 / 背景 | SceneAdapter/SelectionController发布源相机与实体，RowLighting/LightingPalette提供世界光场；材质、透明排序、雾/AO/Bokeh/MSAA/SMAA和最终颜色依据沿R19B–D。整场采集保留相同生产RHI，不设置辅助相机、降画质或取消设计模糊。 |
| 详情 / 总览 / 片头 | PresentationController/OverviewController/OpeningController沿R19G与R20的状态、布局和源数学；整场重放只编排现有控制器与窗口内Qt事件，不建立另一套运动算法或产品输入路径。 |
| Detail::detailBack | 冻结music-theme-switch.css最终竖屏left22/top118优先于music.css早期宽度断点，显式响应式字号按SDE-01的正常预期。Detail的位置/字号遵守该优先级与预期，返回按钮纳入独立DOM/CSS结构采集与≤1逻辑px比较。 |
| NavigationFrame | 冻结CSS Grid在空间不足时横向溢出。按SDE-02以操作组自然宽度判定换行并限制长列名显示，保留源字号/滑尺/间距，可容纳时走原单行算法。 |
| Browse::overviewToggle | 冻结music-overview.css的`data-mode="detail"`规则同时隐藏并禁用总览入口。visible与收藏架交互准入一致，详情及其进退期间不绘制该入口。该状态映射遵守正常源规则，无冻结源缺陷例外。 |

本机Release隐藏Metal使用完整生产舞台覆盖九尺寸、双主题、两阵列与代表target DPR，分别核对Qt交互、结构/运动、正常自身输出、资源与释放；入口和断言归rhi-integration.md及各组件契约。数值与布局合格不代替实际清晰度/轮廓/裁切/合成检查；显式读回不进入帧成本。已有且未受本轮改动影响的单项材料/后处理/字体/面板/生命周期与R28正式隐藏成本结果按test-gates.md复用。未运行目标、物理输入/跨屏、原生窗口/正常VSYNC与完整产品正式性能仍由迁移计划R29-P及既有专项管理。

## 共用控件来源

冻结style.css/music.css的继承字体、输入框/选择器/checkbox/range/progress以及music-lighting-lab.css最终按钮/滑杆覆盖映射至Archive控件；来源框与文字行高按[controls.md](controls.md)显式设置。QtQuick.Templates仅提供公开输入/焦点/Accessible行为，所有产品/研究/Review视觉由项目组件与DesignTokens绘制。浏览器原生内部装饰映射为明确的项目几何，原生弹出菜单/滚动条空间由项目限定，不宣称其装饰是源CSS常量。FolderDialog保留操作系统目录选择。

控件自身双主题输出、来源几何/行高与Qt编辑事件和宿主集成分别验证。物理输入、系统辅助技术/输入法、原生窗口与其他目标环境按UI-CONTROLS-P保留，技术规范不在此重复。

## macOS部署与原生应用边界

| 生产调用链 | 判定与覆盖 |
| --- | --- |
| CMake APPLE → Info.plist/ICNS → Deployment实际QML → macdeployqt → Mach-O/签名 → 包外产品探针 | 原生适配，保留既有品牌与qrc资产；最新Kit的生产Release/Review OFF独立包实际加载Qt/QML/媒体。中文/空格重定位、启动器参数与清洁查找路径分别核对，动态映像限bundle/系统，不依赖开发Kit。 |
| main → ApplicationMenu → PanelController / 当前Qt编辑器 / 系统原生对话框 | 系统菜单使用公开AppKit，Command设置尊重片头锁；编辑能力沿焦点/只读/选区与撤销状态判定，原生对话框沿系统responder。QPointer与RAII保证菜单不持有重建后的视觉对象；关闭/退出沿既有Qt媒体停止链。 |
| MusicStage.visualActive → FrameClock/SceneItem/封面；独立媒体会话 | 后台视觉停止、媒体意图与来源独立；部署不改变渲染图、源材质/运动/Canvas透明合成或缓存。现有R28正式隐藏成本及未受影响组件结果仍有效。 |

本机已实际执行Release资源/模块/shader/公开媒体/隐藏窗口与AppKit菜单检查，以及Metal绘制、滑尺、完整片头和设置自身输出；实际轮廓/裁切/合成与状态、资源断言分开。部署包的隐藏加载不证明GPU呈现、物理菜单输入、跨屏、设备听感或正常VSYNC。其余目标与产品物理验收保留R31-P及迁移计划R36，技术入口只在macOS平台文档和组件契约维护。

## Linux部署与后端边界

| 生产调用链 | 判定与覆盖 |
| --- | --- |
| Linux CMake生产目标 → Qt QML部署API → ELF闭包/相对RPATH/qt.conf → 包外产品探针 | 原生平台适配，沿实际编译QML清单部署；补齐xcb/Wayland会话集成、Qt随附媒体和C++ CanvasPainter运行库，品牌/字体/模型/shader/音频仍由既有qrc提供。独立目录包不修改系统、驱动或音频配置；原包仅在依赖/完整性/实际加载通过后替换。 |
| GraphicsBackend → QGuiApplication → BackendCapabilities → Qt生产场景图 | Linux默认Vulkan；默认或显式Vulkan的一次初始化/附件格式探测有明确回退，探测资源不进入生产render线程。源渲染图/效果/相机/运动/控制器不变，Vulkan初始化不代表窗口/swapchain或完整产品验收。 |
| 根sh/演示/实验启动器 → 包优先/Release开发回退 | 保留参数、Unicode/空格和退出码，包内查找路径不借外部Qt/媒体库；显示会话、XDG及音频路由保持调用者选择。播放器不引入Python/ELF工具等开发运行依赖。 |

COMMON/LNX/PERF源码审计覆盖非Windows路径、大小写/权限/数据根、媒体状态/关闭、触控板、字体和Canvas缓存/线程/资源图；实际产品导入闭包与qsb跨目标变体由同Kit检查。主机macOS的Release、官方产品qmllint、默认隐藏策略与Metal独立后处理、生产包加载回归只证明受影响共享入口。Linux原生编译/打包/ELF与真实媒体成功路径、Vulkan编译/设备及xcb/Wayland图形/窗口/输入/性能尚需目标环境，完整保留于R32-P并按R36分工；无新增冻结源效果例外。

## 联网资料来源适配

冻结`album-introductions.mjs::assessCandidate/lookup`的标题、歌手、年份与歧义准入、来源白名单和有序查询映射至WikipediaSource/MetadataWorker；以Qt Unicode规范化、Wikipedia重定向/标题转换及Wikidata多语言别名处理现有输入，不引入运行时OpenCC依赖。冻结`music-library.mjs::enrichAlbum`的保守发行匹配保留，人员角色与缓存调度的来源缺陷按source-defect-exceptions.md处理。新实现增加实际曲目核对、取消确认背压、Qt专用线程、响应/时限上界、独立网络授权、有效期与批次合并保存；不复制旧HTTP服务、全库逐项保存或仅producer角色筛选。用户选择的来源为MusicBrainz及Wikipedia/Wikidata，具体行为唯一由library-data.md维护。
