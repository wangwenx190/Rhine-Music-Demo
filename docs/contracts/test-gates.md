# 测试分组与执行范围

本文维护验收证据、CTest标签/preset、选择与复用规则；窗口行为归[test-window-policy.md](test-window-policy.md)，格式命令归[code-style.md](code-style.md)，性能的静态优先策略及最小实测门槛唯一归[performance.md](performance.md)，组件断言在各组件契约。

## 验收对象与最低充分证据

验收服务于桌面本地播放器及其源视觉设计；每个门槛须对应生产功能、正确性、质量、资源边界或明确支持的目标能力。相同输入与状态的固定重复次数本身不形成独立门禁；资源恢复须核对实际释放和恢复后的消费者，不能仅检查对象仍存活。组件文档中的完整输入集、状态数、1200帧序列及重建次数均描述工具覆盖能力；实际执行范围由改动影响和独立缺口决定，不自动成为每次任务数。

| 改动或待确认事实 | 必要证据 |
| --- | --- |
| 文档/注释、来源映射 | 当前符号、路径/链接、命令及职责一致；不启动播放器。资产正文进入qrc/manifest时另验证字节/hash与加载闭包 |
| C++或工具逻辑 | 受影响Release目标编译及相关行为回归；工具覆盖有效参数、错误传播和输出保护 |
| QML或实际类型/导入 | 对应官方qmllint；生产根/导入/合成宿主或初始化依赖变化时另执行production-app-root；受影响交互/布局由组件证明 |
| 生产shader/渲染资源 | 同Kit适用qsb变体及相关解析/附件/真实输出检查；算法helper不能替代真实上传/消费者 |
| 平台分支/打包 | 对应目标构建/依赖/包外加载；主机源码审查与跨目标qsb只形成静态结论 |
| 输出设备、跨屏、输入法/辅助技术、实际显示 | 对应真实系统/设备事实；注入事件与target DPR只覆盖受控状态，缺设备保留任务 |
| 性能/资源寿命 | [performance.md](performance.md)的适用静态审查；仅具体未决问题或用户明确指定时最小实测 |

缺陷修复必须有能触发原缺陷并核对预期的单元回归，优先扩展现有测试；网络单元使用回环服务或可控请求/响应替身。只有明确技术上无法自动化并附可核查限制/权威依据及替代方案时允许例外；环境暂缺只能记待验。文档事实纠正用交叉核对，不为每句文字建立源码字符串测试。

## 结构、画质与设备判定

- 受控布局输入由`tests/fixtures/web-baseline/anonymous.json`维护；组件若需专用小尺寸/奇数宽/解析纹理，以其fixture与契约为准。源轨迹按独立输入核对，不能用生产实现生成自身期望。全局不强加GPU float与double相同误差。
- 通常结构锚点≤1逻辑px、三维投影≤2逻辑px、double轨迹≤1e-6；这是同输入的结构/算法比较，组件的格式量化/相对误差另按所属契约。状态机完成依赖真实就绪/终态；Qt调度或原生动画不能一律要求在固定墙钟帧结束。时序公式的受控采样容差才使用一个60Hz帧。
- 实际画质须查看自身正常输出，记录视口/DPR/主题/后端与受影响区域。漏画、意外错位/裁切、闪烁、错误颜色/合成、可见异常锯齿或额外模糊属于失败；源设计的景深/磨砂/背景模糊保留。抗锯齿在有限像素上不是“任何放大倍数均无台阶”的承诺，不能以任意放大、图像评分或不相关历史截图制造门槛。源码能确定的缺陷仍须修复。
- 物理显示按支持目标实际可用的DPR/刷新率/系统缩放记录；不要求每个平台找到精确DPR1/1.5/2三套屏幕。受控target覆盖这些缩放边界，实物补实际跨屏通知、逻辑尺寸/清晰度/恢复，二者分别记账。HDR/设备格式只在声明支持且具备相应能力时验收，其他环境核对明确回退/失败。
- 公开联网探针证明目标TLS/代理与真实响应处理，不把某张专辑必须返回固定资料状态当作永久门禁。成功、缺资料、歧义和不可达必须忠实区分；保守匹配与失败路径由稳定的可控单元输入覆盖，成功路径由有资料的公开fixture补证。

## 测试与审计边界

- `tests/unit`、`tests/integration`、`tests/render`和`tests/tools`维护长期回归：直接执行生产算法、状态、Qt交互、真实渲染或工具错误路径。`tests/application`维护产品命令验证宿主和隔离Quick3D Review，`tests/support`维护回归共用辅助；生产、审计、测试与工具的物理目录边界唯一见[product.md](product.md#顶层组合与目录)。`tests/runners`只运行已构建的Release产物并校验当前结果，不隐式构建、格式化或扩展矩阵。原生回归宿主输出到`build/<配置>/tests/`，按功能命名。
- `audit/source`维护源码策略与编译器安全分析，`audit/assets`及`audit/rhi`分别维护几何/原生附件与上传审计；`tools/diagnostics`维护显式公开资料联网探针。独立源码审计不注册为CTest，也不成为日常测试的运行依赖；原生审计观察器由Review开关控制，行为回归可直接调用几何检查接口；长期数学测试消费已提交fixture，不读取Git历史、浏览器或本地审计报告。
- 网络诊断的原生工具由`-DYALAHOO_DIAGNOSTICS=ON`单独启用，目标为`metadata-network-probe`，输出到`build/<配置>/tools/`；普通配置默认OFF。窗口仍按test-window-policy.md，实际任务与未验门槛只归迁移计划R36。
- 保留依据是可重复的行为/数值断言或当前审计需求。源码改名、格式或无关实现调整不应使行为测试失败；语义/复杂度/所有权由人工沿实际调用链审查。资产来源和许可仍由对应manifest与专项契约维护。

## 保留设施与回归职责

保留理由必须指向下列独立失败边界或迁移计划R36的具体目标事实。纯粹重算fixture、只保存清单、重复演示宿主及无消费者输出不构成门禁；慢或缺目标环境本身也不代表断言无价值。生产行为使用可控输入与有界完成条件；实际失败必须修复，缺环境保持待验。

| 设施 | 独立保留理由与权威职责 |
| --- | --- |
| `application-options`、`application-startup`、`identity`、`module-link-and-plugins`、`kit-selection`、`shader-tools`、`production-app-root` | 身份/数据根、真实模块与媒体插件、Kit选择、生产qsb变体、真实App创建；native-bootstrap.md及R36目标构建闭包 |
| `source-policy`构建target、`source-format`、官方qmllint、`localization` | CPP-30/QT-19–22源码与翻译要求、格式、QML类型/导入；各最佳实践及code-style.md |
| `motion`、`scene`、`navigation`、`lighting`、`archive-pool`、`real-columns`、`presentation`、`detail-motion`、`overview`、`opening`、`track-focus`、`startup` | 独立冻结公式、边界/中断/身份/输入归属/通知及唯一帧源；对应组件契约，覆盖R36共用算法 |
| `directory-scanner`、`library-data`、`library-qml`、`online-metadata`、`media-metadata`、`json-file` | 文件系统/事务保存、模型协议、来源与取消/晚到、解码/导入失败；library-data.md及library-ui.md。网络单元测试仅用可控响应 |
| `playback`/`playback-device`、`audio-ambience`/`audio-ambience-device`、`single-instance`、`settings`、`experiments`、`logging`、`logging-terminal`、`graphics-backend` | 可控媒体状态与独立真实设备三路验证、跨进程仲裁、持久化/模式/错误、真实终端输出及后端选择；playback/settings/logging/render-backend及相关组件契约 |
| `assets`、`branding`、`title-reels`、`cover-cache`、`rhi-atlas-queue`、`readback-queue`、`rhi-material-parameters`、`render-quality`、`render-support`、`verification-input`、`display-output-probe` | 许可/字节/几何/字体、独立品牌几何、字形与缓存、上传准入、实际材质参数、尺寸/回调寿命、探针输入结构/数值、资产批次回滚与异步显示探测完成状态；assets/branding/covers/title-reels及RHI契约 |
| `verification-window-policy`、显式可见窗口/输出检查、`macos-application-menu` | 不干扰窗口约束、实际native呈现与HDR/系统菜单；test-window-policy/display-output及R36实物事实，隐藏与可见事实分别保留 |
| RHI与封面/阵列GPU组件、`display-transfer-gpu`、`rhi-required-output` | 执行真实生产shader/pass，核对独立解析结果、附件/上传/提交/释放；对应RHI/display-output契约及R36目标后端差异 |
| QML组件回放、生产详情/总览/导航、GUI与同窗口生命周期 | 真实控件/面板/音乐舞台的交互、布局、模态、播放隔离与恢复；组件契约及R36产品接线。诊断采样入口仅按performance.md选用 |
| `runtime-materials`、`runtime-lighting` | 隔离Quick3D三表面/纸面与光场适配，R36明确保留的源参数/投影及有效输出边界；materials/lighting.md |
| `dry-tools`（含详情/总览报告的严格字段、状态集合及输出存在性）、`material-review-input`（Review ON）、`startup-verification`、`test-policy`、`native-tool` | 包装器真实错误传播/失败清单、有限测量/数值错误、独立采集文件、三平台CTest选集/环境隔离及工具独立编译；本契约及verification-artifacts.md |
| `launchers`、`unix-deployment`、`linux-package-tools`、`windows-deployment-report` | 参数/子进程状态、包完整性/权限/依赖/符号链接、原生strip/相对加载及部署报告的严格成功标记；平台部署契约。目标对象格式分支只在对应系统执行 |
| clang-tidy安全审计、资产来源核对与生成器 | 编译器定向检查所有权/未定义行为及超时/诊断错误传播、冻结hash/许可及可复现生产输入；cpp23-best-practices/assets/source-provenance。原生模型重导入的隔离回归为`python3 tests/tools/AssetExportTest.py`，覆盖实际生成、坏输入/发布错误及维护资产不被覆盖，不加入普通CTest |

## 选择与复用

| 入口 | 范围与使用时机 |
| --- | --- |
| `ctest --preset qt-release -R 对应测试` | fast：只选择本次逻辑改动影响的源码/算法/状态/资产/模块/格式检查 |
| `ctest --preset qt-release-render -R 组件` | 按需选择真正隐藏渲染项 |
| `ctest --preset qt-release-media -R 对应测试` | 显式真实音频设备与媒体集成；纯状态/持久化仍在fast，GUI同时受窗口标签限制 |
| `ctest --preset qt-release-full` | fast+不依赖音频设备的隐藏功能集成，不含独立压力；用户明确要求完整测试/全量回归，或影响分析确认确需此完整范围时执行；该请求不包含性能采样、研究选型或独立压力 |
| `ctest --preset qt-release-visible -R 具体测试` | 显式窗口/输入/焦点/呈现/相关生命周期，不默认全选 |

- 标签与环境来自`tests/cmake/TestPolicy.cmake`及`CMakePresets.json`，GPU/窗口项串行。fast/full/render/media清除可见、前台和生命周期开关；fast/full/render排除native-visible及native-media，fast另排除render-integration；media只选native-media并排除native-visible。macOS可见项默认禁用，禁用/跳过只表示待验。
- `-R`使用准确测试名或最小名称并集，运行前可列举确认展开范围。裸preset会选择一批测试；full/render/visible不因名称自动执行。TIMEOUT为失败上界，不是建议观察时长；不延长超时掩盖挂起或降低容差。
- 按被测行为、输入、相关源码/依赖与环境判断已有结果是否有效。无关Git版本号、二进制hash变化不自动使结果失效；文档/注释不配置、编译或启动应用。变更只重做失效的构建/格式/qmllint及行为检查，共享代码沿实际调用链定界，不自动扩大到fast或全量集。
- `playback`验证队列/快照、可控晚到通知和不播放的无输出媒体加载；`audio-ambience`验证静音事件/参数/持久化。真实输出分别由`playback-device`、`audio-ambience-device`证明，设备缺失明确失败；startup-qml/settings-qml/playback-qml同时标native-media。物理拔插、听感和真实窗口事实仍须对应目标证据。
- `qml-behavior`隐藏创建生产QML组件，核对曲目双身份复用/无障碍/离线禁用、流派焦点、零尺寸模糊与堆叠滑杆。`production-app-root`为不依赖Review的fast项，通过生产`--verify-app`创建真实App、控制器及DisplaySurface/Overlay，核对尺寸、无QML警告、无播放和窗口策略。App根、生产导入/注册、合成宿主或初始化依赖变化时，必须执行该项及受影响模块的官方qmllint；构建成功或专用探针不能替代。

## 目标欠验的合并与矩阵选择

1. 先列已有证据、实际缺口、最小入口、所选状态数与退出条件。只有具体未决问题需要计时/压力时才列时长/次数，条件唯一见[performance.md](performance.md)。同目标/后端/输入/相关源码的一次结果可满足多个ID，按真实断言对应；不同系统、后端、物理输入及窗口事实不能互代。
2. 包装器只消费已准备的Release产物，不隐式格式化、切Review或扩展前置矩阵。只构建必要目标/依赖；Review与网络诊断分别按其开关启用。各目标共用已提交的独立数学输入，字体/平台布局仍按实际环境核对。缺历史报告本身不触发重跑。
3. 布局先在DPR1选择受影响断点两侧与相关极端尺寸；双主题在代表桌面/窄屏核对颜色、合成和主题状态，主题改变几何才扩展尺寸。DPR1.5/2补代表桌面及受DPR影响的窄屏/文字/附件边界。格式、奇数尺寸、MSAA、颜色耦合等真实组合依赖保留，不能为减少数量删便宜的解析/状态边界。
4. 长交互只覆盖需要的代表布局；其他布局做进入、关键几何/合成和返回，有独立输入分支才补交互。生产文字/滑尺路径保留；候选API横评、备用绘制对比、最优参数搜索和新验证框架仅在具体缺陷、瓶颈或明确选型任务中采用。
5. 同窗口hide/minimize/restore、跨屏、完整root重建与独立pass释放按资源所有者分别证明，不互相冒充。源码已确定的释放/取消/容量不追加重复压力；具体运行行为仍有疑问才观察相关所有者和必要转换。生产连续序列只补缺失状态/过渡，已有上传、覆盖和资源报告共用，不按历史ID逐pass重跑。
6. 同一Linux系统/Kit/GPU/驱动/后端的X11与Wayland共享构建、数学及无会话依赖的pass结果；各会话独立证明上下文/swapchain、格式/MSAA/resolve、窗口/输入/焦点/DPR与呈现，提交分支相关差异另验。
7. 必要检查被无关回放/采样/压力阻塞时，优先已有更小入口；确无入口才在当前任务内解开最小分支，记录实际覆盖。不先跑整套再称局部，不为小型解析矩阵建设通用选择器/报告框架。

首次目标验收同样只补尚缺的独立事实，代表组合不能冒称全矩阵。报告区分本次实际、复用、未选、暂缓及原因；可靠失败保留并修复，缺环境由迁移计划R36按原门槛维护，不写成通过。

## 源码审查边界

沿实际入口核对构建源清单→链接/moc/qrc/qsb→QML/插件→部署加载闭包；检查C++/Qt/QML实践、路径/编码/权限、公开媒体状态、错误/取消/销毁及平台条件分支。源码符号、适用输入、判定与限制应可追溯；不每轮新建固定COMMON/WIN/MAC/LNX/hash矩阵。复杂度与所有权审查方法归performance.md。

## 门槛解释与停止条件

每项检查须回答具体正确性、质量、资源或目标能力问题。算法/结构/颜色/状态容差按所属契约；数值失败先核对预期、输入及生产语义，真实缺陷修复，采集无效保留限制，不改阈值追分。当前缺口有可靠答案、修复后受影响断言通过即停止；无新增风险不自动复核三次、补其他后端/尺寸、追加长回归或因遥测字段不可测另建研究任务。用户明确指定的范围优先。

## 当前Review入口分类

以下按实际CTest入口分类，适用于现有配置；不以文件名或“GPU”字样推断隐藏能力。

| 实际入口 | 分类 |
| --- | --- |
| rhi-bokeh-gpu、rhi-smaa-gpu、display-transfer-gpu | 直接QRhi offscreen，各平台入口均无可见窗口；实际后端能力另验 |
| startup-qml、controls-qml、experiments-qml、settings-qml、playback-qml、library-panels-qml、opening-qml、presentation-qml、overview-qml、archive-pool-gpu、real-columns-gpu、runtime-materials、runtime-lighting、rhi-opaque/shell/flicker/ao/fog/shadow/shadow-attachments/postprocess/transient-attachments/scale-gpu、rhi-required-output | macOS当前RenderControl真正隐藏；其他系统当前需native窗口，按能力变化重评 |
| cover-gpu、runtime-detail、runtime-overview、display-output-visible | 当前按native-visible注册；macOS可见测试OFF时禁用。cover-gpu不属于macOS RenderControl隐藏组 |
| verification-window-visible | native-visible；macOS仅在可见测试ON时注册，其他系统始终注册 |

整场GUI显式入口为`python3 tests/runners/verify-gui.py`，范围与采集语义见[rhi-integration.md](rhi-integration.md)；它不隐式加入fast/full集合。

`YALAHOO_REVIEW=OFF`不生成/注册独立RHI GPU宿主、runtime-materials/runtime-lighting、rhi-required-output及material-review-input；`ui-tests`、QML组件回放、runtime-detail/runtime-overview和production-app-root仍生成/注册。具体target与注册条件分别见`tests/render`、`tests/unit`及`tests/integration`的CMakeLists.txt。archive/real-columns回归直接消费已提交的最小数学fixture，不读取Git历史或外部Web工程。组件具体状态数/门槛只在组件契约维护。
