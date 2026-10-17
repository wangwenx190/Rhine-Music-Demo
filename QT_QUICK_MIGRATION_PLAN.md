# yalahOO!!! · Qt Quick / QML 原生迁移执行计划

## 优先级与源码基准

- 优先级：用户最新明确要求 → 本计划 → `AGENTS.md`及各专项契约 → 历史资料。下表专项契约提供对应实施细则；发现不一致时直接修正文档并同步引用。历史任务、授权和验收方法不自动沿用。
- 冻结基线为 V0.4.1、`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，通过`docs/contracts/history-rewrite.json`及`frozen-git.mjs`/`ResolveFrozenRevision.ps1`解析清理后的等价源码。旧路径在当前树对应`legacy-web/`，旧模块名从该目录解析；根`.git/`保留。Web工具在`legacy-web/`运行并显式使用隔离`MUSIC_DATA_DIR`，不得写回冻结文件或用户旧数据。
- **所有外观、几何、相机、字体、材质、光照、特效、动画与合成，均以冻结源码的实际生产调用、最终override和理论效果为基准。** 新品牌及用户指定差异优先；布局/数学/状态可严格核对，原实机图只作大致参考。
- **禁止用截图逐像素比较、SSIM/RGB/ROI/边缘带分数或浏览器GPU字节相等驱动实现、调参、选型、诊断或验收。** 遗留耦合工具只保留必要的非像素检查；旧分数不重跑、不改写为通过、不再作为待修门禁。
- **冻结原项目中明显的缺陷、bug或不合理行为不得强行复刻。** 能从生产调用、状态与上下文推断正常预期时，实现该预期行为/表现；无法可靠推断时跳过该特性。两种判定均在[原项目缺陷与复刻例外](docs/contracts/source-defect-exceptions.md)记录来源、问题、预期依据、处理及影响边界；本原则同样约束历史复查与本轮修正。
- 原生运行检查漏画、错位、裁切、错误合成、锯齿和异常模糊；按源码定位并消除缺陷，保留设计景深/磨砂/背景模糊。禁止锐化、整体模糊、噪点或无依据改变分辨率来掩盖问题。

## 文件维护与经验规则

- 本文件只保留长期约束、剩余任务和必要经验。完成轮次直接删除，编号不重排；依赖指向既有交付物，不要求保留已完成小节。未完成项须有明确修复对象、输入、依赖、入口和退出条件。
- **经验只记录“明确触发条件 → 核心解决方案”。** 必要适用边界写入触发条件或方案本身；不记实验过程、尝试顺序、失败数值、验证成绩、过程日志或重复说明。只保留已确认、后续仍适用的经验；未决问题归入具体待办，不写成经验结论。
- **所有项目文档必须职责清晰且单一，每个领域只保留一份对应的唯一权威文档，并始终与项目最新代码同步。内容必须正确、清晰、简洁，不允许过时、错误、模糊或冗余；绝不允许写入任何过程性文字。** 修改时直接更新权威原文，消除冲突、重复和失效引用；不得靠优先级叠加互斥要求，也不得列举已删除对象或记录开发、迁移、清理过程。职责由下表索引确定，其他文档只引用，不重复维护。
- **项目不保留对老字段、老设置项或任何已弃用设施的兼容性；确认退役/弃用时必须当轮完整移除。这里“移除”意味着项目中不再存在对应概念的一切蛛丝马迹，如同项目从未认识它。** 包括名称、实现、注释、文档、测试、脚本、工具、依赖及构建/验证接线。不得用运行时识别后删除、迁移、黑名单、兼容/占位/转发入口、disabled或报错退出代替移除，也不得专门写防止该概念再被引入的测试或闸门。测试仍在使用不是保留理由，须沿真实生产调用链同步改写或删除完整关联闭包。
- 职责索引：计划管任务与关键边界，专项契约管技术规范，来源复查文档管当前源码的逐项判定，平台文档管操作。
- 运行输出按[验证产物边界](docs/contracts/verification-artifacts.md)保留本地：`verification/`、`docs/media/`、`docs/contracts/web-scenes/`及旧Web输出不入Git。只提交源码、产品资产、真正测试输入和必要简明契约；大体积证据须用户明确指定个别文件。提交前按明确路径暂存并检查文件数量/体积，禁止改名绕过规则。
- 本阶段不自动发布、上传私人数据、改写发行标签或改动用户音乐文件；保留冻结资料、作者署名与资源许可链。

## 执行顺序、规模与动态验证

- 当前主线：`R34 → R35`。一次只执行一个轮次，不跳轮；历史来源与收口结论见source-fidelity-review.md。R17–R33的“依赖R16”等指实现与阶段交接，不等待后置实机专项，避免循环依赖。
- 开始前按代理实际能力估算代码增删、实施、构建、验证、文档和提交总耗时并简报。代码理想≤1000行、通常≤2000行，上限目标两三千行/一小时；预计约4000行或七八十分钟仍不能收尾时，必须先拆计划，明确接口/输入/门槛。执行中超出预估及时缩小边界，保留未完成任务，不追认超时为达标。
- 行数按新增与删除之和计，不能净增抵消或拆提交规避；生成脚本、文档、素材不计代码行数，但其执行时间计入。正常思考、工具等待、构建和调试均计时；仅断网/用户中止间隔扣除，恢复不重置累计时间。必要收尾允许小幅浮动。
- 每轮依据实际系统/CPU、最新Kit、后端/设备、匿名输入、采集入口和窗口策略判断能力。当前主力macOS/Apple Silicon优先真正离屏；本轮受影响范围内能可靠执行的构建/测试/渲染/媒体/部署必须执行。缺常规工具先补齐；源码可确认的缺陷、本机可靠检查失败或本轮应提供的入口缺失必须修复。
- 只有独立缺环境或采集不可靠的子任务可严格审计后移交后置专项（非OpenGL归R34，OpenGL归R35），保留原任务ID、目标/能力缺口、源码与输入、已知失败、修复对象、准确入口/预期结果、原门槛、依赖和受影响回归。移交不代表通过，不减少实机要求。已交后置专项默认集中处理；用户指定提前补验仅处理指定已实现边界。
- 已交付目标的受影响构建/部署回归按test-gates.md执行；R32-P保留Linux目标环境的待验闭包。R34负责非OpenGL的后置实机事项，R35集中执行OpenGL动态验证；两轮按实际环境及规模拆执行单元，缺环境时保留任务。

### 测试与窗口

- 后端执行分工：R20–R34的Windows默认动态检查使用D3D11，macOS默认使用Metal；显式D3D12和三平台实际启用的Vulkan同归R34；三平台OpenGL/GLES上下文、GPU/窗口、质量、输入、生命周期与性能统一归R35，包含Linux显式OpenGL。各轮照常完成源码/算法/构建/模块和适用的跨目标qsb静态检查，保留OpenGL实现及原门槛；用户明确点名的单项复现按其指定执行。
- 验证优先复用已有有效结果；不影响行为的注释/文档/脚本等改动免测，逻辑改动只跑对应测试，不因共享代码变更或提交惯性执行全量回归。Release配置、测试分组、有效性判定与执行范围统一由[test-gates.md](docs/contracts/test-gates.md)定义；正式成本/性能采样执行[performance.md](docs/contracts/performance.md)。
- 全部测试/采集/重建执行[test-window-policy.md](docs/contracts/test-window-policy.md)。默认隐藏；当轮可见授权与结束恢复只按该契约处理，不跨轮次继承。采集限自身渲染，不能为遮挡或节流提升前台。
- 离屏、Qt提交、正常VSYNC、物理输入/屏幕和媒体听感是独立事实，按实际能力分别验收，未执行或无有效样本保留后置任务。

## 产品与身份

- 原生三平台Windows/macOS/Linux，C++23、最新本机原生Qt、Qt Quick/QML、Qt Quick 3D（Review）、QtMultimedia与根CMake；移动端不在范围。QML管界面，C++管模型、状态机、运动、扫描和控制器。
- 最终运行不依赖Node、浏览器、WebEngine、本地HTTP、Swift桥接、外部FFmpeg或新增第三方库；Qt随附依赖由部署工具处理，不自行链接内部FFmpeg。Web开发工具只可用已有锁定依赖，不新增图像比较库或下载浏览器。
- 普通播放器只用QtMultimedia公开API：播放/暂停/停止/上下曲/队列/进度/音量/设备/错误。歌曲、BGM、UI音效独立；删除旧无缝三档及浏览器/CoreAudio后端选择。不实现无缝、交叉混音、手动PCM/重采样/音频回调/环形缓冲/自建引擎，不引入QAudioSink/QAudioDecoder/QAudioBufferOutput或AVAudioEngine/CoreAudio路线。
- 保留双主题、减少动态、0.25×–3×速度、响应式、真实列/填充、列记忆、详情拖动、画质与光效调节；浏览/搜索/总览定位不改变播放。旧`?original=1`、PWA、浏览器菜单限制、在线播放代理及拆解研究界面不进入产品。
- 本阶段零网络请求；保留异步Provider、来源/缓存/状态/查询配置、未知JSON字段和计划明确的未来能力扩展；已确认不再被生产使用的字段/设置直接删除，不保留兼容，不能伪造查询结果。
- 展示名统一`yalahOO!!!`，技术名`yalahoo`；CMake项目`yalahoo`，目标`Yalahoo`/`YalahooCore`，输出`yalahoo`，QML模块`Yalahoo.Music`。创建引擎/读取QStandardPaths前设organizationName/applicationName=`yalahoo`、applicationDisplayName=`yalahOO!!!`，改展示名不迁移数据根。
- macOS bundle ID=`yalahoo.music`；Linux desktop/图标名=`yalahoo`。旧档案/署名保留原名。原创图标/Logo以真正SVG路径为主，不嵌PNG、不套用旧标识；主素材`resources/branding/`、衍生`resources/icons/yalahoo/`，设计/中心线/分组/安全区/16–32px光学稿及来源按[branding.md](docs/contracts/branding.md)。品牌替换保留原容器锚点、留白、层级和动效时间窗。

## 专项契约与职责

下列文档各管表中职责；复现命令放所属组件，公共规则只在对应公共契约维护。来源hash/数值输入以其引用的JSON和manifest为准，当前Kit/运行成绩不写成永久平台限制。

| 文档（`docs/contracts/`） | 唯一职责 |
| --- | --- |
| [code-style.md](docs/contracts/code-style.md) | 源码格式、格式工具及官方qmllint使用 |
| [cpp23-best-practices.md](docs/contracts/cpp23-best-practices.md) | C++23语言、资源、类型与接口实践；不重复构建/格式门槛 |
| [qt-best-practices.md](docs/contracts/qt-best-practices.md) | Qt对象、信号、线程、模型与值语义实践 |
| [qml-best-practices.md](docs/contracts/qml-best-practices.md) | QML/C++职责、声明式绑定、类型、视图与视觉生命周期实践 |
| [native-bootstrap.md](docs/contracts/native-bootstrap.md) | 最新原生Kit发现、C++23探测、构建与模块加载 |
| [web-baseline.md](docs/contracts/web-baseline.md) | 冻结来源解析、匿名输入和基线重建 |
| [assets.md](docs/contracts/assets.md) / [branding.md](docs/contracts/branding.md) | 导入资产/字体/图标/音频来源；原创品牌资产 |
| [playback.md](docs/contracts/playback.md) | 普通歌曲Qt播放、保留队列、进度与输出设备/恢复 |
| [library-data.md](docs/contracts/library-data.md) | 稳定身份、本地扫描/存储、排序/搜索与离线Provider接口 |
| [motion.md](docs/contracts/motion.md) / [scene.md](docs/contracts/scene.md) | 唯一帧源/运动数学；布局/相机/坐标/拾取 |
| [opening.md](docs/contracts/opening.md) | 二维序章/三维开场、事件就绪、遮挡提交与交互交接 |
| [materials.md](docs/contracts/materials.md) / [lighting.md](docs/contracts/lighting.md) | 源材质/纸面语义；世界光场/主题/源单位 |
| [canvas-painter.md](docs/contracts/canvas-painter.md) / [browse-ui.md](docs/contracts/browse-ui.md) | 两种绘制API共用能力与选型；浏览静态布局/遮罩/图标合成 |
| [controls.md](docs/contracts/controls.md) | 项目自绘控件、来源尺寸、输入行为与控件检查入口 |
| [navigation.md](docs/contracts/navigation.md) / [title-reels.md](docs/contracts/title-reels.md) | 滑尺/数字/输入；标题字轮/字体/文字缓存 |
| [presentation.md](docs/contracts/presentation.md) / [detail-motion.md](docs/contracts/detail-motion.md) / [overview.md](docs/contracts/overview.md) | 详情展示；抽取/换片/拖动；总览取景/折叠/标签 |
| [covers.md](docs/contracts/covers.md) / [archive-pool.md](docs/contracts/archive-pool.md) / [real-columns.md](docs/contracts/real-columns.md) | 封面解码/图集/租约；填充池/选择坐标；真实列/会话记忆 |
| [logging.md](docs/contracts/logging.md) | 分类日志、Qt消息格式、UTF-8控制台/文件、终端颜色、轮转与保留期 |
| [render-backend.md](docs/contracts/render-backend.md) | 后端选择和SceneAdapter固定业务边界 |
| [display-output.md](docs/contracts/display-output.md) | HDR/SDR输出、平台动态检测/白电平与默认绘制质量 |
| [rhi-opaque.md](docs/contracts/rhi-opaque.md) / [rhi-shell.md](docs/contracts/rhi-shell.md) | 不透明目标/阴影/地面；透射捕获/壳体/排序 |
| [occlusion.md](docs/contracts/occlusion.md) / [postprocess.md](docs/contracts/postprocess.md) | SSAO/画质；雾/Bokeh/SMAA/最终输出算法与组件验证 |
| [rhi-postprocess.md](docs/contracts/rhi-postprocess.md) / [rhi-integration.md](docs/contracts/rhi-integration.md) | RHI图集/后处理资源图；生产快照/同步/生命周期/测量接线 |
| [performance.md](docs/contracts/performance.md) | PERF-01–07、预算来源、采样/统计和结果解释 |
| [test-gates.md](docs/contracts/test-gates.md) / [test-window-policy.md](docs/contracts/test-window-policy.md) / [verification-artifacts.md](docs/contracts/verification-artifacts.md) | 门禁分组；验证窗口；输出/Git边界 |
| [source-defect-exceptions.md](docs/contracts/source-defect-exceptions.md) | 原项目缺陷/不合理行为的预期判定与跳过记录 |
| [source-fidelity-review.md](docs/contracts/source-fidelity-review.md) | 逐历史轮次来源、适配/补偿判定与覆盖索引，非实施规范或运行日志 |
| `docs/platforms/{windows,macos,linux}.md` | 平台操作/部署差异与执行入口；欠验归属和门槛集中在本计划后置专项 |

### 工具链与目录

- 最新原生Kit发现、ABI/模块闭包、同Kit工具、私有API编译探测和C++23能力均执行[native-bootstrap.md](docs/contracts/native-bootstrap.md)，不得固定Qt版本/安装目录或降级。
- Canvas Painter共用API、生命周期和选型执行[canvas-painter.md](docs/contracts/canvas-painter.md)；按实际等价性、质量、成本和维护性选择，缺能力明确定位，不因许可类别/技术预览标签放弃合适实现。
- 根CMake/presets；C++在`src/{app,core,motion,render,media,library,services,platform}`，QML在`qml/Yalahoo/Music/{components,pages,panels,scene}`，资产`resources/`，测试`tests/`，工具`tools/{build,assets,review,deploy,audit,launchers}`。普通启动器在根，演示/研究启动器在`tools/launchers/`；不提前创建未来轮次的空实现。
- `src/`子目录维护源清单；共用代码优先Qt，平台目录只放真实差异。QML用模块相对别名和`loadFromModule("Yalahoo.Music","App")`，非QML资源qrc `/assets/`，shader用qt_add_shaders/qsb；不依赖工作目录。构建`build/{release,debug}`、部署`out/{windows,macos,linux}`；本机配置/路径不提交。
- 提交前依次执行`format`、`check-format`及适用Release检查，`source-format`必须通过；QML诊断只能用CMake官方`Yalahoo_qmllint`/`ui-audit_qmllint` targets。四空格、展开语句和冻结/第三方排除范围由code-style契约定义。

### 架构与产品接线

- 全部现有及新增QML控件遵守[controls.md的长期强制规则](docs/contracts/controls.md)：禁止QQC预制控件，完全自绘并按冻结来源理论值显式设置视觉属性；产品、探针、研究与Review同样适用。具体边界只在该契约维护。

- AppController组合模型、选择/展示/运动、媒体、偏好与Provider，禁止巨型业务类；稳定ID操作，导航与播放独立。模型通知在GUI，GUI→render仅不可变SceneFrame，render不访问QML/LibraryStore/IO；扫描/解码用受限worker，媒体探测用有事件循环的专用线程，异步按requestId/generation/entityId淘汰陈旧结果。
- 数据根/JSON/排序/搜索/Provider协议以library-data契约为准。专辑/曲目ID固定为`album-/track-`加原生绝对目录/直属单曲/文件路径UTF-8的SHA256前24位；跨系统不猜映射。旧数据仅显式只读导入，不移动原目录或导入PCM缓存，不写源歌曲标签。
- SceneAdapter/QML接口稳定，生产唯一RHI SceneItem；Quick3D仅Review，私有API留在RHI/字体适配边界。通道/格式/ABI与资源图分别由rhi-opaque/shell/postprocess/integration维护，接口/调用链与契约同步维护，不再重选框架。
- 源材质/几何/光场、局部LOD、原contain/透明留白、帧源/打断/减少动态与状态机均按专项契约；不得建立第二套常量/时间/播放/身份机制。跨API换算必须有源公式/单位依据。
- 生产滑尺用C++ Canvas Painter，标题当前glyph+彩色Quick Text；序章实现见opening.md，R34/R35按后端分工完成最终选型。列表/输入/焦点/Accessible由Quick承担；固定有界池/封面队列/上传预算、ResourcePlan及异常失败路径按对应契约，不以局部benchmark豁免设计缺陷。

## 验收要点

- 匿名fixture覆盖空/单/奇偶/短长列/超过12张列/超过432库、横竖方/alpha封面、长中文/下伸英文/括号/emoji/组合字。Web/Qt共用viewport/DPR/theme/quality/arrayMode/IDs/selection/motionTime/事件JSON，不读私人库；Review冻结时钟CLI不进入普通产品。
- 尺寸1920×1080、1280×720、995×858、960×900、930×900、390×844、320×720、2560×1080、3440×1440，代表DPR1/1.5/2；布局锚点≤1逻辑px、三维投影≤2逻辑px、double运动≤1e-6，时序≤一个60Hz帧。20/30/60/120fps和长帧，轨迹以1/120秒源状态采样。
- 打断覆盖立即返回/退字连选、三次换片与反向、搜索替换/取消、总览切列、改速/途中减少动态、扫描替换及跨1.05分界；真实清晰度/轮廓/合成、功能、数学和资源分别验收。
- 性能指标、采样/临界复核、预算和历史record-only例外统一见performance.md；后台视觉停止且歌曲继续，资源压力执行范围见test-gates.md。

### 严格静态审计

工具置`tools/audit/`，输出只留本地。每平台矩阵含checkId/requirement/platform/sourcePath/lineOrSymbol/evidence/result/limitations和内容hash；自动检查后必须推演实际调用链/条件分支/错误路径，字符串命中或文件清单不足以证明实现。N/A须说明原因，源码可确定缺陷必须修复，实际与静态结果分开。

| ID | 审计职责 |
| --- | --- |
| COMMON-01 | 顶层CMake到目标/源/链接/moc/qrc/qsb/安装/测试完整闭包；C++23/最新Kit；禁止空平台分支、#error或静默关闭产品功能 |
| COMMON-02 | QFile/QDir/QUrl/QStandardPaths的Unicode/空格/大小写/分隔符/可写根；模块/qrc/字体/图标/模型/音频/插件到部署读取的解析链 |
| COMMON-03 | 相机/clip/Y/深度、MSAA/resolve、透明排序、屏幕采样、binding/location和qsb各目标；当前可执行的跨目标生成必须执行，不能冒充目标GPU运行 |
| COMMON-04 | 命令到公开媒体API的source/设备/错误/结束/取消/销毁、不可seek/未知格式/后台恢复，禁止模拟后端替代真实产品 |
| COMMON-05 | 每项证据可追溯；功能、构建/资源、平台、部署缺陷与未实现路径修复；缺实机单独交接，不能以静态通过冲销失败 |
| WIN-01 | MSVC/入口/PE与ICO、windeployqt/QML/CanvasPainter/媒体/运行库、启动器自身路径/参数/退出；不伪称生成/运行PE |
| MAC-01 | bundle/Info.plist/Resources/Frameworks/PlugIns/qml/macdeployqt、Metal/Retina/Command/菜单/关闭/数据根、.command引用 |
| LNX-01 | xcb/Wayland、默认Vulkan探测回退及显式GL、库/插件/媒体依赖、LF/可执行位/参数/XDG/权限错误；不静默改系统配置 |
| PERF-01–07 | 按performance契约展开复杂度/背压/快照上传/资源图/缓存/QML输入/生命周期，覆盖本轮与受影响路径 |

## 跨轮次经验

仅在修改对应领域时读取；以下每条只表达触发条件及已确认核心方案。

### 文字与布局

- 源文字探针请求字号与computed字号不一致 → 在测量前以相同CSS/children替换整个probe根并重绑定引用，保留原粗搜/细搜算法。
- 主题切换使彩色字形节点丢失 → 普通文字保留整行glyph ID/位置，彩色grapheme交Qt Quick Text/Regular；GUI只传字体描述和值，render重新取得QRawFont并核对物理表。
- 整行glyph关联将行末空白并入前组 → text范围与cursorToX终点同时限制到inkEnd；保留整行shaping，不拆字测宽。
- 混合字体较大ascent使普通字形下移 → line.setPosition减实际line.ascent×局部scale；彩色描述符补回回退与主字体ascent差，按QFixed的1/64逻辑px核对。
- macOS选中`.Apple Color Emoji UI`而源使用`AppleColorEmoji` → 控制器/QML创建前公开API指定`Apple Color Emoji`；缺字体显式失败并回滚注册，比较实际run表而非重新查询的描述符。
- macOS主字体CSS半行距小数导致字体框基线偏移 → 主字体半行距按逻辑CSS px取floor，保留分数字号/行高、整行shaping、混合ascent和彩色基线；不外推Windows/Linux。
- 隐藏ReelInk/Canvas候选仍处理文字 → GUI请求核对有效可见性，render同步保存item/window可见状态，隐藏paint不遍历/排版/绘字；层同时核对窗口，恢复重发状态、换窗口断旧连接，资源由render释放。
- 自然宽度/active/visible同步通知形成绑定重入 → 宽度仅实际变化通知；caption按内容/退出状态保留，hide立即停advance并合并一个对象所属queued finish，恢复取消旧完成。
- 测量读到未polish的页眉状态 → 在提交下一状态前或afterSynchronizing读取已布局值；ToolTip/Accessible由QML直接读取，QPointer跟随控件销毁。
- ListView header变化/模型替换导致顶部或委托错误 → 分类模型禁跨类型复用；布局后positionViewAtBeginning，准备期合并顶部保持、用户滚动/定位即取消；按QQuickItem视觉树统计虚拟委托。

### 验证驱动与生命周期

- PowerShell启动WIN32 GUI读到陈旧LASTEXITCODE → 用Process对象等待并读取本次ExitCode，同时检查真实报告；console工具仍单独核对退出码。
- headless CDP的页面Runtime请求在受限环境不返回 → 使用独立页面连接并在具原生服务权限的环境运行同一导出入口，保持headless/外部请求拦截，不改产品算法。
- 标题恢复仍沿用超宽页眉root而停帧 → 恢复前明确设置标题window/root尺寸，以正常Qt事件与单个有界在途读回驱动。
- 隐藏验证期间其他应用切换导致foregroundUnchanged=false → 同时记录初始/当前/自身PID并监测AppKit/NSWorkspace自身激活与窗口焦点历史，只将验证进程占据前台/激活/焦点判为干扰，外部切换单独计数，未知PID继续失败。
- macOS最小化恢复的原生动画晚于固定验收定时器 → 以真实恢复帧及场景终态完成验证，定时器只作有界失败看门狗，分别核对每次恢复首帧丢弃与暂停状态。
- macOS验证窗口仅Qt flags仍可能激活 → QGuiApplication前设置启动策略；默认Prohibited、原生低层级/拒key/忽略输入并观察重建，授权前台则预先Regular，统一走VerificationWindow。
- 原生item已使用显示逻辑viewport却再次套参考舞台比例 → 从冻结生产宿主最终CSS核对外层变换，已无变换时传stageScale=1；按实际target DPR/附件与最终合成验证，不提高全局画质。
- RenderControl的高DPR图像占错误区域或隐藏控件不消费状态 → 显式设置target DPR，Title/Header/Transport/Canvas接offscreenRendering，仍消费生产FrameClock。
- RenderControl同线程读回在endFrame内完成且后续会销毁窗口 → 完成结果强制queued交GUI，待当前RHI帧返回后再重建，旧窗口结果以epoch拒绝。
- Metal静态场景异步读回无后续帧 → 显式采集按MaxAsyncReadbackFrames+1有界续帧、最多两批在途，不重复submit或轮询；无采集回调时不增加生产工作。
- macOS OpenGL shader/context不兼容或可见drawable需主线程 → QGuiApplication前选择4.1/CoreProfile，显式GL用basic、Metal保持threaded；不补旧GLSL近似或降Qt。
- macOS缺glTexStorage2DMultisample入口 → 能力探测后改用同采样数/同格式QRhi color renderbuffer并保留resolve，核对backingFormat与资源清单；Quick3D Review按有效context的函数能力校正其纹理MSAA标志，走Qt既有等价分支并在render线程核对附件，适配归materials契约。
- basic的surface重建未产生新context或静止SceneItem不再提交 → 可见恢复排对象所属ensurePolished/update，分别验证实际三维提交与context；完整释放需销毁最后QQuickWindow，旧窗口回调按epoch丢弃，清理帧在有效context执行。
- Quick3D Review仅复用旧root重新initialize后漏画 → 销毁拥有场景的QML root，再释放离屏上下文/窗口并重建场景；同时核对真实invalidation和有效输出，不混称同对象恢复。
- 封面数量与呈现状态跨polish帧混读 → ready核对同revision实体objectId/albumId集合、租约集合及选中资源；不放宽数量上界。
- 基本渲染循环复用同对象surface而探针要求新renderer → 明确销毁完整验证窗口/根，重绑策略与输入/遥测并以epoch拒绝旧回调；同对象恢复另验，不把局部generation当全局身份。
- 新renderer的resourceGeneration从1重置 → 用真实sceneGraphInvalidated事件及恢复输出/上传证明重建，不把局部代次当全局身份。
- 几何探针绕过像素保护或把错误提示算作模型 → 共用RenderQuality保护、记录limited/实际尺寸，要求renderError为空并排除提示区域。
- Offscreen附件估算漏D32FS8或resize重叠 → RGBA8颜色4bytes加depth/stencil8bytes保守上界并计旧/新同时存活；估算与driver实分配分开。
- QML视觉树重建时延迟闭包引用已销毁上下文 → 延后动作绑定所属对象方法并核对revision；页签指示条沿实际按钮几何变化同步，不排捕获旧页签的闭包。
- Qt Quick Popup在统一退出状态结束后仍保留模态焦点/捕获 → 关闭样式默认enter/exit，仅由既有SurfaceTransition控制可见生命周期。
- 虚拟歌单远距离滚动的originY或高度估计变化 → 读取原生首尾范围，在有界已实现项polish后按实际目标行转换坐标，保持源cubic目标距离，并按revision排队聚焦。

### 三维与数值实现

- 减少动态时旋转归位副本无法回收 → yaw归零、清returnY、lift.finish(0)并当次发布回收；选中盒手动查看目标仍独立保留。
- 全局detail无法表达阵列/选中/归位材质 → CaseClarity维护逐实体REVEAL曲线与ready，归位副本保存自身clarity按exp(-9dt)回雾，阵列保持磨砂；只更新对应uniform。
- 辅助参数测试正确而实际上传含无依据ROI系数 → 沿实际override/upload核对每实体uniformScratch，移除全部无源补偿；Review拒绝校准输入，不能只测helper。
- GL奇数宽R16F读回受PACK_ALIGNMENT填充影响 → 仅显式未对齐捕获同格式复制到按实际pack补齐的缓存，按storage stride读取逻辑宽；产品目标/精度不变、正常无捕获不做该工作。
- SSAO深度/核与源不一致或存在闲置上传 → RGB24白哨兵精确判断、法线只unpack；kernel仅创建时写入、动态80-byte前缀、删除未采样纹理，保留16/32/64累加顺序与源seed417。
- visibility与Equal壳体pass位置不同导致漏画 → 两个geometry顶点统一`p=model*vertex; nativeVP*p`且声明invariant，只有这两个shader使用MSL2.1；不加depth epsilon/偏置掩盖。
- roughness导数因相机roll变化而不等价 → 计算源view-space normal最大分量导数，不能替换为world-space无穷范数；LTC奇点独立验证。
- alpha黑边断言混用连续值与RGBA8码值 → 使用原整数码值门槛`qRound(expected*255)-1`，不扩大容差。
- 多个Capture字段别名到同一纹理 → 按纹理身份去重读回/转换并计真实回调数，最后一次统一交付共享QImage；不能按语义字段数等待。
- shader生成丢失后续适配 → 核对完整生成token与依赖，覆盖basic/early变体，生成后format可复现提交内容；纸面生成保留entityLight0.xy原点场提升。
- 倾斜平面depth误差与投影门槛单位不同 → 保留原depth差并按解析平面每物理像素梯度换算≤2物理px；恒深面仍检查2e-6，法向/颜色门槛不变；远平面射线用解析camera ray避免端点相减消减误差。
- 地面探针取景使接收阴影全在视口外 → 专项相机aim=(0,-4.63,0)，强制开启阴影场景实际occluded>0，生产相机不改。
- 源SMAA宏`float(ivec2)`被误作二维偏移 → 取首分量后广播：(1,0)两轴加1，(0,1)不增加轴并保留调用前y减1；严格按GLSL展开，不改shader迎合错误测试。

## 待执行轮次

### R34 · 非OpenGL后置补修与实机验证

- 依赖R33阶段交接。按下方专项分工表执行D3D11、Metal、Windows显式D3D12及三平台实际启用的Vulkan，以及各平台非OpenGL的构建/媒体/部署事项；每单元按目标/DPR/窗口或性能边界执行。
- 入口与门槛引用“后置专项输入与退出条件”，公共检查引用test-gates.md、test-window-policy.md及performance.md。共享修正回归当前主力平台受影响组件，范围按test-gates.md选择；适用PowerShell工具必须在目标真实解析/运行。
- 缺环境或采集不可靠的事项保留具体任务；全部本轮任务完成才删除本节。阶段收口不表示R35已完成。

#### 后端选择与日志目标系统检查

- 输入为GraphicsBackend/BackendCapabilities、LoggingSession及Release `graphics-backend`/`logging`/`logging-terminal`。Windows缺原生系统/Qt Kit/驱动与控制台；macOS当前Qt Kit未编译Vulkan，缺启用该特性的同版本原生Kit、SDK头及MoltenVK；Linux缺原生系统/Kit/驱动/终端。无GPU选择检查不证明运行支持。
- Windows分别显式选择d3d11/d3d12/vulkan，macOS显式metal/vulkan，Linux显式vulkan；Qt特性缺失、设备/格式初始化失败时显式请求必须非零退出并输出原因。成功目标复用各专项原匿名输入、质量/资源/生命周期门槛，核对实际QRhi API；不能将默认API成绩外推为其他后端通过。OpenGL仍归R35。
- 三平台运行上述日志定向测试，按logging.md检查实际日志目录、保留期、轮转及会话总量上界；Windows终端核对VT黄/红/默认色和UTF-8输入输出代码页，管道/文件无自动颜色序列，GUI父控制台连接保留已有重定向。日志异常须继续控制台并明确错误。缺目标环境不记通过。

### R35 · 三平台OpenGL集中验证与补修

- 依赖R33实现交接和R34非OpenGL阶段审计；作为最后的专门轮次，按平台/设备/DPR/组件拆单元，集中执行三平台OpenGL兼容后端，包含Linux显式OpenGL的X11/xcb和Wayland会话。
- 按下方分工表保留原任务ID、输入、修复对象、准确入口、原门槛和回归依赖。使用最新原生Kit的Release，显式`QSG_RHI_BACKEND=opengl`或对应工具`--backend opengl`，确认实际后端；macOS上下文/basic适配见平台文档，窗口选择见test-window-policy.md。
- 源码可确定缺陷直接修复；共享修正回归受影响默认生产后端，范围按test-gates.md选择。缺真实系统/Kit/GPU/窗口的单元保留具体缺口，全部三平台适用独立覆盖项及必要组合完成后才删除本节。

## 后置专项输入与退出条件

本节是R34/R35共用的单一任务规范，不是额外执行轮次。每个任务按下表分配目标，使用对应平台的最新原生Kit；原任务ID、阈值和输入只在下文维护。已有实测以source-fidelity-review.md为准，不能外推到未运行目标。

| 专项 | R34执行范围 | R35执行范围 |
| --- | --- | --- |
| HDR-OUTPUT-P | Windows/D3D11、macOS/Metal、Linux/Vulkan的HDR/SDR原生输出与系统通知 | 三平台显式OpenGL的共享变更SDR回归 |
| BP-P | Windows/MSVC与Linux原生Kit的严格字符串编译、翻译/模型/资源与部署回归；Windows PowerShell工具真实目标运行 | 三平台OpenGL的共享读回/资源提交与受影响QML回归 |
| R31-P | macOS独立包在其他目标设备上的Finder/菜单/媒体/物理输入/跨屏及Metal正常呈现与性能 | macOS部署包的OpenGL图形/窗口/输入/性能回归 |
| R32-P | Linux原生最新Kit/C++23构建、ELF/独立目录部署与干净系统、非图形模型/媒体/设备及实际启用Vulkan的图形/窗口/性能 | Linux显式OpenGL及部署包的X11/xcb、Wayland图形/窗口/输入/生命周期与性能 |
| R30-P | Windows原生MSVC/最新Kit构建、windeployqt/PE/运行库与干净环境部署、D3D11产品GUI/媒体/性能 | Windows部署包的OpenGL图形/窗口/输入/性能回归 |
| UI-CONTROLS-P | Windows/D3D11与macOS/Metal原生控件/物理输入、输入法/无障碍/跨屏；三平台目标构建与模块闭包 | 三平台OpenGL控件与宿主窗口/输入回归 |
| R21-P | Windows/Linux原生文件系统、缓存与非图形扫描回归 | — |
| R27-P | Windows/D3D11与macOS/Metal的物理实验控件输入、原生恢复/跨屏DPR和播放中改速；三平台非图形参数/启动器/构建 | 三平台OpenGL实验面板、参数上传、采样和生命周期回归 |
| R26-P | Windows/D3D11与macOS/Metal的原生设置输入/焦点/跨屏DPR、实际播放中视觉重载；三平台非图形偏好/Provider/配置与构建 | 三平台OpenGL设置GUI、滚动/焦点/DPR与窗口/视觉重载回归 |
| R25-P | Windows/Linux原生BGM/音效/偏好与三平台物理音频路由、听感、最小化播放、非OpenGL音频面板 | 三平台OpenGL声音设置GUI/窗口/输入回归 |
| R24-P | Windows/Linux原生媒体与三平台物理设备/听感、非OpenGL播放器/输入集成 | 三平台OpenGL播放器GUI/窗口/输入回归 |
| R23-P | Windows/D3D11与macOS/Metal的原生目录选择、物理输入/焦点、跨屏DPR及播放定位集成；三平台非图形构建/模型 | 三平台OpenGL的面板/定位/资源恢复与Linux窗口/输入 |
| R22-P | Windows/Linux公开媒体无声探测、封面与显式导入/偏好JSON | 仅共享封面变更受影响的OpenGL图形回归 |
| R19A/B/C/D/E各`-P`、R16M3-S、R17-P/R18-P/R19-P、R20-P | Windows/D3D11；macOS/Metal物理显示/输入与其他目标设备；各平台不创建OpenGL上下文的目标构建/算法检查 | Windows/macOS OpenGL受影响回归；Linux/OpenGL全部目标GPU、X11/Wayland窗口、输入及生命周期 |
| R16B2W | Windows/D3D11文字 | — |
| R16B3 | — | Windows/Linux OpenGL文字选型及macOS受影响回归 |
| R29-P | Windows/D3D11整场GUI与macOS/Metal尚欠的物理输入/跨屏/原生呈现；三平台非OpenGL构建/模块 | 三平台OpenGL整场GUI、窗口/输入/生命周期与质量回归 |
| R28-P | Windows/D3D11与Linux实际Vulkan的冷启动/生命周期/呈现/输入/资源峰值；macOS其他目标设备、物理显示/输入及呈现间断复核；Windows R10D噪声复现 | 三平台OpenGL的同项产品/窗口/性能与资源压力 |
| MAC-VISIBLE-3D、各平台最终实机矩阵 | D3D11/实际Vulkan的完整图形/窗口/性能；Metal物理显示/输入、其他目标设备与呈现间断复核；非OpenGL媒体与部署 | 三平台OpenGL的同项动态矩阵和部署启动 |

各单元的配置/格式/官方qmllint/回归范围统一见test-gates.md，COMMON/PERF与平台审计执行本计划“严格静态审计”。目标检查优先使用可靠隐藏入口；具体测试和数值门槛见各项。原生同对象窗口恢复、SG失效、完整root重建、正常VSYNC/物理输入分别记录，未知GPU完成/显示/driver显存值保留null。没有有效样本或缺目标环境不算通过。

积欠项按下表合并执行；组合选择、结果有效性及缺选择器的处理唯一按test-gates.md，性能触发与采样唯一按performance.md。每个任务保留自身断言和真实目标缺口，同一份有效证据按覆盖项分别销项。

| 共用执行单元 | 对应待办 | 仅补的独立事实 |
| --- | --- | --- |
| 构建/部署及产品正常呈现 | R30-P/R31-P/R32-P、R28-P、R29-P、MAC-VISIBLE-3D、最终实机矩阵 | 包外加载/干净系统、物理输入/显示/媒体；部署未改相关运行依赖时复用已有呈现/压力，不重跑产品长矩阵 |
| 音乐与声音面板 | R24-P/R25-P、UI-CONTROLS-P对应宿主 | playback-qml一次覆盖双方共有GUI断言；歌曲/BGM/音效各自路由、格式、听感和设备错误仍独立 |
| 设置/实验/资料面板 | R26-P/R27-P/R23-P、UI-CONTROLS-P、R29-P | 各宿主功能各一次，复用同源控件和原生宿主窗口恢复/跨屏事实；不同路由/取消/关闭断言不能省略 |
| 文字/页眉/滑尺 | R16B2W/R16B3/R16M3-S、R19E-P及R17-P | 来源数学、各绘制路径的字形语义/缓存各一次；相同DPR/页眉/窗口证据不按历史轮次重做，成本重复仅明确比较时开启 |
| 生产RHI连续输出 | R19C2C-P、R19D1-P/R19D2-P/R19D3A-P/R19D3B-P/R19D4A-P/R19D4B2-P/R19D4C-P | 同目标最终源码运行一次完整rhi-flicker序列，逐项覆盖各pass实际上传/有限值/覆盖/资源；各独立pass公式、格式和所有权测试仍保留 |
| 启动、场景和媒体生命周期 | R28-P与组件/部署相关积欠项 | startup-stress一次证明完整舞台换片/root压力；各独立renderer重建及原生同对象窗口恢复按真实所有者分别补验，不跨主题/DPR/计时批次重复 |

组件自动回放的剩余平台为Windows/D3D11、Linux实际Vulkan及R35的三平台OpenGL。macOS/Metal待办限各项明确列出的物理输入/显示、跨屏、其他目标设备、实际音频路由及R28-P呈现间断复核；这些目标仍使用原输入、入口与门槛。

#### HDR-OUTPUT-P · HDR目标系统与原生显示事件

- 输入为DisplayOutput/三平台DisplayMonitor、DisplaySurface、display-output.md与5×1颜色输入；需要Windows/D3D11、Linux/Vulkan及支持HDR的真实系统/最新Kit/GPU/窗口环境。本机跨目标qsb仅证明静态翻译，不能替代目标编译/运行。
- 入口：Release构建Yalahoo/DisplayOutputTest，官方Yalahoo_qmllint/ui-audit_qmllint；`ctest --preset qt-release-render -R '^display-transfer-gpu$'`；获得当轮可见授权后`ctest --preset qt-release-visible -R '^display-output-visible$'`。只运行本专项的小窗口和颜色输入，不隐式启动整场矩阵/性能/阵列。
- 目标门槛：Windows实测HDR开关、跨HDR/SDR屏、SDR content brightness slider更新及正确nits/80，WCG SDR不误判；Linux在Wayland颜色管理协议与PQ/linear实际swapchain下核对reference white/target luminance通知、X11/无协议SDR及Vulkan初始化失败回退；macOS真实亮度/电源导致current EDR变化及跨屏恢复。查询失败不使用旧HDR状态，参数更新不重建，模式变化保留QML/音频状态，所有者/epoch/关闭释放及可见授权结束恢复默认策略有效。当前系统设置的实际通知、物理显示亮度和跨屏事件须实机操作，不用受控Monitor状态替代。
- 全平台显式OpenGL继续实现且保持SDR；其HDR相关共享变更回归归R35，原OpenGL门槛不减少。HDR专项不隐式触发全量/大型阵列或性能A/B；性能按既有PERF专项独立验收。

#### BP-P · 语言与框架实践的目标补验

- 输入为根CMake永久编译闸门、`resources/i18n/`、Localization、GenreRowsModel/OptionMetrics、LibrarySession/OfflineMetadataProvider、TrackFocusController与RHI ReadbackImage/资源提交闭包。当前缺Windows/MSVC和Linux原生Kit/系统/GPU/媒体环境；主机编译、qsb、PowerShell解析或合成报告检查均不替代目标事实。
- R34沿R30-P/R32-P原构建部署入口，以最新完整Kit/LinguistTools的Release实际编译全部适用目标，执行受影响的localization、library-data、track-focus、cover-cache、rhi-material-parameters、directory-scanner、media-metadata及模块/格式/官方qmllint检查；核对中英文及英文复数、Unicode数据路径、异步取消/替换与退出。Windows还须真实执行受影响的verify-r10d/r14/r15/r16与detail/overview工具，验证所选Kit DLL/插件、当前报告协议和真实Process.ExitCode；R10D数值/噪声结论仍依其原专项，不能以工具运行替代。
- R34对D3D11/实际Vulkan、R35对三平台OpenGL复验共享读回格式/步幅/通道/非有限错误、资源批次提交/失败清理及受影响的文字/面板/定位/隐藏恢复；复用各组件现有匿名输入和原质量/资源/生命周期门槛，不添加图像评分，不将源检查记为目标GPU通过。缺目标或可靠样本继续保留本任务，共享修正回归主力Metal对应组件。

#### R32-P · Linux生产部署、媒体与X11/Wayland实机验证

- 依赖R33阶段交接和R32交付：`cmake/Deployment.cmake`、`GraphicsBackend.h`/`BackendCapabilities`、`DeploymentProbe`、`tools/build/linux.sh`、`tools/deploy/linux*`/`verify-linux.py`、根普通启动器和`docs/platforms/linux.md`。输入为实际生产QML清单、qrc品牌/字体/CanvasPainter/shader/音频及既有匿名9×64库、支持媒体样本和StartupAudit fixture。当前缺Linux系统/原生编译器/最新完整Kit、ELF执行、GPU、X11/Wayland会话与物理音频设备；主机macOS的Release/模块/源码/工具检查不替代目标事实。Linux打包/动态加载成功路径和Vulkan编译分支尚待目标执行。
- R34非图形入口：按Linux平台文档真实执行`sh tools/build/linux.sh`和受影响fast检查、`sh tools/deploy/linux.sh`、`python3 tools/audit/audit-linux-deployment.py --build-directory build/linux-release`及包外新目录`python3 tools/deploy/verify-linux.py --output 绝对新目录`。核对最新Kit/C++23与官方qmllint/同Kit qsb、compiled QML导入清单、ELF架构/全部ldd依赖/相对RPATH/qt.conf、xcb/Wayland/Qt随附媒体/CanvasPainter闭包、许可/可执行位/LF。失败须保留旧包；真实当前退出码与当前report均通过，不以帮助或文件存在代替加载。包复制到无开发Qt的兼容CPU/系统ABI及中文/空格路径重验，实际加载Qt/媒体库不得借用外部Kit。隐藏offscreen QPA探针只加载QML/媒体，无GPU上下文/窗口呈现证明。
- R34非图形修复对象：原生大小写/Unicode/空格/只读权限与XDG数据根、扫描/异步缓存/只读导入/离线Provider、三路公开媒体播放/暂停/停止/seek/上下曲/自然结束/格式及权限错误、默认/显式设备消失恢复、后台继续和关闭停止。入口及匿名输入沿R21-P/R22-P/R24-P/R25-P；Qt动态支持格式与元数据索引分开，实际设备/听感不能由无输出Loaded样本代替。不得安装包/改驱动/音频配置或写用户歌曲。
- R35负责显式OpenGL在真实X11/xcb与Wayland会话的产品/组件/部署图形、输入、DPR/跨屏、原窗口hide/minimize/restore、十次SG/视觉重建与全部原质量/资源门槛。修复对象包括实际C++滑尺/二维序章的透明裁切/轮廓、glyph/Quick Text与emoji、三维阵列/玻璃/详情/总览/片头及物理触控板pixelDelta/输入法/焦点；生产无QtCanvas2D导入。依Linux平台文档和对应R19A–E/R16/R17–20/R29专项运行现有Release Review入口，显式`QSG_RHI_BACKEND=opengl`并记录真实API/会话/尺寸/DPR1/1.5/2；原几何≤2逻辑px、布局≤1逻辑px、double运动≤1e-6、颜色/附件/样本/上传/资源断言不放宽。不能用隐藏target DPR或完整root重建冒充物理跨屏/同对象窗口恢复。
- 若Linux Kit/设备支持Vulkan，R34以`QSG_RHI_BACKEND=vulkan`在两个真实会话执行相同受影响组件及按test-gates.md选择的产品覆盖，核对实际API，不能将回退OpenGL算Vulkan通过。另以不可用`QT_VULKAN_LIB`分别运行默认普通启动/帮助检查警告与OpenGL回退，以及显式Vulkan请求非零退出和明确错误；初始化成功仍须验证surface/swapchain/格式/MSAA/resolve/所有权。缺目标Vulkan能力保留具体缺口，不假造运行。
- 正式产品性能沿R28-P及performance.md入口：正常VSYNC正式矩阵、未限速诊断与独立资源压力的选择、次数及指标统一按performance.md，保留输入到Qt提交、准备时延与资源峰值事实；分别记录GL/Vulkan、xcb/Wayland、实际DPR/刷新/窗口策略。driver显存/GPU完成/显示时刻未知仍为null。性能不能从Metal/D3D11或离屏成本推断；缺可靠样本继续保留。
- 退出：Linux实际构建/依赖/非图形/媒体/部署以及两会话对应API的质量、输入、生命周期和性能原门槛有可靠目标证据；未执行项不记通过。共享修正按test-gates.md回归主力Metal对应组件、格式/官方qmllint及默认隐藏策略，其余目标不外推。

#### R31-P · macOS生产包的物理设备与完整产品实机验证

- 依赖R33阶段交接、R31部署/菜单实现及可靠macOS目标设备、显示会话、匿名音乐和实际音频路由。输入为`cmake/Deployment.cmake`实际QML清单、Info.plist/ICNS、`src/platform/macos/ApplicationMenu.mm`、普通启动器、`tools/deploy/macos.*`、`verify-macos.py`与既有生产fixture。缺口为另一台目标系统、物理屏幕/键鼠/触控板/跨屏事件、设备拔插/听感及正常原生呈现的可靠采集。
- 静态入口`python3 tools/audit/audit-macos-deployment.py`绑定COMMON-01–05/MAC-01/PERF-01–07和三平台symbol/hash；构建/部署及用户操作清单唯一入口为`docs/platforms/macos.md`。在目标以`sh tools/deploy/macos.sh`生成`out/macos/`，从包外全新目录运行`python3 tools/deploy/verify-macos.py --output 绝对新目录`；复制整包到另一台目标与中文/空格路径后重复。真实进程退出、当前report、所有实际加载映像/架构/相对资源/签名须有效；不复制开发Kit，不以帮助或普通退出0认定功能通过。
- 修复对象：Finder品牌/普通.command参数与cwd、AppKit菜单/系统目录对话框编辑焦点与输入法、片头输入锁、真实关闭/后台媒体、Retina跨屏后SceneItem/Canvas/文字的尺寸/缓存/透明裁切与生命周期。用隔离`--data-dir`和匿名`--music-root`检查浏览/详情/总览/片头/玻璃、三路公开媒体的播放/暂停/seek/换曲/错误、系统默认与显式设备/消失恢复、最小化继续及关闭停止；不改用户歌曲。物理Command快捷键、同窗口hide/minimize/restore、跨屏DPR1/1.5/2与十次视觉/SG重建的原门槛保留，隐藏Qt事件/target DPR/完整root重建分别不能替代对应物理事实。
- Metal归R34；部署包显式OpenGL的同项图形/窗口/输入/资源回归归R35。正常VSYNC/未限速、冷启动/输入/准备/资源峰值按R28-P、MAC-VISIBLE-3D与“各平台最终实机矩阵”的正式场景、独立资源压力及数值门槛执行；范围与复用统一按performance.md/test-gates.md，不能用本机隐藏成本或资源估算替代。独立听感/物理设备按R24-P/R25-P，不重复维护阈值。
- 退出：目标部署/真实质量、物理菜单与输入/显示/媒体/生命周期及适用性能门槛有可靠证据；缺样本仍保留原任务。共享修正按test-gates.md回归当前Metal受影响组件、默认隐藏策略、格式与官方qmllint；Windows/Linux回归保持R30-P/R32-P及后置分工，不将主机qsb翻译外推目标运行。

#### R30-P · Windows生产部署与干净环境实机验证

- 依赖R33阶段交接、R30部署实现及可靠原生Windows/MSVC/最新完整Qt Kit、D3D11 GPU/显示会话、匿名音乐输入和实际音频设备。当前缺Windows系统/编译器/Kit/PE执行能力；目标脚本成功路径尚待执行。
- 输入为`cmake/Deployment.cmake`生成的实际应用QML清单、`cmake/yalahoo.rc.in`、R03B品牌manifest及ICO/PNG/SVG、qrc资产、匿名库/可支持媒体样本；源码审计入口`python tools/audit/audit-windows-deployment.py --scan-qml --build-directory build/windows-release`，输出COMMON-01–05/WIN-01/PERF-01–07的symbol/hash矩阵。核对完整CMake/C++23/最新Kit/RC/PE入口、生产Review OFF及Quick3D隔离、QtQuick/Templates/Dialogs/Multimedia/CanvasPainter/Qt随附媒体依赖/字体/shader、Unicode与可写数据根、三路公开媒体与设备/错误/退出调用链。源码可确定或目标运行发现的缺陷必须修复，不以本机qsb翻译或脚本解析宣称目标通过。
- 准确入口及用户清单为`docs/platforms/windows.md`：先以`pwsh -NoProfile -File tools/deploy/windows.ps1`真实configure/build/windeployqt生成`out/windows/`，核对同Kit工具版本、dumpbin import/delay-import解析、架构匹配官方VC Redistributable、相对qt.conf、许可证及包清单。核对失败保留旧包、无私人库/缓存/PDB/研究工具/比较模块、不复制整个Kit；实际检查PE GUI资源与Explorer/窗口/任务栏大小图标。审计PowerShell成功/错误/空格Unicode参数/Process.ExitCode及包优先/开发回退启动器，不仅检查文件存在。
- 在无Qt开发PATH/Node/外部FFmpeg的临时环境执行`out/windows/verify-windows.ps1`的help、browse/navigation/detail/overview、demo和normal，自动验证要求当前退出码与当前report均成功；再在未安装Qt/开发工具的干净Windows上重复，安装随包官方运行库，排除机器依赖掩盖。正常启动检查离线/空库/匿名demo/Unicode音乐目录、字体、设备列表、支持样本、播放/暂停/seek/切歌/输出设备变化、错误恢复及后台继续播放；滑尺和二维序章实际CanvasPainter透明合成/裁切/shader/重建必须有效，不能仅凭导入成功。原生物理输入、正常呈现/VSYNC、尺寸/DPR/跨屏及听感分别证明；窗口策略沿test-window-policy，不未经当轮授权提升前台。
- 正式产品路径按performance.md及R28入口验证主要场景、冷启动/准备/输入时延和资源峰值，正式呈现、未限速诊断及独立资源压力统一按performance.md，不按部署阶段无条件重复矩阵，记录实际OS/GPU/Qt/D3D11/逻辑物理尺寸/DPR/刷新/VSYNC/窗口策略；Review工具单独构建，不混入产品、不转抄历史benchmark或macOS结果。Windows OpenGL同项动态运行归R35。适用格式/官方qmllint与受影响回归按test-gates选择；共享修正回归当前主力平台对应入口。退出须上述目标实际检查通过，缺环境或有效样本保留本任务，不以阶段交接销项。

#### R29-P · 整场GUI目标与原生呈现回归

- 依赖R33阶段交接与R29整场重放实现；冻结来源经历史映射，输入为`StartupAudit::fixture`的576匿名专辑及qrc横/竖/方封面、各组件源数学/结构输入。缺Windows/Linux目标系统/最新原生Kit/GPU/显示会话；macOS仍需物理输入/跨屏与其他目标设备。
- 目标Release Review ON实际构建后，运行`python3 tools/build/verify-gui.py --backend d3d11|metal|vulkan|opengl --output verification/全新目录`和`python3 tools/audit/audit-gui.py --report 上述目录/matrix/report.json`；后端按R34/R35分工，仅执行实际启用的Vulkan。完整矩阵/采集语义由rhi-integration.md维护，按test-gates.md分别覆盖九尺寸布局、双主题合成和两阵列状态，保持原画质/完整数量/动效/源效果。目标实际DPR另验，不能以隐藏target覆盖冒充物理屏幕。
- 原生自身窗口回放并检查头部/文字/滑尺/盒体/光带/背景/详情/总览/片头的正常清晰度、轮廓、裁切和合成；Qt动作、最终CSS结构/来源数学、实际输出质量分别验收。详情总览入口隐藏/返回恢复、竖屏返回按钮位于头部下方、退字连选/搜索隔离/片头交接、有限矩阵/容量/资源代次和最终释放通过。CSS锚点≤1逻辑px、投影≤2逻辑px、double运动≤1e-6、时序≤一个60Hz帧仍由各组件原入口保持，SDE-01/02按来源例外契约处理字号与空间不足时的导航换行；可容纳布局及其余门槛不变。
- 物理键鼠/触控板、原生同对象hide/minimize/restore、真实跨屏DPR/焦点与正常VSYNC按各平台最终实机矩阵和R28-P执行；遵守当轮窗口策略，不因遮挡提高前台。GPU完成/屏幕扫描/driver显存未知保留null。共享修正按test-gates.md回归当前主力平台受影响组件；无渲染/调度/资源算法变化不重复无关正式性能矩阵。保留COMMON/PERF/三平台符号/hash、实际OS/Kit/GPU/后端和原始报告，仅本地归档。

#### R28-P · 冷启动、原生生命周期与性能目标

- 输入/修复对象：StartupCoordinator、FrameClock、MusicStage、SceneItem/RhiItem、CoverCache/AtlasUploadQueue及既有ResourcePlan，含StartupAudit的共享呈现采样与销毁期回调寿命；`StartupAudit::fixture`生成9×64匿名库/96张独立2048² JPEG和静音WAV，禁止私人输入。依赖R28实现与原R10D设备/Kit/fixture。剩余完整目标为Windows/D3D11、Linux实际Vulkan及R35三平台OpenGL；macOS/Metal保留其他目标设备的冷启动/部署、物理显示/输入、跨屏、实际设备资源观测与下述呈现间断复核。Windows/Linux系统、目标编译器/GPU/显示会话与物理设备缺失，静态审计不能代替这些事实。
- 入口：目标最新原生Kit Release，`python3 tools/audit/audit-startup.py`，`ctest --preset qt-release -R '^(startup|motion|cover-cache|opening|playback|verification-window-policy)$'`；按默认或当轮显式授权窗口策略运行`python3 tools/build/verify-startup.py --backend metal|d3d11|vulkan --formal --mode vsync --scenario day-wave --output verification/新目录`；未限速仅按performance.md的诊断触发条件选择相关场景。OpenGL同入口显式`--backend opengl`归R35。普通隐藏功能入口为`startup-qml`；独立资源压力用`qt-release-stress -R '^startup-stress$'`或`verify-startup.py --stress`，原生同对象hide/minimize另加`--mode vsync --lifecycle`；三平台原生构建和Windows PowerShell适用门禁仍执行公共契约。
- 正式场景、采样协议、数值门槛及局部诊断结论边界唯一按performance.md。首次目标建立完整正常VSYNC基线，后续只补受影响且无有效结果的场景；短matrix或隐藏CPU成本不代替正式验收。DPR、其他画质/大尺寸、资源压力分别验证，不与长采样组合穷举。物理显示与input-to-photon无法获取继续null。
- 原生hide/minimize/restore、同对象surface/SG失效、独立完整QML root/窗口及换片压力、隐藏扫描合并/晚到解码、恢复首间隔/最新revision、暂停及播放中当前源/意图连续均须真实执行；具体次数与入口归performance.md，同一有效压力结果不随主题/DPR/计时模式重复；检查实际滑尺/文字/序章的输入、透明合成、缓存、资源回落。描述符峰值与driver/系统可获得峰值分别保留，释放压力不能凭generation重置判定。异常/样本不足/遮挡节流保留原始失败，源码可确定问题直接修复。
- macOS/Metal呈现间断待定位：`night-wave`正常VSYNC的一次120秒运行出现3个>50ms间隔，最长约2.48秒；最长间隔内GUI时钟推进154帧，RHI没有封面上传或资源代际变化，结束时应用失去前台。该次缺少连续曝光原始记录，不能自动判为环境噪声或通过。输入仍为`StartupAudit::fixture`，定位对象为`StartupAudit::Driver`、Qt窗口曝光/呈现调度与`SceneItem::frameSubmitted`时间链；使用上述入口的`--formal --mode vsync --scenario night-wave --foreground`在可持续曝光的目标会话复核，保留`qtPresentationTimestampsNs`、`nativeWindowEvents`和GUI/同步/CPU/GPU时间。核实曝光变化或真正停提的原因，按performance.md处理固定复核与原门槛；不以其余运行抵消失败，也不无限重跑。共享修正只回归受影响调度/窗口场景。
- Windows R10D的18.35/20.98ms及record-only事实仍未复现；按`tools/build/verify-r10d.ps1 -Backend d3d11`在原设备/Kit/匿名fixture运行（R35显式改用`-Backend opengl`），使用当前非像素入口保留原采样/噪声和原数值门槛，不能重跑已禁图像评分或写为已确认环境噪声。D3D11归R34、OpenGL关联回归归R35。
- 退出：相应目标COMMON/PERF、正常启动产品功能/质量/媒体、有效VSYNC原始样本及按需未限速诊断、输入/准备时延、真实资源压力和生命周期原门槛均有可追溯结论；共享修正回归当前Metal受影响startup/封面/开场/实验面板及默认隐藏策略。未解决失败明确保留，不由部署或本机局部成绩冲销。

#### UI-CONTROLS-P · 自绘控件的目标系统与原生输入

- 输入为controls.md定义的Archive控件、TextAreaFormat、全项目QML视觉树与ControlsReview/ControlsAudit；冻结music.css/style.css/music-lighting-lab.css按历史映射。匿名控件与既有面板fixture不读取私人库。缺Windows/Linux最新原生Kit及目标GPU/窗口，本机隐藏Metal只证明Qt对象输入/自身输出，不能证明物理输入/系统输入法/辅助技术或跨屏。
- 目标Release构建、官方Yalahoo_qmllint/ui-audit_qmllint、check-format/source-format、kit-selection/bootstrap与`python3 tools/audit/audit-controls.py`；按可靠窗口策略执行`controls-qml`及settings-qml/experiments-qml/library-panels-qml/playback-qml，保留来源尺寸/行高、按test-gates.md分别覆盖布局尺寸、双主题及DPR边界与各宿主输入隔离/关闭释放原门槛。改变环境主题后绘制仍取项目token，模块/qrc/字体和QuickTemplates2在真实目标部署闭包可解析。
- 物理键鼠/触控板、Tab/ShiftTab、方向键/Space/Enter/Escape、失焦/外部点击关闭、滚动条拖动与下拉虚拟滚动、系统中英文/emoji组合输入、选择/剪贴板菜单/撤销重做、readOnly/disabled、屏幕阅读器角色/名称、目录选择与跨屏DPR/原生hide/minimize/restore按目标系统验收。D3D11/Metal归R34，全部OpenGL及Linux窗口归R35；不以隐藏事件或显式target DPR替代原生事实。共享修正只回归受影响的本机控件/宿主与窗口策略，完整性能仍依R28和最终矩阵。

#### R27-P · 光照/速度实验与匿名启动的目标平台/实机回归

- 输入为LightingPanel/MotionPanel/ExperimentPanels、SelectionLighting已确认/临时对照状态、SettingsController/PreferencesStore、DiagnosticSamples/SceneItem/RhiItem、demonstrationLibrary及根/工具目录启动脚本；冻结music-lighting-lab、music-motion-lab、music-row-lighting和CSS经历史映射。匿名九列二十七专辑只含qrc封面/空曲目。缺Windows/Linux真实系统、最新原生Kit、目标GPU/原生窗口；本机隐藏Metal/Qt事件不能证明物理键鼠、跨屏、正常VSYNC或歌曲听感。
- 目标Release真实构建、官方Yalahoo_qmllint/ui-audit_qmllint、check-format/source-format，执行`python3 tools/audit/audit-experiments.py`与`ctest --preset qt-release -R '^(experiments|settings|motion|lighting|verification-window-policy)$'`。保留四模式、五项原范围/guided固定预设、无效patch原子拒绝、未知嵌套字段、临时对照不保存、改速x/v/a/revision连续和片头未缩放dt。Windows另真实解析/运行新ps1及FormatTools，核对任意cwd/Unicode空格/参数与本次GUI ExitCode；macOS/Linux运行`python3 tools/audit/verify-experiment-launchers.py`，帮助与演示/数据组合拒绝不得触碰数据根。
- 按可靠窗口策略运行`ui-audit --experiments 全新目录`/`experiments-qml`：按test-gates.md分别覆盖布局尺寸、双主题及DPR边界、窄屏折叠/滚动、真实Qt滚轮/方向键/空格、四模式实际参数上传与资源/postprocess generation不变、按住原版后释放/失焦/隐藏/销毁恢复、关闭/重开与renderer释放。诊断开启才采样；固定256样本/nearest-rank/至少500ms批次与一个待送通知，关闭/隐藏/换窗口拒绝旧epoch，无生产readback。CPU提交/同步、QRhi已完成GPU、未钳制提交间隔、原生Qt呈现事件、请求上传字节与解码计数范围按performance.md；缺GPU/原生呈现/实际VSYNC不可伪造为0或已通过。
- 产品实机用物理键鼠/触控板核对面板滚动不导航、各倍率往返连续、片头/普通歌曲不随改速、默认复位与设置入口一致；原生同对象hide/minimize/restore、跨屏DPR和应用后台恢复保持当前参数/歌曲且不累积诊断。Windows/D3D11和macOS/Metal归R34，全部OpenGL归R35；目标修正仅回归受影响的本机参数/设置/光照/面板/窗口检查。正常VSYNC与完整性能归R28/最终矩阵，不能将隐藏调度吞吐当作正式呈现成绩。

#### R26-P · 完整设置与资料路由的目标平台/实机回归

- 输入为SettingsController/PreferencesStore（含C++单字段画质合并）、SettingsPanel/MetadataSection及MetadataPresentation、LibrarySession批次/能力/代次、LibraryStore::setMetadataConfiguration及实际App视觉重载；冻结music-app::renderSettingsPanel、music.css、quality-settings和motion-settings经历史映射。匿名库/公开Qt合成WAV、SampleProvider的Matched/Uncertain/Error及不同代订阅者只用于测试；产品仅OfflineMetadataProvider，零网络。接口/来源边界见library-data.md和source-fidelity-review.md。缺Windows/Linux系统、最新原生Kit与目标窗口/音频环境；本机隐藏Metal与Qt事件不代替物理输入、跨屏或正常VSYNC。
- Release目标真实构建后执行`ctest --preset qt-release -R '^(settings|library-data|library-qml|media-metadata|audio-ambience|real-columns|render-quality|verification-window-policy|source-format)$'`、官方Yalahoo_qmllint/ui-audit_qmllint及`python3 tools/audit/audit-settings.py`。保留schemaVersion1/未知字段、对象JSON≤1MiB/只读源/坏文件保护、原子保存合并、变更字段路由、八个独立请求/一个批次请求、requestId/generation/album/kind拒绝和取消/换库/退出门槛；不以禁用实现代替目标编译。
- 当前可靠窗口策略运行`ui-audit --settings 全新目录`/`settings-qml`，按test-gates.md分别覆盖布局尺寸、双主题及DPR边界：两列主题、排序/阵列/列记忆、画质/折叠精细、减少动态/.25–3速度、三路声音/设备、MusicBrainz配置及整页滚动；查询按钮禁用且说明离线，缓存内容/来源/原状态正常，注入三种状态与能力变化不改UI。实际Qt输入、关闭/反向打开/模态隔离、面板与背景释放、真实App十次视觉树重建保留同一媒体对象/音源/意图/用户画质。macOS用隐藏Metal；Windows/D3D11归R34，全部OpenGL归R35，目标有可靠隐藏能力先重新分类；不将显式target DPR当物理屏幕。
- 原生产品还须以真实键鼠/触控板核对设置控件滚动不导航、重新打开反映当前状态、全屏/片头重播/界面重载，并在实际播放/暂停时确认媒体连续、三路音量与设备意图保留；原生同对象hide/restore和跨屏DPR归既有最终矩阵。正常VSYNC/正式性能按R28及最终矩阵；隐藏场景重载只证明视觉所有权，听感/物理输出及缺目标执行保留，不能写通过。共享修正仅回归本机受影响路径。

#### R25-P · 三路声音与原生数据的目标平台/实机回归

- 输入为BackgroundMusicController/InterfaceSoundController、PlaybackController静音门、PreferencesStore与MusicTypes读写边界、原生metadataConfiguration/genreOverrides、RenderQuality单参数接口及生产声音面板。资源为既有atmosphere.ogg、interface-events.json（17事件当前均无独立短资产）、AudioAmbienceTest匿名WAV/测试专用短WAV、LibraryAudit匿名库及既有来源画质oracle；来源/消费者与实现规范见playback.md/library-data.md/occlusion.md。缺Windows/Linux系统、最新原生Kit/音频后端；本机Qt媒体进度与Metal隐藏输出不证明物理路由/听感、原生最小化或其他平台通过。
- R34目标Release实际构建及`ctest --preset qt-release -R '^(audio-ambience|playback|library-data|library-qml|directory-scanner|media-metadata|render-quality|real-columns|source-format|verification-window-policy)$'`、官方Yalahoo_qmllint/ui-audit_qmllint、`python3 tools/audit/audit-audio-ambience.py`。保留独立player/output、单个24ms必要淡变timer/四槽音效且满池丢弃、无视觉时钟音频进度、实际静音后放行歌曲、220ms下降/650ms恢复、暂停/格式错误不恢复、曲间不出现BGM、开关/反向/退出取消原门槛。当前画质算法/尺寸门槛、未知未来扩展和明确Provider槽不变。共享修正回归本机受影响项。
- 当前可靠窗口策略执行`playback-qml`或`ui-audit --playback 全新目录`，按test-gates.md分别覆盖布局尺寸、双主题及DPR边界：三路音量/开关、静音资源试听禁用、全内容滚动、设备/Qt格式错误、进度/导航播放隔离与面板关闭资源回落。D3D11/Metal归R34；三平台OpenGL和Linux native图形/输入归R35，正常VSYNC/完整产品性能依R28和最终矩阵。
- 三平台物理输出分别核对歌曲显式设备与BGM/音效系统默认路由、默认切换/无输出/重连及当前Playing/Paused/Stopped意图；真实最小化和同窗口恢复时歌曲继续，后台只保留必要音频事件与淡变。听感确认无意外重叠/突变，三路音量独立；当前缺资源事件须精确静音，不补合成器/PCM。程序进度/真实Qt描述符通知不冒充听感、拔插或物理最小化，缺音频设备与窗口条件不记通过。其余原R24-P/R22-P设备、格式、导入门槛仍各自执行，不重复定义。

#### R24-P · 普通歌曲播放与设备的目标平台/实机回归

- 输入为PlaybackController、MusicStage/Header/Detail/PlaybackControls/PlaybackPanel、LibrarySession受控播放ID同步、PlaybackTest/AudioFixture及`ui-audit --playback`匿名WAV/损坏输入。来源为冻结music-player/music-app和REFDB-04公开函数/事件序列，实际读取hash/作者/MIT见随包参考NOTICE。缺Windows/Linux目标系统、最新Kit与音频后端；本机真实Qt播放和Metal隐藏结果不代表物理拔插/默认路由切换、听感、物理输入/跨屏或其他格式全集通过。
- 目标真实Release构建、format/check-format、官方Yalahoo_qmllint/ui-audit_qmllint、`ctest --preset qt-release -R '^(playback|library-data|library-qml|track-focus|navigation|presentation|source-format|verification-window-policy)$'`及`python3 tools/audit/audit-playback.py`，保留单个歌曲player/output、公开API、local URL、rate=1、source revision/断旧连接、暂停/停止意图、一次EndOfMedia、保留孤立队列和导航播放隔离。不得迁入私有设备补丁、模拟描述符或PCM/外部解码后备。
- 当前可靠窗口策略运行`playback-qml`/`ui-audit --playback 全新目录`：按test-gates.md分别覆盖布局尺寸、双主题及DPR边界，真实播放按钮、歌名/图标、播放进度/单次seek、音量/系统默认与设备选择、格式错误/重试、搜索/库替换/定位不改音源及关闭面板/隐藏恢复。D3D11/Metal归R34，三平台OpenGL和Linux native图形/输入归R35；正常VSYNC与完整产品性能按R28/最终矩阵，不从离屏或局部媒体事件推断。
- 真正设备分别执行：默认输出改选、明确选择仍在时不随默认、显式设备消失回默认并提示、全部输出消失时待播、同设备/新设备重连、加载中换曲/换设备、暂停/停止后重连、有效位置/音量保留、最小化歌曲继续及退出停止。PlaybackTest空列表/真实描述符通知仅为竞态分支证据，不能冲销实物门槛。Windows/Linux用本机Qt实际支持格式验证播放/暂停/seek/停止/自然队列结束；覆盖不可seek/未知duration、损坏/无音轨/不支持格式的Qt原因，不按扩展名假定能力。无声加载、Qt播放进度、实际出声/听感分别记录；缺格式/设备条件仍未通过，修正共享代码只回归受影响范围。

#### R23-P · 音乐库面板、搜索与歌曲定位的目标平台回归

- 输入为LibraryPanel/SearchPanel/GenreRowsModel/LibraryPanels、PanelController/TrackFocusController、LibraryModels、LibraryStore与生产MusicStage，冻结music-app/music-search/music-track-focus/music.css/ui-transitions及TrackFocusTest/LibraryAudit匿名库（跨歌手列、200曲目专辑/4000命中、1024专辑查询）。本机Metal隐藏执行范围见source-fidelity-review.md；缺Windows/Linux目标系统/最新Kit，物理输入/原生目录对话框、真实屏幕DPR及正常呈现不由离屏替代。
- 目标Release真实构建、`check-format`、官方`Yalahoo_qmllint`/`ui-audit_qmllint`及`ctest --preset qt-release -R '^(track-focus|library-data|library-qml|directory-scanner|cover-cache|presentation|detail-motion|navigation|source-format|verification-window-policy)$'`，运行`python3 tools/audit/audit-library-panels.py`绑定COMMON/PERF/平台符号/hash。保留冻结216例、trim/lowercase语义、按快照一次预处理、查询增量通知、专辑内曲目身份和一次最后目标/无播放副作用。
- 图形入口`ui-audit --library 全新目录`/CTest `library-panels-qml`按窗口契约与当前可靠能力分类：按test-gates.md分别覆盖布局尺寸、双主题及DPR边界，面板宽/边距/全内容滚动、4000结果/大量流派虚拟化、输入取消/无结果、模态焦点/Find/Escape/箭头与Enter、跨歌手列退详情→归架→五次定位→打开→目标居中高亮、同专辑不重抽盒、关闭背景附件释放/Descriptor回落。布局≤1逻辑px；滚动/高亮按源函数与1100ms减少动态，不能恢复图像评分。D3D11/Metal归R34，三平台OpenGL及Linux native窗口归R35。
- 实机普通产品用临时匿名目录通过文件夹选择/多根保存/重扫/取消/离线错误检查Unicode/空格/#/Windows UNC，任何目录保存不写歌曲。真实键鼠/触控板、编辑焦点、面板滚动与场景输入隔离、窗口隐藏恢复/跨屏和当前播放歌名定位逐项验证；R24接线后核对播放/暂停/source/音量不因定位变化。共享修正仅回归受影响范围；正常VSYNC、120秒完整产品成本与实际显存按R28和最终实机矩阵，不用局部隐藏提交推断通过。

#### R22-P · 无声元数据、封面与旧数据的目标平台回归

- 输入为MediaMetadataProbe/ArtworkStore/LegacyImporter/LibraryStore/PreferencesStore、原生main的`--import-legacy`/`--import-preferences`接线和MediaMetadataTest运行时生成的匿名RIFF/INFO/ID3-PNG、v3/v4索引/配置。缺口是Windows/Linux目标系统、最新原生Kit、媒体插件及真实文件系统；本机macOS非图形和默认隐藏Metal实际执行范围见source-fidelity-review.md，不能外推目标后端或实际播放能力。
- R34在目标最新原生Kit实际Release构建后执行`ctest --preset qt-release -R '^(media-metadata|directory-scanner|library-data|library-qml|cover-cache|rhi-atlas-queue|source-format)$'`及`python3 tools/audit/audit-media-metadata.py`。保留两个复用/无输出/无play的player、各任务generation/serial、8秒超时、取消/晚到/换根/退出、Unicode路径、全部公开字段/缺失字段保留、损坏文件不删曲目、Loaded与实际播放能力分离的原门槛，不用模拟source替代真实Qt加载。后台加载须保留GUI事件响应和一次快照/模型交接；完整产品时延仍归R28。
- 真实输入需覆盖目录cover/folder/front及坏图fallback、直属单曲内嵌图/同名图、30MiB编码与4,194,304展开像素保护、内容寻址复用/换代、缓存符号链接拒写、v3/v4旧索引制作信息/备注/来源/未知JSON、配置与人工流派引用、只读源/不复制歌曲及PCM、显式浏览器JSON/坏文件/原子保存错误。Qt内部嵌入图解码峰值不可见，不能把返回QImage之后的检查称为原始压缩字节检查或真实分配峰值测量。Windows核对UNC/大小写/ACL与原子替换拒绝，Linux核对大小写/权限/只读目标；符号链接能力缺失的项仍未通过。
- 共享封面回归按本轮目标可靠窗口策略执行`archive-pool-gpu`、`real-columns-gpu`及受影响光学/封面检查；生产图集中同专辑换曲保持零重解码/上传，单封面revision只上传其阵列tile与16个高清tile，保留无关槽serial/兼容资源generation、归位旧像素和稳态零上传。D3D11及实际非OpenGL后端归R34，三平台OpenGL归R35，不恢复源截图评分。修正共享代码须回归主力平台相关项，范围按test-gates.md选择；缺实机不记作通过。

#### R21-P · 本地扫描的目标文件系统回归

- 输入为`src/library/DirectoryScanner.*`、`LibraryStore.*`、原生main的`--music-root`/`--rescan-library`接线和`DirectoryScannerTest`运行时生成的匿名目录；来源为冻结`scripts/music-library.mjs::safeRootList/walkAlbums/readAlbum`。macOS当前原生Kit可执行非图形扫描/模型/缓存检查；Windows/Linux缺目标系统、原生Kit和文件系统环境，不记作目标运行通过。
- R34在目标最新原生Kit真实Release构建后执行`ctest --preset qt-release -R '^(directory-scanner|library-data|library-qml|source-format)$'`、`node tools/review/verify-scan-fingerprint.mjs`（仅开发验证）及`python3 tools/audit/audit-library-scan.py`；保留16扩展名、64根上限/嵌套去重、原生路径UTF-8 ID、size:mtimeMs、重扫/改名/删除/离线、最后切根/取消、一次GUI模型重置、坏缓存保留/原子写入失败和源hash不变门槛。
- Windows额外核对真实Unicode/空格/大小写盘符/UNC、目录junction/符号链接循环与重复、ACL读取拒绝及缓存替换拒绝；Linux核对大小写区分、目录权限拒绝和只读缓存。使用目标临时匿名目录，符号链接能力缺失不能冒充该项通过。修复对象是nativePath/qtPath/normalizeRoots/fingerprint和QSaveFile错误路径；不引入目录监视、网络或媒体解码。
- 扫描期间事件循环/输入响应记录实际worker阶段与GUI快照应用间隔，完整产品呈现/扫描快照交接性能继续归R28；无声元数据/封面/旧数据导入归R22。共享修正回归主力平台directory-scanner/library-data/library-qml，范围按test-gates.md选择。

#### R20-P · 序章目标平台与实机交接

- 输入为OpeningController/OpeningInk、生产MusicStage/SceneItem、`tests/fixtures/opening/source-golden.json`、原创SVG/reveal-map和内置匿名封面；冻结函数/相机片段由export-opening-golden.mjs导出。当前缺Windows/D3D11目标系统/Kit/GPU；三平台OpenGL执行分工见上表。无已确认目标产品失败，未运行不能写通过。
- 静态入口`python3 tools/audit/audit-opening.py`及`node tools/review/export-opening-golden.mjs --check`；目标Release实际构建/qsb、`opening`数学/状态测试及适用的官方qmllint/格式检查。具体算法和资源职责见opening.md。
- 目标运行`ui-audit --opening 新目录`或`opening-qml`精确CTest选择，按test-gates/test-window-policy选择隐藏或当轮授权native入口。按test-gates.md分别覆盖布局尺寸、双主题及DPR边界，保留逐参数/相机≤1e-6、20–120fps、遮挡零三维submit、正确恢复首帧/一次交接、跳过/重播/减少动态/焦点、十次SG重建与Canvas资源回落门槛；实际质量独立检查描边/点/圆环/遮罩/透明合成。
- macOS本轮隐藏结果不代替原生同窗口恢复、物理输入/跨屏DPR、正常VSYNC或完整产品性能，这些按“各平台最终实机矩阵”执行；共享修正回归受影响组件，范围按test-gates.md选择。

#### R19A-P · 资产来源保护与缺图绘制的目标平台回归

- 输入为冻结基线（按history-rewrite映射）、assets/branding清单、匿名本地/qrc封面与现有库fixture；实现与历史归属见`docs/contracts/source-fidelity-review.md`。
- 严格静态核对`PrepareAssets::verifyFrozenInputs`、WIN32 DirectWrite/非Windows SFNT复用、QImageReader插件/ICC、CoverCache脚注字重及generation/lease/serial、QtKit架构/ABI、qrc/QtSvg与隐藏窗口分支；本机macOS Release资产/缓存/模型及Metal隐藏回归不外推目标平台。
- 目标Release构建后运行`prepare-assets <仓库> <同一最新Kit> --verify-inputs`及`ctest --preset qt-release -R '^(assets|branding|cover-cache|rhi-atlas-queue|library-data|library-qml|kit-selection|module-link-and-plugins)$' --output-on-failure`。在临时隔离源码根篡改一个输入，确认普通生成入口在任何写入前失败；不得修改真实归档/产品资产完成负向测试。
- 退出：冻结blob/归档/清单一致、完整几何/字体物理表/功能SVG及ICO/ICNS容器有效、无声媒体仅加载不播放、ICC/横竖方/alpha/缺图脚注符合源码、队列/租约/关闭与PERF-01–07通过。共享修正回归当前主力平台的assets、cover-cache、library-data/library-qml，范围按test-gates.md选择；目标GPU、部署和媒体设备仍由本节对应专项验证。

#### R19B-P · 运动、相机与阵列的目标平台回归

- 输入为冻结基线（经映射）、motion/scene/archive-pool/real-columns 数学 fixture 和匿名库。
- 严格静态核对唯一帧源/速度、double x/v/a、canonical GL 投影→QRhi 修正、GLB/XYZ 矩阵、最多 432+4 实例、惰性列时间/减少动态 epoch、稳定物理 ID、lease/generation/关闭及所有平台条件编译；审查结论与实现符号/hash 入口见 `docs/contracts/source-fidelity-review.md` 的 R19B。
- 目标 Release 构建后运行 `check-web-baseline.mjs` 及 motion/scene/archive-pool/real-columns 源码导出器；用当前系统 `verification/{windows,linux}/real-columns/full-trajectories.json` 设置 `YALAHOO_REAL_COLUMN_GOLDEN` 运行 RealColumnsTest。普通快速 CTest 覆盖 motion、scene、real-columns、archive-pool、cover-cache、rhi-atlas-queue、navigation、detail-motion、overview、verification-window-policy。
- `YALAHOO_REVIEW=ON` 后通过 `ctest --preset qt-release-visible -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure` 运行目标实际后端；入口现无任何图像匹配。按当轮授权选择默认置底/拒绝焦点或显式前台；保留源 x/v/a≤1e-6、Qt float 矩阵按既有测试容差、投影≤2逻辑px、边界事件消费不改变稳定 ID、两模式/记忆/库替换、90,000 库容量、物理 ID/cover 区域 rebase 不变、稳态零上传、≤2MiB 突发及 cover/renderer 生命周期要求。
- 三平台物理输入及后续完整产品窗口/表面与正常VSYNC见“各平台最终实机矩阵”，不用offscreen invalidation替代。目标共享修正后回归当前平台 motion、scene、archive-pool、real-columns、navigation、detail-motion、overview、封面，范围按test-gates.md选择；

#### R19C1-P · 材质与光场参数的目标平台回归

- 输入为冻结基线（经映射）、R19C1提交中的45组materials/parameters、210组render-backend参数、lighting/golden及匿名阵列；实现符号与hash入口为check-web-baseline及audit-material-parameters。
- 静态展开ShellMaterial/SelectionLighting/RowLightController→SceneItem→Renderer uniformTemplate/逐实体lid/上传、QML有效属性、WIN32 D3D适配隔离、Linux/Apple真实Qt分支、std140和qsb各目标。矩阵入口`python3 tools/audit/audit-material-parameters.py`；当前同Kit跨目标qsb只证明静态翻译。
- 目标最新Kit配置Release，运行`node tools/review/export-material-parameters.mjs`、check-web-baseline、`ctest --preset qt-release -R '^(rhi-material-parameters|lighting|scene|cover-cache|rhi-atlas-queue|verification-window-policy)$' --output-on-failure`。Review ON后按窗口契约运行`ctest --preset qt-release-visible -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure`；能可靠隐藏时优先用真实隐藏入口，不能写假offscreen结果。
- 退出：45+210源参数、924轨迹/392场核和真正上传的transport/coat/tint/attenuation逐实体通过（既有2e-6/3e-5、轨迹/主题1e-6、位置1e-4容差不放宽），无ROI系数重入，双主题/详情/归位、资源重建和默认窗口策略通过；光学shader/连续质量与BLUR-01/FLICKER-01的来源和本机结论见source-fidelity-review.md，目标质量仍须实际验证；共享修正回归当前主力平台材质/灯光/阵列/封面，范围按test-gates.md选择。

#### R19C2A-P · 材质与灯光Review目标平台回归

- 输入为R19C2A实现提交（起点48a9495）、冻结基线（按映射）、materials/single-case.json、lighting/visual.json及均匀paper-probe.png。PowerShell薄入口的目标进程等待/退出仍须在目标执行，不能以宿主解析/格式检查代替目标运行。
- 静态展开MaterialReview→独立GeometryScene→OffscreenSurface/原生窗口、QML root/engine/控制器所有权、原子invalidation计数、qrc/GuiPrivate/QtQuick3D、Windows MSVC入口及真实Python进程退出；`python3 tools/audit/audit-material-review.py`生成绑定符号/hash的COMMON-01–05/PERF-01–07及平台矩阵。
- 目标Release Review ON后运行`ctest --preset qt-release-visible -R '^(runtime-materials|runtime-lighting)$' --output-on-failure`（可靠隐藏能力改变时先重新分类）；默认置底/拒绝焦点，只有当轮授权才可前台。Windows先执行`tools/format/FormatTools.ps1 -Check`并运行verify-r07.ps1/verify-r08.ps1，Linux可在配置工具链后运行`python3 tools/build/verify-material-review.py materials|lighting`的对应单个入口。
- 退出：99材质/纸面与48灯光状态、3表面参数≤1e-6、源码/CPU/Quick3D投影≤2逻辑px、纸面源公式误差≤3码值、实际DPR/目标与有效输出、QML warnings为空及窗口策略通过；材质十次/灯光一次完整root重建均真实失效并重放有效输出。原生窗口的同对象hide/restore、正常VSYNC仍按“各平台最终实机矩阵”执行，不以完整root重建替代。共享修正回归主力平台受影响测试，范围按test-gates.md选择。RHI光学及BLUR-01/FLICKER-01的来源和本机结论见source-fidelity-review.md，不能外推为目标通过。

#### R19C2B1-P · 不透明RHI目标平台回归

- 输入为R19C2B1实现提交（起点75b5543）、冻结基线（经映射）、匿名三封面、已知GLB平面/源纸面光照/阴影公式及有界RhiCaptureHarness；
- 静态矩阵入口`python3 tools/audit/audit-rhi-opaque.py`逐COMMON-01–05/PERF-01–07/平台绑定符号/hash，核对D3D/GL clip/资源/线程、QPointer值交接、最多2批/唯一纹理读回、实际格式与imageOf端序/Y翻转；不以Metal或同Kit跨目标qsb代替目标运行。PowerShell工具须按目标执行真实解析/FormatTools -Check及对应退出行为，不以宿主检查代替目标运行。
- 目标最新Kit Release Review ON后，执行paper/shared shader检查、`ctest --preset qt-release-visible -R '^rhi-opaque-gpu$' --output-on-failure`，按当轮窗口契约；可靠隐藏能力变化时先重新评估。预期36状态、16源墨色探针、12有序尺寸、30逐帧resize、同request封面替换、6阴影状态及十次真实graph重建通过；保持深度<2e-6、法向/纸面≤1/255、四角≤2实际DPR1物理px和原shadow强度范围.675–.70，不降低门槛。alias字段共享storage、无额外readback，真实driver bytes未知为null。
- 共享修正回归主力平台受影响的阵列/真实列/封面/材质/详情/总览检查，范围按test-gates.md选择。隐藏graph重建不证明原生窗口恢复、正常VSYNC或产品性能，按“各平台最终实机矩阵”执行；BRDF/透明捕获、fog与BLUR-01/FLICKER-01来源及本机结论见source-fidelity-review.md，目标仍须实际验证。

#### R16B2W · D3D11/DPR2文字、成本与生命周期

- 依赖R33阶段交接和可靠Windows/D3D11目标环境；执行前重新评估规模，覆盖真实DPR1/1.5/2中尚欠的字体、布局与生命周期事实，不为栅格相似度改动正确绘制路径。
- 输入为冻结字体/生产字轮源码、`tests/fixtures/text/titles.json`。目标先运行`export-title.mjs --output 新源目录`，再以`verify-title.mjs --backend d3d11 --dpr 数值 --source 新源目录 --output 新目录`及`audit-title.mjs 新目录`验证。静态入口`python3 tools/audit/audit-title-source.py`绑定原轮次/hash；Windows须真实运行`FormatTools.ps1`及`-Check`并解析/执行两个PowerShell薄入口。
- 针对R19F当前实际DPR槽层（起点cfce597）及既有字体/通知修正，检查20项双主题文字的内容、字体身份、连字/组合字/彩色字体、断行/省略/裁切、QML层请求尺寸与资源计划一致，以及18项页眉的结构、按钮位置、改速/减少动态及定位不改变播放；验证十次恢复和最终释放。三路径各一次120帧活动/缓存回放；明确成本对比才开启三次独立采样，分别报告GUI、同步/CPU提交、Canvas绘制、缓存与资源，隐藏候选无文字绘制、静止无重排，GPU不可测为null。
- 退出：结构≤1逻辑px、源码运动≤1e-6、窗口/资源/生命周期、Release/source-format/官方qmllint/qsb与PERF-01–07检查通过；只回归实际改动影响的DPR/后端。

#### R16B3 · OpenGL功能、质量与文字路径选型

- 依赖R16B2W及R16M3阶段选型。在可靠Windows/Linux目标OpenGL环境运行相同匿名文字/页眉、DPR/主题、运动、生命周期及一次局部成本诊断（明确对比才重复），验证实际可见呈现；macOS已有隐藏/授权原生OpenGL4.1/CoreProfile事实不外推到其他系统，也不无故重复未受影响矩阵。
- 修复实际后端的shader/pipeline、文字缺失、裁切、输入、缓存或资源错误；按源码核对布局/字形与运动，检查可见锯齿和异常模糊。保留正常画质，不为贴合另一后端的栅格改字号、基线或采样。
- 依据源码语义、实际功能/质量、成本、缓存失效与生命周期选择生产路径，写明连字/组合字、彩色字体及维护差异。执行COMMON/PERF和平台分支审计；OpenGL上下文/着色器兼容边界见`docs/contracts/title-reels.md`。
- 退出：上述非像素要求通过，更新简明结论，并回归R17–R33受影响路径。

#### R16M3-S · 共享隐藏完成通知的目标平台回归

- 依赖R33交接及可靠Windows/D3D11、Linux/OpenGL环境。
- 输入为冻结字轮/字体、匿名titles fixture及TitleReel自然宽度通知、active=false有界queued finish/恢复取消、TransportTitle退出运动。运行目标Release的`title-reels`与适用Release回归，确认十次hide合并、恢复取消、真实宽度单次通知；使用上节当前非图像入口检查20项文字、18项页眉、三路径活动/缓存（次数按title-reels.md）、隐藏零文字工作、十次恢复/释放及零绑定环/shader错误。
- 此项为尚未执行的共享回归。结构≤1逻辑px、源码运动≤1e-6及窗口/资源门槛保留；修改共享通知链后回归当前平台受影响路径。真实可见hide/restore/minimize与正常VSYNC另行验证，离屏释放不代替呈现事实。

#### R17-P · 详情展示的目标后端与可见输入/焦点

- 依赖R33交接、R18同步运动ports及可靠目标Kit/窗口管理器。保留Windows/D3D11、Linux/OpenGL/X11/Wayland的组件验收和三平台后续产品运动/焦点集成。
- 输入为匿名PresentationTest、PresentationAudit曲目/介绍/制作人员与冻结music-presentation/ui-transitions/music-title/music.css调用链。运行目标Release configure/build、官方两组qmllint和适用Release回归；`ui-audit --presentation 输出目录`检查18项结构、非零且<100的委托、零QML warnings、十次恢复/释放与windowPolicy。审计入口`tools/audit/audit-presentation.py`，细节见`docs/contracts/presentation.md`，
- R19G2新增sticky tabs/指示条/内容淡入、scroll距离钳制、正常混合字体行高、离线badge/lookup和制作人员分组；目标入口额外执行`node tools/review/export-browse-ui.mjs --presentation`及`compare-presentation-structure.py`独立冻结DOM/CSS结构核对。后续产品接线用匿名`verify-detail.py --backend <目标后端> --dpr 1 --runs 1 --output 新目录`核对；物理输入/系统焦点和R23/R24实际播放隔离继续保留。
- 实际产品验证browse退场后相机进入、layoutRevision/场景就绪、退字连选/取消只提交最后目标、四种archiveRoute、快速open/back、改速/减少动态、介绍/歌单滚轮及键鼠/触摸。检查退出期禁用、有效Item焦点恢复、hide/restore/minimize及surface/flags重建；R23/R24后补真实定位不改变播放。
- 退出：结构≤1逻辑px、时序≤一个60Hz帧、源码状态/revision与PERF-01–07通过；按授权选择前台或默认策略，遮挡无效样本不算通过。共享变更后回归presentation、presentation-qml、navigation、title-reels，范围按test-gates.md选择。

#### R18-P · 详情运动的目标后端、物理输入与产品集成

- 依赖R33及可靠目标Kit/窗口管理器/设备。输入为DetailMotion/ReturningCasePool/PointerDragController/CaseClarity、匿名detail-golden/qrc封面和冻结music-camera/scene/music-app；
- 目标Release构建、官方两组qmllint和适用Release回归后，运行`python3 tools/build/verify-detail.py --backend <目标后端> --dpr 1|1.5|2 --runs 1 --output 全新目录`；当轮获授权才加`--foreground`，夜间用`--night`；三十次换片/十次重建仅在相关边界以`--lifecycle`另执行一次，不随每个DPR或功能重复运行。检查退browse后同步抽取/相机、三次换片/反向、满池最后目标、旋转副本对齐/下降与新盒等待、途中返回/搜索、拖动取消、改速/减少动态、三十次切片及十次图形上下文重建。
- 分开记录Qt提交/ready延迟、上传突发、四份CPU封面租约/分块、GPU图集/附件估算；运动不重建不变目标/材质，实际driver显存不可测为null。轨迹各≤1e-6、投影≤2逻辑px、时序≤一个60Hz帧及PERF-01–07保留。
- 三平台补真实触摸/系统抓取与焦点丢失、有效Item焦点恢复及R23/R24实际定位/播放隔离。自注入Qt事件不冒充物理设备或input-to-photon。三维源码/质量、完整GUI及120秒性能分别由相关任务验证。共享修正回归detail-motion、scene、archive-pool、real-columns、presentation、presentation-qml、navigation、title-reels及实际后端；入口见`docs/contracts/detail-motion.md`和`tools/audit/audit-detail.py`。

#### R19-P · 总览的目标后端、物理输入及平台回归

- 依赖R33、R19源码交付及R19A–R19H复查结论；输入为冻结基线、`tests/fixtures/overview/source-golden.json`和匿名内置库。静态审计见`tools/audit/audit-overview.py`，不替代实机。
- R19G1补验沿`tools/audit/audit-detail-overview-source.py`追踪R18/R19及后续来源，修复标签槽退出240ms早于入口收拢420ms时跨列残留宽度/透明度，保留反转复用；计数文字恢复源block宽度。目标`--visual`报告须含真实Qt两步点击、投影锚点/卡片/局部纹理结构、退场释放与十次根/渲染资源重建。执行`node tools/review/export-browse-ui.mjs --overview --overview-native 目标report.json`及`python3 tools/review/compare-overview-structure.py 目标report.json verification/overview-source/report.json 输出.json`，源碰撞准入和卡片结构保留原门槛。Windows/Linux缺目标Kit/GPU；后续产品接线、物理输入/屏幕及缺目标环境按下列原门槛执行，不以组件或离屏结果外推。
- 在目标最新原生Kit的Release构建后运行官方两组qmllint、适用Release回归及`python3 tools/build/verify-overview.py --backend <目标后端> --dpr 1|1.5|2 --runs 1 --output 全新目录`；夜间用`--night`，当轮明确授权才加`--foreground`。窗口压力序列以`--lifecycle`在相关所有权/上下文边界单独执行一次，不在每个DPR或功能重复运行。另以`--visual --runs 1`在真实可用隐藏入口覆盖九尺寸/双主题/收起与展开36状态、十次实际根/资源重建；目标无可靠离屏入口时使用遵守窗口策略的真实渲染并记录范围，不能伪造隐藏运行。
- 检查V/Esc/点专辑同一返回、第一击只展开/第二击引导进入、真实列/填充共用progress、±4.5折叠与extent≤.12不可拾取、标签滞回/碰撞/窄屏、播放歌名与按钮固定、减少动态/改速/反转、库替换、十次隐藏/surface重建和资源回落。三维目标/postprocess不随总览转换重建，图集稳态突发≤2MiB；UI局部模糊层另行记账，未知driver分配写null。
- 三平台补物理键鼠/触摸板、焦点丢失/恢复与真实窗口状态；R23/R24后补真实定位不改变播放。当前macOS自注入Qt事件、窗口读回/隐藏UI均不证明物理设备、屏幕扫描或完整120秒性能；这类独立事实仍需实际目标验证。
- 退出：数学轨迹≤1e-6、源代码投影≤2逻辑px，实际结构/交互/清晰度/平滑轮廓/资源和平台要求通过，无QML/shader/窗口策略错误；共享变更回归overview、overview-qml、detail-motion、scene、real-columns、archive-pool、presentation、navigation、title-reels及受影响实际后端；准确范围见`docs/contracts/overview.md`。

#### MAC-VISIBLE-3D · 物理显示、跨屏与输入

- 依赖R28/R33完整产品接线。输入为冻结基线、生产资源与render-backend/rhi-shell/postprocess/occlusion源参数；在实际目标屏幕检查可见锯齿/异常模糊、漏画/裁切/错误合成。物理屏幕/跨屏与输入事实不能从自身纹理或Qt事件推断，不恢复浏览器GPU字节或截图相等门禁。
- 用普通部署包和匿名库检查真实键鼠/触控板、焦点、Retina跨屏后SceneItem/Canvas/文字的尺寸与清晰度、窗口和实际显示恢复。其他目标设备的构建/部署/性能沿R31-P及各平台最终矩阵；当前Metal呈现间断只按R28-P定位，不重复维护采样任务。
- Quick3D同格式4×renderbuffer/resolve适配归materials契约。OpenGL同项按R35执行；未知GPU完成、屏幕显示延迟与driver显存保持null，窗口前台仍须当轮授权。

#### 各平台最终实机矩阵与专项退出

- 输入为同源匿名fixture、冻结生产源码、当前Release源码/二进制及具体产品失败/暂缓记录。先核对系统/Kit/后端/设备/入口/窗口策略，未具可靠环境的项目继续保留，不能把“已安排”写成“已执行”。
- 窗口：Windows/Linux验证首次show、hide/restore、minimize、flags/surface重建与退出；按当轮授权选择前台或默认策略。默认隐藏策略持续回归；未授权模式遮挡/节流样本不可用，不提升Z顺序。
- 图形/输入：按后置专项分工分别对D3D11、Metal/Retina、OpenGL及实际启用的Vulkan，按test-gates.md分别覆盖布局尺寸、双主题及DPR边界，以及浏览/详情/总览/片头、文字/彩色字体/滑尺、玻璃/纸面/透明合成、键鼠/触摸板/快捷键与焦点。核对源码效果与真实产品质量；发现锯齿、异常模糊等问题时定位实际原因并修复，保留缓存/资源差异。
- 媒体/部署：用匿名或用户指定本地样本真实播放、暂停/seek/停止/换曲，检查独立歌曲/BGM/音效、后台、默认/切换/消失设备与格式错误。部署包脱离开发Kit、Node/外部FFmpeg运行，检查Unicode/空格路径、资源/插件/字体/图标/可写目录，不改用户音乐文件。
- 性能/资源：同Kit/设备/fixture的正常VSYNC正式验收、按需未限速诊断及独立资源压力按performance.md执行，记录呈现事件、输入到Qt提交、冷启动/准备、资源峰值和重建/换片回落；有效结果按test-gates.md复用，解释历史Windows波动的适用范围和噪声。不可测显示/GPU完成写null或估算，不以离屏/CPU或其他设备成绩替代。
- 退出：上述源码、功能、实际质量、窗口、媒体、部署、性能与资源项目在可靠目标环境通过，受影响主线回归及文档完成后删除专项。缺环境、真实产品失败或未完成非像素检查保留具体任务；R33阶段完成不豁免本专项。

#### R19D4A-P · Bokeh 目标平台与生产集成回归

- 输入为R19D4A源码（起点f4e1610）、冻结基线经来源映射、锁定Three0.183.2/BokehShader、tests/fixtures/postprocess/bokeh-kernel.json及匿名9×64生产库。
- 静态入口`python3 tools/audit/audit-rhi-bokeh.py`覆盖COMMON-01–05/PERF-01–07/历史版本及三平台符号/hash；`python3 tools/assets/export-bokeh-kernel.py --check`核对锁定源hash/41点与公共/生产shader。核对RGB24线性深度/完整几何、nearest深度/linear颜色、41采样/aspect/±.011、单次最终ACES、仅80-byte动态前缀、资源所有权与关闭释放。目标真实编译/qsb/PowerShell适用解析仍须执行。
- Release Review ON先运行`ctest --preset qt-release-render -R '^rhi-bokeh-gpu$'`：直接QRhi真正离屏、不创建可见窗口，46状态含横竖/奇数目标、focus20/25/30、强度0/.0003/.0008/.0012及十次真实Postprocess重建/另一组clip范围，预期全部1799438解析像素的颜色误差≤既有3/255、有限值/alpha=1/HDR/完整RGBA16F/缓存/80-byte前缀/关闭释放通过。此重建不替代scene-graph或原生窗口恢复。
- 再按目标当前可靠隐藏能力运行`rhi-flicker-gpu`1200帧生产序列；无隐藏入口时使用`qt-release-visible -R '^rhi-flicker-gpu$'`并遵守当轮窗口策略。实际上传focus/aperture/maxblur/near/far/aspect与源状态逐帧匹配，完整DOF depth尺寸正确，原光学覆盖/有限值/上传/资源门槛不变。适用回归通过；共享修正回归主力平台Bokeh/AO/光学，范围按test-gates.md选择。BLUR-01与AA/尺度的源码结论见source-fidelity-review.md，目标回归见R19D4B2-P/R19D4C-P；原生窗口、正常VSYNC和正式性能按既有专项。

#### R19C2B2-P · 光学RHI目标平台回归

- 输入为R19C2B2实现（起点3c90d2d）、冻结基线映射、rhi-shell/visual.json、源GLB/Room/DFG/LTC及Review OpticsProbe。
- 静态入口`python3 tools/audit/audit-rhi-optics.py`，COMMON-01–05/PERF-01–07/平台逐符号及hash；核对D3D/GL clip、float readback、SRB、共享depth、resolve/mips、有限奇点及Review生产隔离。目标真实执行格式/PowerShell解析与shader生成。
- Release Review ON后执行`ctest --preset qt-release-visible -R '^rhi-shell-gpu$' --output-on-failure`，先按实际能力重评隐藏路径，遵守当轮窗口策略。预期61源状态、6排序/2隔离、重复ID拒绝、法向/深度和投影≤2px、1×/4×alpha中心≤1/255且黑边≤原一整数码值、透明洞不写depth、4×有部分覆盖、十次真实SG重建及窗口策略通过。10数学样本：LTC解析误差≤.001，其他max(1e-6,abs(expected)×.001)，含正视/零光/GGX/DFG/shadow及view-roll0/45的roughness导数；不放宽门槛。
- 共享修正回归主力平台受影响测试，范围按test-gates.md选择。正常呈现/VSYNC和原生窗口恢复按后置专项分工，连续FLICKER-01与BLUR-01的本机结论见source-fidelity-review.md，目标仍按各原专项补验。

#### R19C2C-P · 连续透明阵列目标平台回归

- 输入为R19C2C实现（起点ebfb320）、冻结基线映射、原GLB/光学资产、匿名9×64三封面库及rhi-shell/visual.json。
- 严格静态入口`python3 tools/audit/audit-rhi-flicker.py`逐COMMON-01–05/PERF-01–07/平台符号/hash；展开invariant跨目标qsb、相同world→nativeVP计算、稳定ID/depth顺序、capture隔离、Review probe不入产品和有界异步封面/读回。MSL2.1仅两个geometry顶点，其他shader仍保持原目标；任何Kit更新重新核对实际生成限定符与运行。
- 目标Release Review ON执行`ctest --preset qt-release-visible -R '^rhi-flicker-gpu$' --output-on-failure`，先按当前可靠隐藏能力调整实际入口，遵守窗口契约。1/7/8/9实体×1×/4×、640×480/1280×720双主题静止/波浪/浏览/总览进退16×60帧；480诊断样本missing/nonfinite必须精确0、shade>100，480正常光学保留代表输出实际检查，不恢复旧截图比较。实际DPR/目标/窗口策略、模式、资源generation与上传预算通过；十次SG重建、resize/封面及按test-gates.md选择的目标受影响回归、适用的官方qmllint/check-format通过。
- 共享修正回归主力平台受影响测试，范围按test-gates.md选择。原生窗口、真实呈现/VSYNC和不同设备须按“各平台最终实机矩阵”，不能将缺环境写通过；BLUR-01及本机已修闪烁的来源/范围见source-fidelity-review.md，未运行目标不得销项。

#### R19D1-P · SSAO 数学、标量读回与常量上传目标平台回归

- 输入为R19D1实现（起点2d3c9b1）、冻结基线映射、occlusion/visual.json的source状态80、seed417六份kernel/noise、匿名9×64生产库。
- COMMON-01–05/PERF-01–07及各平台符号/hash入口`python3 tools/audit/audit-rhi-ao.py`，静态展开RGB24精确背景、source法线unpack、TBN/投影/16/32/64完整累积、5×5/.22、native GL pack读取/同格式copy/逻辑stride、缓存边界与销毁、partial uniform更新及不存在闲置kernel纹理。工具使用对应host的qsb，Windows解析/构建仍须真实执行，qsb跨目标编译不是运行。
- Release Review ON后执行`ctest --preset qt-release-visible -R '^(rhi-ao-gpu|rhi-flicker-gpu)$' --output-on-failure`，按实际可靠隐藏能力重评分类、遵守当轮窗口策略。AO为3核×3倍率(1/.75/.5)×3尺寸(128×17/129×17/213×159)=27个背景恒1状态，实际raw/blur尺寸不加padding且格式R16F；复采alignment revision不变，常态三个pass更新请求240字节，无ao-kernel纹理；三核各81源公式点有接触、raw/blur≤1/255、.22合成≤2/255，十次真实SG重建有效。GL按实际PACK_ALIGNMENT决定诊断宽，不改产品或有限值门禁。
- 完整960帧生产序列在双主题/大小/静止/波浪/浏览/总览进退中正常输出与诊断均有效、missing/nonfinite精确0；记录实际平台/后端/DPR/尺寸/窗口策略。公共Effect五目标qsb及目标适用回归；共享修正回归主力平台受影响测试，范围按test-gates.md选择。正常VSYNC/设备/窗口/完整产品性能按“各平台最终实机矩阵”；fog/颜色、阴影、景深/AA/尺度及BLUR-01来源和门禁拆分见source-fidelity-review.md，目标回归仍按对应专项。

#### R19D2-P · 生产雾与线性颜色/捕获次序目标平台回归

- 输入为R19D2实现（起点4ab1e73）、冻结基线来源映射、锁定Three0.183.2的fog/meshlambert/meshphysical/WebGLMaterials/WebGLRenderer/OutputPass、rhi-shell/visual.json及匿名9×64生产库。
- `python3 tools/audit/audit-rhi-fog.py`展开COMMON-01–05/PERF-01–07逐符号/hash：实际相机距离而非target距离、detail-only发布、线性theme fogColor、负view-Z、smoothstep、纸面capture雾→resolve/mips→壳体自身雾→后处理→一次ACES/sRGB。832-byte std140保留既有offset、fog在816；不新增图形资源/生产readback/队列，保留GL/Metal/D3D必要API适配。五目标qsb仅静态翻译，不当作目标运行。
- Release Review ON后执行`ctest --preset qt-release-visible -R '^(rhi-fog-gpu|rhi-flicker-gpu)$' --output-on-failure`，按实际可靠能力重新选择隐藏分类并遵守当轮窗口策略。fog为双主题×7深度分数×启用/禁用×fog/ACES/sRGB三个实际生产helper，共84状态；RGBA16F数值与独立源数学误差<.001，不引入源截图/浏览器GPU oracle。十次真实SG失效重建后几何和实际fog uniform恢复，雾/颜色变化不得重建无关目标。
- 生产SceneItem双主题/640×480与1280×720/静止/波浪/浏览/总览进退/详情.5→1→0共1200帧：实际camera/detail与paper/shell已上传range/linear color/outputOptions一致；正常光学与诊断均有效，missing/nonfinite为0，保持正常动效、实体数量、源透明度和后处理。记录真实平台/后端/DPR/目标尺寸/窗口策略；适用回归通过，共享修正再跑主力平台受影响测试，范围按test-gates.md选择。可见窗口/正常VSYNC/输出设备与完整产品指标仍按“各平台最终实机矩阵”；阴影、景深/AA/尺度和BLUR-01来源及本机结论见source-fidelity-review.md，目标仍按对应专项补验。

#### R19D3A-P · 阴影投射、偏置与过滤目标平台回归

- 依赖R33交接、R19D3A源码及可靠Windows/D3D11、Linux/OpenGL最新原生Kit/编译器/窗口环境。输入为冻结基线（按历史映射）、锁定Three0.183.2 shadow源码、music-case-v040.glb、匿名rhi-shell/visual与9×64生产fixture；起点2f50812。
- 静态入口`python3 tools/audit/audit-rhi-shadow.py`输出COMMON-01–05/PERF-01–07和平台symbol/hash。核对source backface caster、D32F comparison/R32F canonical诊断、receiver顶点normalBias→插值→片元depthBias、五点Vogel/IGN、同Kit qsb五目标、Y/clip转换、缓存失效、最多两读回及所有权。
- 目标Release Review ON后按当轮窗口契约运行`ctest --preset qt-release-visible -R '^(rhi-shadow-gpu|rhi-opaque-gpu|rhi-shell-gpu|rhi-flicker-gpu)$' --output-on-failure`；若已有可靠隐藏能力先重新分类、真实隐藏执行。期望三map、1/7/8/9 caster边界、源几何投影与PCF、开关/运动/接收相机/灯色/重复捕获、十次真实SG重建，以及共用rhi-flicker生产序列通过。投影≤2物理阴影纹素，恒深面depth<2e-6，PCF≤2/255，原.32强度/.035 normalBias/-.0003 bias/radius4和D32F/R32F/单采样不变；实际paper/shell上传参数匹配源、未失效非批量caster专用uniform数为0。
- 产品地面与真实接收的目标回归另见R19D3B-P，景深/AA/缩放/BLUR-01来源和本机结论见source-fidelity-review.md，目标补验不得用其他组件代替。目标修正须回归本机阴影、透明覆盖/雾/封面及适用的官方qmllint/check-format，范围按test-gates.md选择；原生窗口同对象hide/restore、正常VSYNC和正式产品性能按既有专项，不用离屏SG重建冒充。

#### R19D3B-P · 产品地面与接收的目标平台回归

- 依赖R33交接、R19D3B实现（起点e9360b7）和可靠Windows/D3D11、Linux/OpenGL最新原生Kit/编译器/窗口环境。冻结基线经历史映射，输入为scene.ts的200×200/y=-4.63平面、三源主题色、锁定Three0.183.2 Standard/DFG/PMREM/PCF、rhi-shell匿名fixture与9×64生产库。
- 静态入口`python3 tools/audit/audit-rhi-floor.py`逐COMMON-01–05/PERF-01–07/平台符号/hash及LTC/GGX helper等价检查。核对一次共享几何上传、opaque kind6/Standard PBR、线性theme/fog、capture→resolve/mips→shell→AO/DOF→output、FrontSide与receive/cast、共享SRB/PSO/DFG/LTC、14语义字段最多2批/唯一纹理读回及renderer所有权。目标真实构建/qsb不可由本机跨目标翻译代替。
- Release Review ON构建后按测试窗口契约执行对应目标`ctest --preset qt-release-visible -R '^rhi-(shadow|flicker|shell|opaque|ao|fog)-gpu$' --output-on-failure`及按test-gates.md选择的受影响回归。rhi-shadow-gpu内含14组地面源数学、320×240/960×720、双主题及dusk源颜色、三map尺寸/开关、实际接收阴影、1×/4×采样、全/半AO与完整DOF附件、颜色变化不重建目标/阴影及十次真实SG重建；rhi-flicker-gpu真实SceneItem双主题/小大视口、静止/波浪/浏览/总览/详情1200帧逐帧检查有效floor提交、上传/资源和连续光学覆盖。
- 原投影≤2物理px、法向≤1/255、PBR/fog≤3/255、R19D3A原投射/PCF门槛不放宽；必须实际覆盖occluded>0及AO/full DOF ground depth。适用回归通过；共享修正回归当前主力平台地面/阴影/封面/光学/雾/AO，范围按test-gates.md选择。原生窗口同对象hide/restore、正常VSYNC和正式产品性能仍按既有专项，不把离屏SG重建或数值通过写为实机呈现通过；BLUR-01和总体DOF/AA/尺度来源及本机结论见source-fidelity-review.md，目标补验见R19D4A/B/C对应项。

#### R19D4B1-P · SMAA 目标平台独立回归

- 输入为R19D4B1实现（起点704a99d）、冻结基线按来源映射、锁定Three0.183.2 SMAAShader/SMAAPass与既有两个lookup、RhiSmaaAudit解析输入。
- `python3 tools/audit/audit-rhi-smaa.py`核对冻结生产调用者/锁定hash、三个完整helper、.1阈值/八步/源标量crossing偏移/gamma2.2、lookup原PNG、历史及COMMON-01–05/PERF-01–07/平台symbol/hash。目标同Kit真实构建/qsb仍须执行，跨目标翻译不代替目标GPU。
- Release Review ON执行`ctest --preset qt-release-render -R '^rhi-smaa-gpu$' --output-on-failure`。入口直接QRhi真正离屏，不创建可见窗口；34状态包含64×64/129×73/73×129、两个斜向/相交/矩形/低对比/局部对比抑制/静态清零、HDR/alpha及十次Postprocess真实重建。每后端277610个解析点分别检查三个实际生产pass：edges≤1/255，weights/blend≤既有颜色公式3/255、值有限、无反馈，八项资源的RGBA16F/RGBA8/单采样/无mip和240-byte实际uniform前缀/缓存/关闭释放通过。
- 共享修正回归主力平台SMAA/Bokeh/AO/光学，范围按test-gates.md选择。此项不代替B2的SceneItem/MSAA/resolve/旧门禁覆盖闭包或C的尺度/BLUR-01；scene-graph/native窗口恢复、正常VSYNC及完整产品性能按“各平台最终实机矩阵”。

#### R19D4B2-P · MSAA/resolve、组合资源图与恢复门禁的目标平台回归

- 输入为R19D4B2交付、冻结基线映射、R09B/R10/R10C/D/e98c798源码及`render-backend/visual.json`、`rhi-shell/visual.json`和匿名9×64生产库。依赖R33及可靠Windows/D3D11、Linux/OpenGL最新原生Kit/编译器/窗口环境；当前主力macOS隐藏验证不替代目标运行。
- 静态入口`python3 tools/audit/audit-rhi-resolve.py`输出COMMON-01–05/PERF-01–07、历史symbol/hash和三平台矩阵；目标须实际构建/qsb，不能以主机翻译或字符串命中代替。组件覆盖见source-fidelity-review.md。
- Release Review ON按test-window-policy选择当前可靠入口：先执行`ctest --preset qt-release-visible -R '^rhi-(postprocess|shell|flicker|ao|fog|shadow|opaque)-gpu$' --output-on-failure`，能可靠隐藏时重新分类并用对应render preset；另执行真正离屏`ctest --preset qt-release-render -R '^rhi-(bokeh|smaa)-gpu$'`。postprocess预期48状态（三尺寸×1/4样本×八组合）、实际RT/PSO样本/颜色RGBA16F及normal/depth RGBA8/独立单样本resolve、后处理SRB链/无反馈/关闭释放/缓存/80-byte前缀与十次真实SG失效通过；Shell原解析alpha/部分覆盖/黑边门槛不变。
- 保留64tile初始化16MiB、单tile262144bytes、稳态2MiB/第九tile延期、432实例与全可见容量、1/7/8/31/32/33/64/432边界、cover副本/atlas自身GPU读回/还原/逆序/resize/generation拒绝；Windows额外要求真实native UpdateSubresource boxes/bytes，其他后端不伪造native证明。实际SceneItem original/high双主题/大小/静止/波浪/浏览/总览/详情1200帧，真实组合图和输出无漏画/错误合成，保持源数量/动效/效果。
- 适用回归通过；共享修正回归主力平台受影响测试，范围按test-gates.md选择。`rhi-postprocess-audit 新目录 --profile`仅三批局部CPU提交诊断，GPU完成/显示时刻/driver显存未知保留null。原生同窗口hide/restore、正常VSYNC和完整产品120秒/最终设备仍按既有专项；目标尺度/BLUR的源码判定见source-fidelity-review.md的R19D4C，目标补验见R19D4C-P，不以移交豁免本机调查。

#### R19D4C-P · 目标尺度与BLUR修正的目标平台回归

- 依赖R33及R19D4C交付；输入为冻结基线经来源映射、`postprocess/visual*.json`九尺寸、`RhiProductionFixture.h`匿名9×64库和实际生产SceneItem。缺口为Windows/D3D11、Linux/OpenGL真实系统/最新原生Kit/目标GPU/窗口环境；macOS隐藏RenderControl的显式DPR不代替物理屏幕或跨屏事件。
- 静态入口`python3 tools/audit/audit-rhi-scale.py`生成冻结音乐最终CSS/resizeQuality、R09/R09B/R10C/D及D1–D4逐符号/hash、COMMON-01–05/PERF-01–07/平台矩阵。目标真实构建与同Kit qsb；修复对象为SceneItem显示逻辑尺寸→effective DPR→RenderQuality→所有物理附件→Qt合成，不得恢复重复参考舞台缩放或用截图分数改变源效果。
- Release Review ON后，按test-window-policy及当前可靠能力执行`rhi-scale-gpu`、`rhi-flicker-gpu`及组合/Bokeh/SMAA/AO/雾/阴影/壳体/不透明实际目标检查。macOS用`qt-release-render`，其他平台现有native入口用`qt-release-visible`；若已具可靠隐藏入口先重评。native尺度入口仅核对实际DPR；在真实DPR1/1.5/2的目标环境分别运行，保留真实值、同源相机/时间/ID、全部附件/格式/MSAA/resolve/mips、limited与最终合成比例。九尺寸/双主题、50/125/200 scale、高清租约与半AO/full DOF/源CoC及过滤门槛见rhi-integration.md，原门槛不放宽。
- 目标正常光学连续输出保持原数量/动效/景深/磨砂、无非预期低清放大/漏画/错误合成；诊断missing/nonfinite精确0。重复大小/DPR变化、十次SG重建及原生跨屏同窗口恢复时尺寸/租约/零稳态上传与资源回落有效；不得把显式target DPR或SG失效写为物理屏幕/原生恢复证明。适用回归通过；共享修正回归主力平台受影响测试，范围按test-gates.md选择。正常VSYNC、实际显存、120秒产品性能仍按R28测量入口与后置专项分工，未运行目标不记通过。

#### R19E-P · 浏览控件/滑尺的目标平台与原生输入回归

- 依赖R33及R19E交付；冻结输入经历史映射，`export-browse-ui.mjs`生成CSS结构、`export-ruler.mjs`生成144组数学/数字轨迹，均无源PNG。Windows/D3D11及Linux/OpenGL缺真实系统/最新原生Kit/GPU；物理键鼠/触控板、跨屏DPR及后续完整产品窗口/VSYNC仍需对应实机证明。
- 静态入口`python3 tools/audit/audit-browse-navigation.py`绑定R14/R15/R16页眉接线及Apple后续修正的符号/hash，展开COMMON-01–05/PERF-01–07与三平台分支；目标实际编译/qsb、PowerShell解析/FormatTools仍须运行。修复对象为Header最终竖屏song-cap、SVG实际窗口DPR、Ruler/Ordinal的唯一时钟/可见性、InputRouter背压及Canvas附件/恢复，不恢复四倍图标采样或任何图像评分。
- 目标Release Review ON后执行上述两个导出器；按窗口契约运行`ui-audit 输出目录`及`ui-audit --ruler 输出目录`，按test-gates.md分别覆盖布局尺寸、双主题及DPR边界；用`compare-browse-structure.py 原生report.json 冻结report.json 输出.json`核对锚点≤1逻辑px。滑尺20/30/60/120fps及1/120秒trace/数字轨迹误差≤1e-6；三API正常轮廓/裁切/淡隐、十次真实SG重建/33恢复捕获、静止/隐藏零paint、renderer归零与默认窗口策略必须通过。macOS用隐藏入口；目标有可靠隐藏能力时优先重新分类，正常呈现仍单独记录。
- 原生产品入口`yalahoo --verify-browse 输出目录`、`--verify-navigation 输出目录`（Windows包装verify-r14/r15.ps1读取本次Process.ExitCode及report）检查播放歌名/按钮位置、主题/动作、真实Qt输入receipt→navigation commit→对应帧提交、滚轮正反/有限端点/切列/滑尺/触摸/编辑焦点、浏览播放隔离和hide/restore。三平台实物输入及同对象窗口/跨屏/VSYNC按既有窗口专项执行；遮挡或节流无样本不能算通过，不未经授权提升前台。
- 适用回归通过；共享修正回归当前主力平台navigation/title-reels/ui-drawing/ruler-drawing，范围按test-gates.md选择。保持四份封面所有权/一个待提交目的地、固定槽/字轮面、ResourcePlan及异步读回上界；正式产品性能依performance.md与后置专项分工，不以局部CPU成本或离屏提交替代屏幕延迟。
