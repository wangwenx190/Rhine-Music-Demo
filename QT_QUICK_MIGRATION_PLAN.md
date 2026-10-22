# yalahOO!!! · Qt Quick / QML 原生迁移执行计划

## 优先级与源码基准

- 优先级：用户最新明确要求 → 本计划 → `AGENTS.md`及专项契约 → 冻结历史资料。冲突须更新唯一权威原文及引用，不以多份互斥规则叠加解决。
- 外观、几何、相机、字体、材质、光照、特效、动画与合成以冻结生产源码的实际调用、最终override和理论效果为准；新品牌及用户固定差异优先。冻结身份、路径映射、匿名输入与导出由[web-baseline.md](docs/contracts/web-baseline.md)唯一维护。
- 禁止以截图逐像素比较、SSIM/RGB/ROI/边缘带评分或浏览器GPU字节相等驱动实现、调参、选型、诊断或验收。数学/结构、实际输出质量、功能和设备事实分别证明；具体判定归[test-gates.md](docs/contracts/test-gates.md)。
- 不复刻冻结原项目的明显缺陷或不合理行为。能由生产调用及上下文推断预期时实现正常预期，否则跳过该特性；来源、依据、决定及边界唯一记入[source-defect-exceptions.md](docs/contracts/source-defect-exceptions.md)。

## 文档维护

- 本计划只管公共执行边界、职责索引和剩余任务；技术规范归专项契约，平台文档只管操作，README只作使用导航。每个主题有一份权威原文，其他文档用链接指向它；入口摘要不是第二份规范。
- 当前权威文档范围为根入口、本表索引的契约/平台文档及原生资源说明。`legacy-web/`中的文档是冻结历史来源，只在其原版本语境解释，不承担原生现状或门禁职责，不改写以追赶原生代码。manifest/数学JSON是机器输入，维护来源与数值，不另设一份文字参数表。
- 文档与当前代码同步，不保留过程日志、过时完成成绩、未实现接口或失效命令。完成任务直接删除，编号不重排。未完成项保留修复对象、目标能力缺口、输入、依赖、准确入口、预期与受影响回归。
- 经验仅保留“明确触发条件 → 核心方案”，放在所属专项契约，已有规范覆盖者不另复制；未决问题写入下方任务。
- 确认退役的原生设施须连同实现、文档、测试、脚本、依赖及构建接线完整移除；不留兼容/占位/转发入口或专门禁止其重入的测试。冻结来源和产品明确支持的只读旧数据导入是保留的独立范围。
- 不自动发布、上传私人数据、改发行标签或写用户歌曲；作者、来源和许可完整保留。运行输出/暂存/提交唯一执行[verification-artifacts.md](docs/contracts/verification-artifacts.md)。

## 执行顺序与范围

- 当前主线`R36`。一次选择一个具备可靠环境、可收尾的执行单元；用户明确指定的其他工作优先。跨目标且无源码依赖的缺环境事项不阻塞当前单元。
- 开始前评估增删代码、实施/构建/验证/文档/提交总耗时并简报。常规代码理想≤1000行、通常≤2000行，目标两三千行/一小时；预计约4000行或七八十分钟不能收尾先拆接口、输入与退出条件。行数计新增加删除，生成脚本/文档/素材不计代码行数但耗时计入；正常思考、等待与调试计时，中止/断网间隔除外。用户可按轮豁免。
- 依实际系统/CPU、最新Kit、后端/设备、匿名输入与窗口能力选择检查。构建/功能/格式/qmllint和结果复用归[test-gates.md](docs/contracts/test-gates.md)及[code-style.md](docs/contracts/code-style.md)，性能静态优先与最小实测归[performance.md](docs/contracts/performance.md)，窗口授权与恢复归[test-window-policy.md](docs/contracts/test-window-policy.md)。不将工具完整状态数当作执行单元规模。
- 源码可确定缺陷、可靠失败及必要入口缺失当轮修复。只有独立缺环境或采集不可靠的事项可移交R36，保留原ID、能力缺口、源码/输入、已知失败、修复对象、准确入口/预期、原门槛、依赖及回归；移交不表示取得运行事实。性能只保留具体静态未决问题。

## 专项契约与职责

下列文档各管表中职责；复现命令放所属组件，公共规则只在对应公共契约维护。来源hash/数值输入以其引用的JSON和manifest为准，当前Kit/运行成绩不写成永久平台限制。

| 文档（`docs/contracts/`） | 唯一职责 |
| --- | --- |
| [product.md](docs/contracts/product.md) | 产品定位/支持范围、技术身份、顶层组合与目录职责 |
| [settings.md](docs/contracts/settings.md) | 设置路由、偏好schema/持久化/导入与实例仲裁 |
| [code-style.md](docs/contracts/code-style.md) | 源码格式、格式工具及官方qmllint使用 |
| [cpp23-best-practices.md](docs/contracts/cpp23-best-practices.md) | C++23语言、资源、类型与接口实践；不重复构建/格式门槛 |
| [qt-best-practices.md](docs/contracts/qt-best-practices.md) | Qt对象、信号、线程、模型与值语义实践 |
| [qml-best-practices.md](docs/contracts/qml-best-practices.md) | QML/C++职责、声明式绑定、类型、视图与视觉生命周期实践 |
| [native-bootstrap.md](docs/contracts/native-bootstrap.md) | 最新原生Kit发现、C++23探测、构建/模块加载与启动器 |
| [web-baseline.md](docs/contracts/web-baseline.md) | 冻结来源解析、匿名输入和基线重建 |
| [assets.md](docs/contracts/assets.md) / [branding.md](docs/contracts/branding.md) | 资产导入/字体/功能图标与来源保护；原创品牌资产 |
| [playback.md](docs/contracts/playback.md) | 普通歌曲Qt播放、保留队列、进度与输出设备/恢复 |
| [library-data.md](docs/contracts/library-data.md) | 稳定身份、本地扫描/存储、排序/搜索与资料Provider接口 |
| [motion.md](docs/contracts/motion.md) / [scene.md](docs/contracts/scene.md) | 唯一帧源/运动数学；布局/相机/坐标/拾取 |
| [opening.md](docs/contracts/opening.md) | 二维序章/三维开场、事件就绪、遮挡提交与交互交接 |
| [materials.md](docs/contracts/materials.md) / [lighting.md](docs/contracts/lighting.md) | 源材质/纸面语义；世界光场/主题/源单位 |
| [canvas-painter.md](docs/contracts/canvas-painter.md) / [browse-ui.md](docs/contracts/browse-ui.md) | Canvas Painter API、生命周期与资源语义；浏览静态布局/遮罩/图标合成 |
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
| [source-fidelity-review.md](docs/contracts/source-fidelity-review.md) | 冻结模块到当前实现的适配理由与来源索引，非技术参数或运行成绩 |
| `docs/platforms/{windows,macos,linux}.md` | 平台操作/部署差异与执行入口；欠验归属和门槛集中在本计划后置专项 |
| `resources/audio/README.md`、`resources/demo-covers/README.md` | 各自音频/演示图像的作者与素材来源；不维护运行实现 |
| `resources/licenses/NOTICE.md`、`materials-NOTICE.md`、`qt-playback-reference-NOTICE.md` | 原生随包总权利说明；移植算法及外部Qt参考各自的许可/来源 |

## R36 · 目标环境、剩余后端与实物验收

- 依赖现有生产实现、有效组件结果及下方任务；只处理尚缺的目标事实。图形运行剩余目标为Windows的D3D11、显式D3D12/OpenGL及实际启用的Vulkan，Linux的OpenGL和实际启用的Vulkan（真实X11/xcb与Wayland），以及缺少启用特性Kit/SDK/MoltenVK的macOS Vulkan；macOS生产bundle的严格置底无激活可见入口沿下方MAC-VISIBLE-3D处理。非图形构建、文件系统、媒体、部署与三平台物理设备同属本轮。
- 能力准入：Windows/Linux须有各自原生系统、最新完整Kit及所需GPU/桌面会话；macOS跨设备部署须有另一台兼容设备，Vulkan须有启用该特性的原生Kit、SDK/MoltenVK。物理输入、跨屏DPR/HDR/EDR、设备拔插与听感须有对应实物和可靠操作条件，不能用注入事件或离屏DPR替代。
- OpenGL使用`QSG_RHI_BACKEND=opengl`或工具`--backend opengl`并核对真实API；Windows/Linux须有原生系统、最新完整Kit和实际GPU。Linux同系统/Kit/GPU/后端的无会话依赖结果共用，xcb/Wayland分别补上下文/swapchain、窗口/输入/焦点/DPR与呈现差异；各组件准确入口、匿名输入和原门槛沿下方六组，不复制矩阵。macOS尚缺的物理输入、跨屏和设备事实沿相应物理任务保留。
- 原ID与完整闭包沿下方六组保留：BP-P/R30-P/R31-P/R32-P的构建部署与日志；R21-P/R22-P/R24-P/R25-P的目标文件系统/媒体与声音；UI-CONTROLS-P及文字/详情/总览/序章/整场产品的目标输入、显示与窗口事实；R19A/B/C/D各`-P`的目标GPU差异；HDR-OUTPUT-P。各组的源码、匿名输入、准确入口、依赖及原退出门槛是本轮清单，不另复制一套矩阵。
- 只在当前必要检查确被无关回放/压力阻塞时，在对应任务内解开最小分支；不设独立工具改造轮次。共享修正仅回归受影响生产后端和产品状态；全部独立缺口获得相应目标事实后删除本轮。

## 后置专项输入与退出条件

以下六组是R36的剩余任务；表中原ID用于追溯断言，不各自启动一套验证。一次只实施其中一个可收尾单元。每次先确定已有证据、实际缺口、最小入口与退出条件；同一结果覆盖多个ID，完成即删除相应待办。公共构建/格式/官方qmllint及矩阵选择只按[test-gates.md](docs/contracts/test-gates.md)，性能只按[performance.md](docs/contracts/performance.md)，窗口只按[test-window-policy.md](docs/contracts/test-window-policy.md)。缺目标实际事实继续留待办，未知GPU完成/显示时刻/driver显存保持null。

### 目标构建、部署与后端/日志

- 归属：BP-P、R30-P、R31-P、R32-P及后端/日志目标检查。输入为根CMake/生产QML清单、`cmake/Deployment.cmake`、qrc资产、GraphicsBackend/BackendCapabilities、LoggingSession、平台启动/部署工具与匿名库。按test-gates.md的源码审查边界展开语言/Qt/QML实践及适用平台分支；已确定的跨平台算法与性能结论共用，不新建逐组件审计工程。
- 每个目标Kit准备一次所需Release产物及去重检查；生产Review OFF的加载闭包与开发Review工具分开。构建/模块、目标编译器、资源打包和脚本成功路径必须在目标执行；qsb跨目标翻译不能代替目标运行。Windows PowerShell执行`pwsh -NoProfile -File tools/format/FormatTools.ps1`及同入口`-Check`，目标启动/部署工具核对当前Process.ExitCode、参数、含空格路径与子进程环境；无PowerShell/PSScriptAnalyzer时保留此格式/执行缺口，不记通过。GUI缺报告/错误必须失败。
- Windows资产入口共用`tools/build/asset-tool.ps1`。输入为`tools/assets/{prepare-assets,export-branding}.ps1`与匿名源码副本；依赖原生MSVC、最新完整Kit和PowerShell/PSScriptAnalyzer，入口分别为`pwsh -NoProfile -File tools/assets/prepare-assets.ps1`及`export-branding.ps1`。核对含空格路径、所选preset/target、同Kit环境、balsam版本和冻结hash；configure/build/tool任一步失败须停止并非零退出，目录栈在成功/异常后恢复。两项格式入口沿上条执行；无该环境时保持待验，不以POSIX入口代替。

| 目标/归属 | 准确入口与独立退出事实 |
| --- | --- |
| Windows · R30-P/BP-P | 操作按[windows.md](docs/platforms/windows.md)；`pwsh -NoProfile -File tools/deploy/windows.ps1`及包内`verify-windows.ps1`。真实MSVC/最新Kit/C++23及native-bootstrap.md规定的Release优化、PE/图标、同Kit windeployqt/官方VC运行库、插件/媒体/字体/shader/许可与相对路径完整；包复制到无开发Qt的干净系统及中文/空格路径，从包外加载，不借开发PATH。 |
| macOS · R31-P | 输入为`out/macos/`完整生产包，操作按[macos.md](docs/platforms/macos.md)。整包在另一台兼容目标设备的中文/空格路径从包外运行`python3 verify-macos.py --output 绝对新目录`，核对实际映像/架构/相对资源/签名；实机核对Finder品牌、物理AppKit菜单/Command、系统目录选择与输入法。 |
| Linux · R32-P/BP-P | 操作按[linux.md](docs/platforms/linux.md)；`sh tools/build/linux.sh`、`sh tools/deploy/linux.sh`、`python3 tests/tools/UnixDeploymentTest.py`、`python3 tools/deploy/verify-linux.py --output 绝对新目录`。原生C++23/最新Kit及native-bootstrap.md规定的Release优化、平台文档规定的ELF依赖/strip/相对加载、xcb/Wayland/媒体/CanvasPainter及许可/权限完整；整包在无开发Qt的兼容CPU/系统ABI及中文/空格路径从包外加载。 |
| 后端与日志 | 使用Release `graphics-backend`、`logging`、`logging-terminal`及普通部署包。Windows显式D3D11/12、Windows/Linux的OpenGL与已启用Vulkan、macOS Vulkan核对真实API；Qt特性或设备/格式不支持时明确报错并非零退出。Linux默认Vulkan不可用时警告并回退OpenGL，显式请求不回退。目标成功路径需真实输出，不能只看初始化。日志目录/上界/轮转按logging.md；Windows真实控制台VT颜色/UTF-8及重定向正常。 |

- SINGLE-INSTANCE-P：Windows/Linux尚缺本机QLockFile与QLocalServer（named pipe/Unix socket）的真实进程仲裁及窗口激活事实。源码为SingleInstance、main、PreferencesStore/SettingsController和SettingsPanel（契约settings.md）；匿名输入由SingleInstanceTest/SettingsTest生成。依赖目标Release包及正常桌面会话，入口为`ctest --preset qt-release -R '^(single-instance|settings)$'`，并在普通产品设置中切换模式、重启、再次启动同一目录及独立临时目录。退出要求为开启时仅原实例处理激活、关闭时可多开、目录隔离、退出/崩溃锁恢复、错误不降级启动；真实前台/最小化恢复需遵守当轮窗口授权，macOS物理激活同归下方产品/输入任务。目标构建仍按本组原门槛，不以主机测试代替。

部署验证失败保留旧包；加载探针仅证明实际报告中的加载事实。产品功能、设备与窗口检查直接共用下方正常产品会话，不在部署后再跑整套组件/性能矩阵。macOS当前Kit未启用Vulkan，缺对应原生Kit/SDK/MoltenVK；未具能力时保留该具体缺口。

### 联网资料目标运行 · ONLINE-METADATA-P

- 缺口：Windows/Linux原生Qt Network/TLS、系统代理发现、直连与手动HTTP代理的实际运行；macOS其他系统代理/PAC配置仍需对应系统设置环境。共用匹配、取消、缓存、持久化与代理选择算法由`online-metadata`验证，不能外推目标系统的连接事实。
- 源码与输入：`OnlineMetadataProvider`、`metadata/MetadataHttp`、`LibraryStore`、SettingsPanel/MetadataSection及`tools/migration/MetadataNetworkProbe.cpp`；仅用公开专辑名与公开MusicBrainz发行ID/曲目数组，不读用户库。依赖最新目标Kit的Network模块和随包TLS后端、对应系统代理环境。
- 入口与门槛：按library-data.md运行`online-metadata`及`metadata-network-probe`的介绍/制作/流派三类公开查询；设置页核对library-data.md定义的代理配置路由。查询结果按实际公开资料判定，并保留内容、身份核对、来源/许可和查询状态；至少一个有完整介绍和制作资料的公开fixture须证明成功路径。单张指定发行的上游资料缺失或变化不能使保守返回Uncertain/NotFound的正确实现失败；系统或网络不可达保留原因，不能写成NotFound或成功。部署后复用目标包验证Qt Network/TLS闭包；GUI/默认隐藏策略按test-window-policy.md，相关设置检查按test-gates.md选择。

### 文件系统、媒体与数据

- 归属：R21-P、R22-P、R24-P、R25-P。依赖上述目标构建；Windows/Linux尚缺原生文件系统/媒体插件实际执行，三平台尚缺明确列出的物理设备与听感事实。输入仅用DirectoryScannerTest/MediaMetadataTest/PlaybackTest/AudioAmbienceTest生成的匿名目录、标签/图像、WAV/损坏输入及既有BGM；不写用户歌曲。

| 对象与权威契约 | 最小入口及退出条件 |
| --- | --- |
| DirectoryScanner/LibraryStore · R21-P · library-data.md | `directory-scanner`/`library-data`/`library-qml`中尚缺的目标分支；指纹语义需要核对时用`tools/migration/verify-scan-fingerprint.mjs`。核对原生UTF-8路径/ID、去重/重扫/取消/原子缓存；Windows实际UNC/junction/ACL，Linux大小写/权限/只读缓存及循环链接。无能力项保留具体缺口。 |
| MediaMetadataProbe/ArtworkStore/LegacyImporter · R22-P · library-data.md；偏好导入见settings.md | `media-metadata`及受影响缓存/导入测试。真实无输出/无play探测的取消/晚到/超时/退出，封面优先级及解码保护、内容寻址/符号链接拒写、显式只读旧JSON与未知字段/坏文件/原子保存错误符合契约；Loaded不代表可播放。 |
| PlaybackController及三路声音 · R24-P/R25-P · playback.md | `playback`/`audio-ambience`核对共用可控状态，`ctest --preset qt-release-media -R '^(playback-device|audio-ambience-device)$'`验证目标真实输出；GUI共用一次`playback-qml`或`ui-tests --playback 新目录`。普通Qt公开API、队列/自然结束/暂停/seek/错误与导航隔离正常，歌曲/BGM/音效独立；静音事件不补合成音源。 |

物理声音检查在每个系统的一次正常产品会话中合并：真实支持样本的播放/暂停/seek/换曲/自然结束及损坏/不支持原因；默认改选、显式设备保持、消失回退/无输出/重连及加载中换曲换设备，保持暂停/停止意图与有效位置/音量；BGM/音效走各自规定路由，最小化继续、关闭停止，听感无意外重叠/突变。既有有效状态测试复用，不能以无声加载、程序进度或模拟设备替代尚缺的实物事实。

### 产品功能、文字与物理输入/显示

- 目标边界：Windows/Linux保留对应后端实际输出及产品接线；macOS仅保留MAC-VISIBLE-3D的默认置底无激活显示；三平台保留下方物理输入/显示，Windows/Linux另保留原生窗口生命周期。
- 归属：UI-CONTROLS-P、R16B2W/R16B3/R16M3-S、R17-P/R18-P/R19-P/R19E-P/R20-P/R23-P/R26-P/R27-P/R29-P、MAC-VISIBLE-3D。依赖目标构建和现有生产接线；输入为各组件既有匿名fixture、冻结数学/CSS结构与qrc封面。生产文字glyph+彩色Quick Text、滑尺C++ CanvasPainter、序章沿当前路径验收，不安排默认候选路径比较或成本测试。
- 先将已有产品/组件报告按实际断言对照，只补缺口。源码数学、信号/状态可复用同平台非图形结果；布局/主题/DPR按test-gates.md选独立边界。目标真实输出与实际输入仍须对应目标证据，不能以源码审计假称画质通过。

| 共同检查及归属 | 修复对象、输入与入口 | 独立退出条件 |
| --- | --- | --- |
| 面板/控件 · UI-CONTROLS-P、R23-P/R26-P/R27-P | Library/Search/Settings/ExperimentPanels、TrackFocusController、PreferencesStore/OnlineMetadataProvider；LibraryChecks/Settings/Experiment匿名输入。选择`controls-qml`、`library-panels-qml`、`settings-qml`、`experiments-qml`中实际缺项，或对应`ui-tests --library/--settings/--experiments`入口。 | 模态/滚动/焦点、搜索取消与最终定位、资料来源/离线状态、设置保存/未知字段与当前媒体连续、四实验模式/改速/临时对照恢复符合controls/library-data/settings/lighting/motion契约；相同控件断言在各宿主复用，宿主特有路由独立保留。 |
| 文字/页眉/滑尺 · R16B2W/R16B3/R16M3-S/R19E-P | TitleReel/TransportTitle/Header、Ruler/Ordinal/InputRouter；`tests/fixtures/text/titles.json`及144组滑尺源轨迹。`export-title.mjs`→`verify-title.mjs --backend 目标 --dpr 实际值 --source 源目录 --output 新目录`、`title-reels`、`source-ui-probe --ruler 输出目录 源report.json`及export-ruler；静态CSS锚点复用source-fidelity-review.md的R19E，目标布局与宿主联动按下方产品入口补验。 | 字体身份、连字/组合字/emoji、字形/断行/省略/裁切、页眉结构与播放隔离、自然宽度通知/隐藏完成与恢复取消、刻线/clip/淡隐/背压符合title-reels/navigation/browse-ui；所有权/缓存/隐藏工作量先静态确定，未决运行行为才最小观察。 |
| 详情/总览 · R17-P/R18-P/R19-P | Presentation/DetailMotion/ReturningCasePool/PointerDragController、Overview与SceneItem；presentation/detail-golden/overview/source-golden匿名输入。`ui-tests --presentation`、`verify-detail.py --backend 目标 --runs 1 --output 新目录`、`verify-overview.py --visual --backend 目标 --runs 1 --output 新目录`按缺项选择及对应结构导出/比较入口。 | ready/revision/四种archiveRoute、连选最后意图/反向/拖动取消、sticky tabs/行高/委托虚拟化、总览两步点击/跨列收拢与反转/碰撞准入（标签240ms退场与420ms入口收拢的衔接、计数block宽度）、焦点恢复/定位不改播放，遵守presentation/detail-motion/overview；产品回放与组件报告共用。 |
| 序章与产品联动 · R20-P/R29-P | OpeningController/OpeningInk、MusicStage/SceneItem；opening/source-golden.json与qrc封面/匿名库。`opening`、`ui-tests --opening`及export-opening-golden；仅组件证据未覆盖的跨组件联动才选`verify-gui.py --backend 目标 --output 新目录`。 | 输入锁/跳过/重播/减少动态/一次交接、遮挡零三维submit和恢复正确，产品浏览→详情→总览→搜索/片头的真实接线、透明合成与资源容量符合opening/rhi-integration；不为汇总已有组件结果另跑完整GUI矩阵。 |
| macOS默认置底无激活显示 · MAC-VISIBLE-3D | VerificationWindowMac::NativeWindowGuard/prepareNativeApplication、App.qml；`python3 tests/runners/verify-detail.py --backend opengl --runs 1 --output 新目录`的匿名库。已知失败输入为`verification/r35-detail-native/run-1/report.json`（原生应用hidden、窗口不在屏幕且无曝光/帧）与`verification/r35-detail-accessory/run-1/report.json`（进程激活）。依赖可靠观察自身窗口且不干扰工作区的macOS会话，修复对象仅为默认验证启动/显示策略。 | 默认策略下实际OpenGL窗口show/提交、置底、拒key/鼠标、前台PID与完整激活/焦点历史同时合规，恢复后仍遵守该策略。保持普通产品行为；无可靠条件或发生激活时不得宣告通过、提高层级或放宽门槛。 |

物理检查与部署、媒体共用同一目标产品会话：键鼠/触控板pixelDelta/快捷键、系统中英文与emoji组合输入、剪贴板/撤销、目录选择、辅助技术角色/名称、焦点/模态隔离；Windows/Linux另核对首次show、同对象hide/minimize/restore、实际surface变化与退出；真实跨屏DPR后的SceneItem/Canvas/文字尺寸/清晰度和缓存恢复。不同系统/会话的事实分别保留，隐藏target DPR及注入Qt事件不能冒充物理设备。

退出：按组件权威契约核对相应结构、数学和时序；实际输出质量执行test-gates.md的观察与失败判定，QML/shader无错误。源码可确定的复杂度/资源上界/取消与释放直接以静态结论收口；只有具体未决运行行为才补必要转换，不附带默认十次SG/root重建、三十次换片或成本采样。

### 图形算法与生产接线

- 目标边界：下表图形运行只保留Windows/Linux及具备原生Vulkan能力的macOS目标差异；物理屏幕事实由产品/显示任务维护，不重复GPU算法检查。
- 归属：R19A/B/C/D各`-P`。依赖目标构建/真实GPU；输入为冻结生产源码、`resources/manifests/`、各组件现有源数学fixture及匿名RhiProductionFixture，不默认生成完整大封面库。审计与适配判定索引为[source-fidelity-review.md](docs/contracts/source-fidelity-review.md)，算法/格式/容差仅由下表权威契约维护。
- 先审生产入口到实际上传/着色器/附件/合成/释放，复用已有效的源公式与资源结论；只为目标格式、驱动、实际光学/合成等尚缺事实选对应入口。以下为定位表，不是每目标逐项必跑清单；工具的固定状态数、1200帧序列和十次重建均非默认任务数。共有生产状态可同时证明各pass接线，无需另跑同一连续序列。

| 原ID | 源码/输入与权威契约 | 定向入口及尚需目标事实 |
| --- | --- | --- |
| R19A-P | PrepareAssets、字体物理表/QImageReader、封面租约；assets/branding/covers | `prepare-assets --verify-inputs`，assets/branding/cover-cache及资产来源核对；真实平台字体/ICC/缺图/alpha与插件。源保护负向检查只在隔离副本进行。 |
| R19B-P | FrameClock/SceneAdapter、ArchivePool/RealColumns、motion/scene/archive-pool/real-columns fixture与契约 | 同名数学测试/导出器、`archive-pool-gpu`/`real-columns-gpu`中尚缺的目标分支；真实投影、稳定身份/两阵列/记忆/库替换、封面区域与容量。 |
| R19C1-P/R19C2A-P | ShellMaterial/SelectionLighting/RowLightController及MaterialReview；materials/parameters、lighting/golden、single-case/visual输入；materials/lighting | `rhi-material-parameters`/`lighting`及必要的runtime-materials/runtime-lighting；真实参数上传与材质/纸面/光场。Quick3D仅Review，不成为生产RHI的重复验收。 |
| R19C2B1-P/R19C2B2-P | RhiItem/OpticsProbe、原GLB/Room/DFG/LTC、rhi-shell/visual；rhi-opaque/rhi-shell | `rhi-opaque-gpu`/`rhi-shell-gpu`；实际深度/法线/投影、排序/捕获隔离、MSAA部分覆盖/透明洞/alpha黑边与光学解析。 |
| R19D1-P/R19D2-P | SSAO/Fog生产helper与uniform、occlusion/visual、kernel/noise资源和rhi-shell/visual；occlusion/postprocess | `rhi-ao-gpu`/`rhi-fog-gpu`；R16F及奇数宽stride/GL pack、标量/颜色读回、真实camera/detail/线性fog上传与捕获→最终输出次序。 |
| R19D3A-P/R19D3B-P | Shadow及productFloor、源shadow/Standard/GLB/地面主题参数；rhi-opaque | 共用`rhi-shadow-gpu`及`rhi-shadow-test 新目录 --attachments-only/--invalidation-only`；生产D32F-only、显式采集R32F生命周期/预算及同尺寸深度身份保留、normalBias/PCF、地面实际occluded>0、PBR/fog/完整AO与DOF depth。地面不另跑整套阴影。 |
| R19D4A-P/R19D4B1-P | Postprocess/Bokeh/SMAA、postprocess/bokeh-kernel与锁定lookup；postprocess | export-bokeh-kernel --check及真正离屏`rhi-bokeh-gpu`/`rhi-smaa-gpu`；真实生产pass解析颜色、HDR/alpha/过滤/边界。 |
| R19D4B2-P/R19D4C-P | Postprocess/AtlasUploadQueue、SceneItem有效DPR→RenderQuality、render-backend/visual与RhiScaleChecks九尺寸；rhi-postprocess/rhi-integration | `rhi-postprocess-gpu`/`rhi-scale-gpu`；实际MSAA/resolve/无反馈资源图、附件尺度/保护/CoC/租约与tile上传。Windows原生UpdateSubresource boxes/bytes仅该目标可证明；其他后端不伪造。 |
| R19C2C-P及上述pass生产接线 | SceneItem→RhiItem实际生产阵列/几何与光学；rhi-shell/rhi-integration | `rhi-flicker-gpu`所选状态，或已有产品输出/上传报告；共同证明跨批量边界、运动/进退中的实际uniform与连续透明覆盖，使用rhi-shell.md规定的有限值/覆盖解析断言，保持源实体数量/效果，不默认完整1200帧。 |

退出：相关实际输出满足所属契约的原公式、格式、采样、颜色/几何容差及容量/上传边界；静态结论与GPU实际事实分开。无未决项不补性能/缓存成本/重复释放压力；已有实际失败仍须修复并最小复验，未执行目标不记通过。

### HDR输出与系统事件 · HDR-OUTPUT-P

- 依赖真实目标系统、显示设备及输出能力；输入为DisplayOutputState/Monitor、WindowsDisplayQuery/LinuxColorManagement/MacDisplayQuery和DisplayOutputTest的5×1颜色。契约为[display-output.md](docs/contracts/display-output.md)。主机静态/qsb不能证明其他系统编译或显示事实。
- Release的DisplayOutputTest、`display-transfer-gpu`及需要当轮可见授权的`display-output-visible`只补缺项。Windows核对HDR开关/跨HDR-SDR屏/SDR brightness slider和nits/80，WCG SDR不误判；Linux核对真实Wayland颜色协议/PQ或linear swapchain及reference white/target luminance通知、无协议/X11的SDR和Vulkan失败回退；macOS核对亮度/电源引起的current EDR及跨屏恢复。
- 退出：查询失败清旧状态、参数变化不重建/模式切换保留QML与音频、epoch/释放正确，真实通知与输出符合契约。R36按上述剩余目标执行；OpenGL保持SDR，Windows/Linux需真实输出事实，macOS仅在共享变更影响时回归。不附带大型阵列、性能A/B或整场矩阵。
