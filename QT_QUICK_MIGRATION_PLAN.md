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

- 当前主线：`R36`。一次只执行一个具备可靠环境、可收尾的执行单元；目标系统、剩余后端与物理设备缺口集中处理。历史来源与静态判定见source-fidelity-review.md。跨目标的验证结果不作为无源码依赖的阻塞条件。
- 开始前按代理实际能力估算代码增删、实施、构建、验证、文档和提交总耗时并简报。代码理想≤1000行、通常≤2000行，上限目标两三千行/一小时；预计约4000行或七八十分钟仍不能收尾时，必须先拆计划，明确接口/输入/门槛。执行中超出预估及时缩小边界，保留未完成任务，不追认超时为达标。
- 行数按新增与删除之和计，不能净增抵消或拆提交规避；生成脚本、文档、素材不计代码行数，但其执行时间计入。正常思考、工具等待、构建和调试均计时；仅断网/用户中止间隔扣除，恢复不重置累计时间。必要收尾允许小幅浮动。
- 每轮依据实际系统/CPU、最新Kit、后端/设备、匿名输入和窗口策略判断能力，按test-gates.md选择必要构建/功能/渲染/媒体/部署检查。性能先按performance.md严格静态分析，只对仍无法确定的具体问题最小实测，不因环境可用追加采样或压力。需要图形实测时macOS/Apple Silicon优先真正离屏；源码确定缺陷、可靠检查失败及确需的入口缺失必须修复。
- 只有独立缺环境或采集不可靠的子任务可严格审计后移交后置专项（统一归R36），保留原任务ID、目标/能力缺口、源码与输入、已知失败、修复对象、准确入口/预期结果、原门槛、依赖和受影响回归。移交不代表取得实际运行事实；性能移交仅保留静态未决问题，不复制历史采样矩阵或重复次数。已交后置专项默认集中处理；用户指定提前补验仅处理指定已实现边界。
- 已交付目标的受影响构建/部署回归按test-gates.md执行；R32-P保留Linux目标环境的待验闭包。R36按目标系统、后端与设备能力拆执行单元，缺环境时保留具体任务。

### 测试与窗口

- 后端选择：Windows默认D3D11，macOS默认Metal；显式D3D12、实际启用的Vulkan及OpenGL/GLES按真实目标能力执行。剩余GPU/窗口、质量、输入、生命周期与具体呈现问题统一归R36。各轮照常完成源码/算法/构建/模块和适用的跨目标qsb静态检查，保留OpenGL实现及原门槛；用户明确点名的单项复现按其指定执行。
- 验证优先复用已有有效结果；不影响行为的注释/文档/脚本等改动免测，逻辑改动只做必要检查，不因共享代码变更或提交惯性执行全量回归。Release分组与范围归[test-gates.md](docs/contracts/test-gates.md)；性能的架构→算法→实际实现严格静态审计及最低程度、最小范围实测，唯一执行[performance.md](docs/contracts/performance.md)。
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
| [performance.md](docs/contracts/performance.md) | PERF-01–07严格静态审计、最小实测条件、预算与结果解释 |
| [test-gates.md](docs/contracts/test-gates.md) / [test-window-policy.md](docs/contracts/test-window-policy.md) / [verification-artifacts.md](docs/contracts/verification-artifacts.md) | 门禁分组；验证窗口；输出/Git边界 |
| [source-defect-exceptions.md](docs/contracts/source-defect-exceptions.md) | 原项目缺陷/不合理行为的预期判定与跳过记录 |
| [source-fidelity-review.md](docs/contracts/source-fidelity-review.md) | 逐历史轮次来源、适配/补偿判定与覆盖索引，非实施规范或运行日志 |
| `docs/platforms/{windows,macos,linux}.md` | 平台操作/部署差异与执行入口；欠验归属和门槛集中在本计划后置专项 |

### 工具链与目录

- 最新原生Kit发现、ABI/模块闭包、同Kit工具、私有API编译探测和C++23能力均执行[native-bootstrap.md](docs/contracts/native-bootstrap.md)，不得固定Qt版本/安装目录或降级。
- Canvas Painter共用API、生命周期和选型执行[canvas-painter.md](docs/contracts/canvas-painter.md)；按实际等价性、质量、成本和维护性选择，缺能力明确定位，不因许可类别/技术预览标签放弃合适实现。
- 根CMake/presets；C++在`src/{app,core,motion,render,media,library,services,platform}`，QML在`qml/Yalahoo/Music/{components,pages,panels,scene}`，资产`resources/`，测试`tests/`，工具`tools/{build,assets,review,deploy,audit,launchers}`。普通启动器在根，演示/研究启动器在`tools/launchers/`；不提前创建未来轮次的空实现。
- `src/`子目录维护源清单；共用代码优先Qt，平台目录只放真实差异。QML用模块相对别名和`loadFromModule("Yalahoo.Music","App")`，非QML资源qrc `/assets/`，shader用qt_add_shaders/qsb；不依赖工作目录。构建`build/{release,debug}`、部署`out/{windows,macos,linux}`；本机配置/路径不提交。
- 源码按需执行`format`；提交前`check-format`、Release `source-format`及适用检查须有仍有效的通过结果，按test-gates.md复用，不把命令串当作每轮必跑流程；QML诊断只能用CMake官方`Yalahoo_qmllint`/`ui-audit_qmllint` targets。四空格、展开语句和冻结/第三方排除范围由code-style契约定义。

### 架构与产品接线

- 全部现有及新增QML控件遵守[controls.md的长期强制规则](docs/contracts/controls.md)：禁止QQC预制控件，完全自绘并按冻结来源理论值显式设置视觉属性；产品、探针、研究与Review同样适用。具体边界只在该契约维护。

- AppController组合模型、选择/展示/运动、媒体、偏好与Provider，禁止巨型业务类；稳定ID操作，导航与播放独立。模型通知在GUI，GUI→render仅不可变SceneFrame，render不访问QML/LibraryStore/IO；扫描/解码用受限worker，媒体探测用有事件循环的专用线程，异步按requestId/generation/entityId淘汰陈旧结果。
- 数据根/JSON/排序/搜索/Provider协议以library-data契约为准。专辑/曲目ID固定为`album-/track-`加原生绝对目录/直属单曲/文件路径UTF-8的SHA256前24位；跨系统不猜映射。旧数据仅显式只读导入，不移动原目录或导入PCM缓存，不写源歌曲标签。
- SceneAdapter/QML接口稳定，生产唯一RHI SceneItem；Quick3D仅Review，私有API留在RHI/字体适配边界。通道/格式/ABI与资源图分别由rhi-opaque/shell/postprocess/integration维护，接口/调用链与契约同步维护，不再重选框架。
- 源材质/几何/光场、局部LOD、原contain/透明留白、帧源/打断/减少动态与状态机均按专项契约；不得建立第二套常量/时间/播放/身份机制。跨API换算必须有源公式/单位依据。
- 生产滑尺用C++ Canvas Painter，标题当前glyph+彩色Quick Text；序章实现见opening.md，剩余目标实际输出归R36；仅生产路径存在具体缺陷或瓶颈时再考虑选型。列表/输入/焦点/Accessible由Quick承担；固定有界池/封面队列/上传预算、ResourcePlan及异常失败路径按对应契约，不以局部benchmark豁免设计缺陷。

## 验收要点

- 匿名fixture覆盖空/单/奇偶/短长列/超过12张列/超过432库、横竖方/alpha封面、长中文/下伸英文/括号/emoji/组合字。Web/Qt共用viewport/DPR/theme/quality/arrayMode/IDs/selection/motionTime/事件JSON，不读私人库；Review冻结时钟CLI不进入普通产品。
- 尺寸1920×1080、1280×720、995×858、960×900、930×900、390×844、320×720、2560×1080、3440×1440，代表DPR1/1.5/2；布局锚点≤1逻辑px、三维投影≤2逻辑px、double运动≤1e-6，时序≤一个60Hz帧。20/30/60/120fps和长帧，轨迹以1/120秒源状态采样。
- 打断覆盖立即返回/退字连选、三次换片与反向、搜索替换/取消、总览切列、改速/途中减少动态、扫描替换及跨1.05分界；真实清晰度/轮廓/合成、功能、数学和资源分别验收。
- 性能指标、采样/临界复核、预算和历史record-only例外统一见performance.md；后台视觉停止且歌曲继续，资源压力执行范围见test-gates.md。

### 严格静态审计

工具置`tools/audit/`，输出只留本地。复用仍有效的源码结论，只审查本轮实际缺口及受影响调用链，不以缺新报告触发完整重审。需要更新的平台矩阵含checkId/requirement/platform/sourcePath/lineOrSymbol/evidence/result/limitations和内容hash；自动检查后必须推演架构边界、算法最坏复杂度/工作量及真实调用链/分支/错误路径，字符串命中、helper或文件清单不足以证明实现。性能结论按performance.md区分静态已确定、缺陷与具体未决项；静态可确定者不再实测，不能因缺硬件耗时数字生成泛化补验。源码可确定缺陷须修复，静态与真实运行事实分开。

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

### R36 · 目标环境、剩余后端与实物验收

- 依赖现有生产实现、有效组件结果及下方任务；只处理尚缺的目标事实。图形运行剩余目标为Windows的D3D11、显式D3D12/OpenGL及实际启用的Vulkan，Linux的OpenGL和实际启用的Vulkan（真实X11/xcb与Wayland），以及缺少启用特性Kit/SDK/MoltenVK的macOS Vulkan；macOS生产bundle的严格置底无激活可见入口沿下方MAC-VISIBLE-3D处理。非图形构建、文件系统、媒体、部署与三平台物理设备同属本轮。
- 能力准入：Windows/Linux须有各自原生系统、最新完整Kit及所需GPU/桌面会话；macOS跨设备部署须有另一台兼容设备，Vulkan须有启用该特性的原生Kit、SDK/MoltenVK。物理输入、跨屏DPR/HDR/EDR、设备拔插与听感须有对应实物和可靠操作条件，不能用注入事件或离屏DPR替代。
- OpenGL使用`QSG_RHI_BACKEND=opengl`或工具`--backend opengl`并核对真实API；Windows/Linux须有原生系统、最新完整Kit和实际GPU。Linux同系统/Kit/GPU/后端的无会话依赖结果共用，xcb/Wayland分别补上下文/swapchain、窗口/输入/焦点/DPR与呈现差异；各组件准确入口、匿名输入和原门槛沿下方五组，不复制矩阵。macOS尚缺的物理输入、跨屏和设备事实沿相应物理任务保留。
- 原ID与完整闭包沿下方五组保留：BP-P/R30-P/R31-P/R32-P的构建部署与日志；R21-P/R22-P/R24-P/R25-P的目标文件系统/媒体与声音；UI-CONTROLS-P及文字/详情/总览/序章/整场产品的目标输入、显示与窗口事实；R19A/B/C/D各`-P`的目标GPU差异；HDR-OUTPUT-P。各组的源码、匿名输入、准确入口、依赖及原退出门槛是本轮清单，不另复制一套矩阵。
- 只在当前必要检查确被无关回放/压力阻塞时，在对应任务内解开最小分支；不设独立工具改造轮次。共享修正仅回归受影响生产后端和产品状态；全部独立缺口获得相应目标事实后删除本轮。

## 后置专项输入与退出条件

以下五组是R36的剩余任务；表中原ID用于追溯断言，不各自启动一套验证。一次只实施其中一个可收尾单元。每次先确定已有证据、实际缺口、最小入口与退出条件；同一结果覆盖多个ID，完成即删除相应待办。公共构建/格式/官方qmllint及矩阵选择只按[test-gates.md](docs/contracts/test-gates.md)，性能只按[performance.md](docs/contracts/performance.md)，窗口只按[test-window-policy.md](docs/contracts/test-window-policy.md)。缺目标实际事实继续留待办，未知GPU完成/显示时刻/driver显存保持null。

### 目标构建、部署与后端/日志

- 归属：BP-P、R30-P、R31-P、R32-P及后端/日志目标检查。输入为根CMake/生产QML清单、`cmake/Deployment.cmake`、qrc资产、GraphicsBackend/BackendCapabilities、LoggingSession、平台启动/部署工具与匿名库。静态展开COMMON-01–05、语言/Qt/QML实践及适用平台分支；已确定的跨平台算法与性能结论共用，不新建逐组件审计工程。
- 每个目标Kit准备一次所需Release产物及去重检查；生产Review OFF的加载闭包与开发Review工具分开。构建/模块、目标编译器、资源打包和脚本成功路径必须在目标执行；qsb跨目标翻译不能代替目标运行。Windows PowerShell核对本次Process.ExitCode和当前report，缺报告/错误必须失败；不为每个历史包装入口重复构建。

| 目标/归属 | 准确入口与独立退出事实 |
| --- | --- |
| Windows · R30-P/BP-P | 操作按[windows.md](docs/platforms/windows.md)；`pwsh -NoProfile -File tools/deploy/windows.ps1`，`python tools/audit/audit-windows-deployment.py --scan-qml --build-directory build/windows-release`及包内`verify-windows.ps1`。真实MSVC/最新Kit/C++23、PE/图标、同Kit windeployqt/官方VC运行库、插件/媒体/字体/shader/许可与相对路径完整；包复制到无开发Qt的干净系统及中文/空格路径，从包外加载，不借开发PATH。 |
| macOS · R31-P | 输入为`out/macos/`完整生产包，操作按[macos.md](docs/platforms/macos.md)。整包在另一台兼容目标设备的中文/空格路径从包外运行`python3 verify-macos.py --output 绝对新目录`，核对实际映像/架构/相对资源/签名；实机核对Finder品牌、物理AppKit菜单/Command、系统目录选择与输入法。 |
| Linux · R32-P/BP-P | 操作按[linux.md](docs/platforms/linux.md)；`sh tools/build/linux.sh`、`sh tools/deploy/linux.sh`、`python3 tools/audit/audit-linux-deployment.py --build-directory build/linux-release`、`python3 tools/deploy/verify-linux.py --output 绝对新目录`。原生C++23/最新Kit、ELF/ldd/相对RPATH/qt.conf、xcb/Wayland/媒体/CanvasPainter及许可/权限完整；整包在无开发Qt的兼容CPU/系统ABI及中文/空格路径从包外加载。 |
| 后端与日志 | 使用Release `graphics-backend`、`logging`、`logging-terminal`及普通部署包。Windows显式D3D11/12、Windows/Linux的OpenGL与已启用Vulkan、macOS Vulkan核对真实API；Qt特性或设备/格式不支持时明确报错并非零退出。Linux默认Vulkan不可用时警告并回退OpenGL，显式请求不回退。目标成功路径需真实输出，不能只看初始化。日志目录/上界/轮转按logging.md；Windows真实控制台VT颜色/UTF-8及重定向正常。 |

部署验证失败保留旧包；加载探针仅证明实际报告中的加载事实。产品功能、设备与窗口检查直接共用下方正常产品会话，不在部署后再跑整套组件/性能矩阵。macOS当前Kit未启用Vulkan，缺对应原生Kit/SDK/MoltenVK；未具能力时保留该具体缺口。

### 文件系统、媒体与数据

- 归属：R21-P、R22-P、R24-P、R25-P。依赖上述目标构建；Windows/Linux尚缺原生文件系统/媒体插件实际执行，三平台尚缺明确列出的物理设备与听感事实。输入仅用DirectoryScannerTest/MediaMetadataTest/PlaybackTest/AudioAmbienceTest生成的匿名目录、标签/图像、WAV/损坏输入及既有BGM；不写用户歌曲。

| 对象与权威契约 | 最小入口及退出条件 |
| --- | --- |
| DirectoryScanner/LibraryStore · R21-P · library-data.md | `directory-scanner`/`library-data`/`library-qml`中尚缺的目标分支；`tools/audit/audit-library-scan.py`，指纹语义需要核对时用`tools/review/verify-scan-fingerprint.mjs`。核对原生UTF-8路径/ID、去重/重扫/取消/原子缓存；Windows实际UNC/junction/ACL，Linux大小写/权限/只读缓存及循环链接。无能力项保留具体缺口。 |
| MediaMetadataProbe/ArtworkStore/LegacyImporter/PreferencesStore · R22-P · library-data.md | `media-metadata`及受影响缓存/导入测试，`tools/audit/audit-media-metadata.py`。真实无输出/无play探测的取消/晚到/超时/退出，封面优先级及解码保护、内容寻址/符号链接拒写、显式只读旧JSON与未知字段/坏文件/原子保存错误符合契约；Loaded不代表可播放。 |
| PlaybackController及三路声音 · R24-P/R25-P · playback.md | `playback`/`audio-ambience`、`tools/audit/audit-playback.py`/`audit-audio-ambience.py`；GUI共用一次`playback-qml`或`ui-audit --playback 新目录`。普通Qt公开API、队列/自然结束/暂停/seek/错误与导航隔离正常，歌曲/BGM/音效独立；静音事件不补合成音源。 |

物理声音检查在每个系统的一次正常产品会话中合并：真实支持样本的播放/暂停/seek/换曲/自然结束及损坏/不支持原因；默认改选、显式设备保持、消失回退/无输出/重连及加载中换曲换设备，保持暂停/停止意图与有效位置/音量；BGM/音效走各自规定路由，最小化继续、关闭停止，听感无意外重叠/突变。既有有效状态测试复用，不能以无声加载、程序进度或模拟设备替代尚缺的实物事实。

### 产品功能、文字与物理输入/显示

- 目标边界：Windows/Linux保留对应后端实际输出及产品接线；macOS仅保留MAC-VISIBLE-3D的默认置底无激活显示；三平台保留下方物理输入/显示，Windows/Linux另保留原生窗口生命周期。
- 归属：UI-CONTROLS-P、R16B2W/R16B3/R16M3-S、R17-P/R18-P/R19-P/R19E-P/R20-P/R23-P/R26-P/R27-P/R29-P、MAC-VISIBLE-3D。依赖目标构建和现有生产接线；输入为各组件既有匿名fixture、冻结数学/CSS结构与qrc封面。生产文字glyph+彩色Quick Text、滑尺C++ CanvasPainter、序章沿当前路径验收，不安排默认候选路径比较或成本测试。
- 先将已有产品/组件报告按实际断言对照，只补缺口。源码数学、信号/状态可复用同平台非图形结果；布局/主题/DPR按test-gates.md选独立边界。目标真实输出与实际输入仍须对应目标证据，不能以源码审计假称画质通过。

| 共同检查及归属 | 修复对象、输入与入口 | 独立退出条件 |
| --- | --- | --- |
| 面板/控件 · UI-CONTROLS-P、R23-P/R26-P/R27-P | Library/Search/Settings/ExperimentPanels、TrackFocusController、PreferencesStore/OfflineMetadataProvider；LibraryAudit/Settings/Experiment匿名输入。选择`controls-qml`、`library-panels-qml`、`settings-qml`、`experiments-qml`中实际缺项，或对应`ui-audit --library/--settings/--experiments`入口；审计分别为audit-controls/library-panels/settings/experiments.py。 | 模态/滚动/焦点、搜索取消与最终定位、资料来源/离线状态、设置保存/未知字段与当前媒体连续、四实验模式/改速/临时对照恢复符合controls/library-data/lighting/motion契约；相同控件断言在各宿主复用，宿主特有路由独立保留。 |
| 文字/页眉/滑尺 · R16B2W/R16B3/R16M3-S/R19E-P | TitleReel/TransportTitle/Header、Ruler/Ordinal/InputRouter；`tests/fixtures/text/titles.json`及144组滑尺源轨迹。`export-title.mjs`→`verify-title.mjs --backend 目标 --dpr 实际值 --source 源目录 --output 新目录`、`title-reels`、`ui-audit --ruler`/普通ui-audit及export-ruler/export-browse-ui/compare-browse-structure；源码入口audit-title-source/audit-browse-navigation.py。 | 字体身份、连字/组合字/emoji、字形/断行/省略/裁切、页眉结构与播放隔离、自然宽度通知/隐藏完成与恢复取消、刻线/clip/淡隐/背压符合title-reels/navigation/browse-ui；所有权/缓存/隐藏工作量先静态确定，未决运行行为才最小观察。 |
| 详情/总览 · R17-P/R18-P/R19-P | Presentation/DetailMotion/ReturningCasePool/PointerDragController、Overview与SceneItem；presentation/detail-golden/overview/source-golden匿名输入。`ui-audit --presentation`、`verify-detail.py --backend 目标 --runs 1 --output 新目录`、`verify-overview.py --visual --backend 目标 --runs 1 --output 新目录`按缺项选择；audit-presentation/detail/overview/detail-overview-source及对应结构导出/比较入口。 | ready/revision/四种archiveRoute、连选最后意图/反向/拖动取消、sticky tabs/行高/委托虚拟化、总览两步点击/跨列收拢与反转/碰撞准入（标签240ms退场与420ms入口收拢的衔接、计数block宽度）、焦点恢复/定位不改播放，遵守presentation/detail-motion/overview；产品回放与组件报告共用。 |
| 序章与产品联动 · R20-P/R29-P | OpeningController/OpeningInk、MusicStage/SceneItem；opening/source-golden.json与qrc封面/匿名库。`opening`、`ui-audit --opening`及audit-opening/export-opening-golden；仅组件证据未覆盖的跨组件联动才选`verify-gui.py --backend 目标 --output 新目录`/audit-gui.py。 | 输入锁/跳过/重播/减少动态/一次交接、遮挡零三维submit和恢复正确，产品浏览→详情→总览→搜索/片头的真实接线、透明合成与资源容量符合opening/rhi-integration；不为汇总已有组件结果另跑完整GUI矩阵。 |
| macOS默认置底无激活显示 · MAC-VISIBLE-3D | VerificationWindowMac::NativeWindowGuard/prepareNativeApplication、App.qml；`python3 tools/build/verify-detail.py --backend opengl --runs 1 --output 新目录`的匿名库。已知失败输入为`verification/r35-detail-native/run-1/report.json`（原生应用hidden、窗口不在屏幕且无曝光/帧）与`verification/r35-detail-accessory/run-1/report.json`（进程激活）。依赖可靠观察自身窗口且不干扰工作区的macOS会话，修复对象仅为默认验证启动/显示策略。 | 默认策略下实际OpenGL窗口show/提交、置底、拒key/鼠标、前台PID与完整激活/焦点历史同时合规，恢复后仍遵守该策略。保持普通产品行为；无可靠条件或发生激活时不得宣告通过、提高层级或放宽门槛。 |

物理检查与部署、媒体共用同一目标产品会话：键鼠/触控板pixelDelta/快捷键、系统中英文与emoji组合输入、剪贴板/撤销、目录选择、辅助技术角色/名称、焦点/模态隔离；Windows/Linux另核对首次show、同对象hide/minimize/restore、实际surface变化与退出；真实跨屏DPR后的SceneItem/Canvas/文字尺寸/清晰度和缓存恢复。不同系统/会话的事实分别保留，隐藏target DPR及注入Qt事件不能冒充物理设备。

退出：按组件权威契约核对布局≤1逻辑px、投影≤2逻辑px、double轨迹≤1e-6及原时序/语义容差；自身输出无漏画/错误裁切/合成、锯齿或异常模糊，QML/shader无错误。源码可确定的复杂度/资源上界/取消与释放直接以静态结论收口；只有具体未决运行行为才补必要转换，不附带默认十次SG/root重建、三十次换片或成本采样。

### 图形算法与生产接线

- 目标边界：下表图形运行只保留Windows/Linux及具备原生Vulkan能力的macOS目标差异；物理屏幕事实由产品/显示任务维护，不重复GPU算法检查。
- 归属：R19A/B/C/D各`-P`。依赖目标构建/真实GPU；输入为冻结生产源码、`resources/manifests/`、各组件现有源数学fixture及匿名RhiProductionFixture，不默认生成完整大封面库。审计与适配判定索引为[source-fidelity-review.md](docs/contracts/source-fidelity-review.md)，算法/格式/容差仅由下表权威契约维护。
- 先审生产入口到实际上传/着色器/附件/合成/释放，复用已有效的源公式与资源结论；只为目标格式、驱动、实际光学/合成等尚缺事实选对应入口。以下为定位表，不是每目标逐项必跑清单；工具的固定状态数、1200帧序列和十次重建均非默认任务数。共有生产状态可同时证明各pass接线，无需另跑同一连续序列。

| 原ID | 源码/输入与权威契约 | 定向入口及尚需目标事实 |
| --- | --- | --- |
| R19A-P | PrepareAssets、字体物理表/QImageReader、封面租约；assets/branding/covers | `prepare-assets --verify-inputs`，assets/branding/cover-cache及audit相关源检查；真实平台字体/ICC/缺图/alpha与插件。源保护负向检查只在隔离副本进行。 |
| R19B-P | FrameClock/SceneAdapter、ArchivePool/RealColumns、motion/scene/archive-pool/real-columns fixture与契约 | 同名数学测试/导出器、`archive-pool-gpu`/`real-columns-gpu`中尚缺的目标分支；真实投影、稳定身份/两阵列/记忆/库替换、封面区域与容量。 |
| R19C1-P/R19C2A-P | ShellMaterial/SelectionLighting/RowLightController及MaterialReview；materials/parameters、lighting/golden、single-case/visual输入；materials/lighting | audit-material-parameters/material-review、`rhi-material-parameters`/`lighting`及必要的runtime-materials/runtime-lighting；真实参数上传与材质/纸面/光场。Quick3D仅Review，不成为生产RHI的重复验收。 |
| R19C2B1-P/R19C2B2-P | RhiItem/OpticsProbe、原GLB/Room/DFG/LTC、rhi-shell/visual；rhi-opaque/rhi-shell | audit-rhi-opaque/optics，`rhi-opaque-gpu`/`rhi-shell-gpu`；实际深度/法线/投影、排序/捕获隔离、MSAA部分覆盖/透明洞/alpha黑边与光学解析。 |
| R19D1-P/R19D2-P | SSAO/Fog生产helper与uniform、occlusion/visual/kernel和rhi-shell/visual；occlusion/postprocess | audit-rhi-ao/fog，`rhi-ao-gpu`/`rhi-fog-gpu`；R16F及奇数宽stride/GL pack、标量/颜色读回、真实camera/detail/线性fog上传与捕获→最终输出次序。 |
| R19D3A-P/R19D3B-P | Shadow及productFloor、源shadow/Standard/GLB/地面主题参数；rhi-opaque | audit-rhi-shadow/floor、共用`rhi-shadow-gpu`；D32F/R32F、normalBias/PCF、地面实际occluded>0、PBR/fog/完整AO与DOF depth。地面不另跑整套阴影。 |
| R19D4A-P/R19D4B1-P | Postprocess/Bokeh/SMAA、postprocess/bokeh-kernel与锁定lookup；postprocess | audit-rhi-bokeh/smaa、export-bokeh-kernel --check及真正离屏`rhi-bokeh-gpu`/`rhi-smaa-gpu`；真实生产pass解析颜色、HDR/alpha/过滤/边界。 |
| R19D4B2-P/R19D4C-P | Postprocess/AtlasUploadQueue、SceneItem有效DPR→RenderQuality、render-backend/visual与postprocess/visual；rhi-postprocess/rhi-integration | audit-rhi-resolve/scale、`rhi-postprocess-gpu`/`rhi-scale-gpu`；实际MSAA/resolve/无反馈资源图、附件尺度/保护/CoC/租约与tile上传。Windows原生UpdateSubresource boxes/bytes仅该目标可证明；其他后端不伪造。 |
| R19C2C-P及上述pass生产接线 | SceneItem→RhiItem实际生产阵列/几何与光学；rhi-shell/rhi-integration | audit-rhi-flicker及必要的`rhi-flicker-gpu`所选状态，或已有产品输出/上传报告；共同证明跨批量边界、运动/进退中的实际uniform与连续透明覆盖，missing/nonfinite精确0、shade>100，保持源实体数量/效果，不默认完整1200帧。 |

退出：相关实际输出满足所属契约的原公式、格式、采样、颜色/几何容差及容量/上传边界；静态结论与GPU实际事实分开。无未决项不补性能/缓存成本/重复释放压力；已有实际失败仍须修复并最小复验，未执行目标不记通过。

### HDR输出与系统事件 · HDR-OUTPUT-P

- 依赖真实目标系统、显示设备及输出能力；输入为DisplayOutputState/Monitor、WindowsDisplayQuery/LinuxColorManagement/MacDisplayQuery和DisplayOutputTest的5×1颜色。契约为[display-output.md](docs/contracts/display-output.md)。主机静态/qsb不能证明其他系统编译或显示事实。
- Release的DisplayOutputTest、`display-transfer-gpu`及需要当轮可见授权的`display-output-visible`只补缺项。Windows核对HDR开关/跨HDR-SDR屏/SDR brightness slider和nits/80，WCG SDR不误判；Linux核对真实Wayland颜色协议/PQ或linear swapchain及reference white/target luminance通知、无协议/X11的SDR和Vulkan失败回退；macOS核对亮度/电源引起的current EDR及跨屏恢复。
- 退出：查询失败清旧状态、参数变化不重建/模式切换保留QML与音频、epoch/释放正确，真实通知与输出符合契约。R36按上述剩余目标执行；OpenGL保持SDR，Windows/Linux需真实输出事实，macOS仅在共享变更影响时回归。不附带大型阵列、性能A/B或整场矩阵。
