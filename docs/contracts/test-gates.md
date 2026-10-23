# 测试分组与执行范围

本文只维护CTest标签/preset、选择与复用规则；窗口行为归[test-window-policy.md](test-window-policy.md)，格式命令归[code-style.md](code-style.md)，性能的静态优先策略及最小实测门槛唯一归[performance.md](performance.md)，组件断言在各组件契约。

## 验收对象与最低充分证据

验收服务于桌面本地播放器及其源视觉设计；每个门槛须对应生产功能、正确性、质量、资源边界或明确支持的目标能力。相同输入与状态的固定重复次数本身不形成独立门禁；资源恢复须核对实际释放和恢复后的消费者，不能仅检查对象仍存活。测试完整输入集是工具能力，实际执行范围由改动影响和独立缺口决定。

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

- `tests/unit`、`tests/integration`、`tests/render`和`tests/tools`维护长期回归：直接执行生产算法、状态、Qt交互、真实渲染或工具错误路径。`tests/runners`只运行已构建的Release产物并校验当前结果，不隐式构建、格式化或扩展矩阵。原生回归宿主输出到`build/<配置>/tests/`，按功能命名。
- `tools/audit`维护源码策略与编译器安全分析；`tools/migration`维护冻结来源导出、独立结构/字体核对和尚缺的目标环境探针。审计不注册为CTest，也不成为日常测试的运行依赖；长期数学测试消费已提交fixture，不读取Git历史、浏览器或本地审计报告。
- 来源/网络审计的原生工具由`-DYALAHOO_MIGRATION_AUDIT=ON`单独启用，目标为`source-ui-probe`和`metadata-network-probe`，输出到`build/<配置>/tools/`；普通配置默认OFF。窗口仍按test-window-policy.md，实际任务与未验门槛只归迁移计划R36。
- 保留依据是可重复的行为/数值断言或当前审计需求。源码改名、格式或无关实现调整不应使行为测试失败；语义/复杂度/所有权由人工沿实际调用链审查。资产来源和许可仍由对应manifest与专项契约维护。

## 保留设施与回归职责

保留理由必须指向下列独立失败边界或迁移计划R36的具体目标事实。纯粹重算fixture、只保存清单、重复演示宿主及无消费者输出不构成门禁；慢或缺目标环境本身也不代表断言无价值。生产行为使用可控输入与有界完成条件；实际失败必须修复，缺环境保持待验。

| 设施 | 独立保留理由与权威职责 |
| --- | --- |
| `application-options`、`identity`、`module-link-and-plugins`、`kit-selection`、`shader-tools`、`production-app-root` | 身份/数据根、真实模块与媒体插件、Kit选择、生产qsb变体、真实App创建；native-bootstrap.md及R36目标构建闭包 |
| `source-policy`构建target、`source-format`、官方qmllint、`localization` | CPP-30/QT-19–22源码与翻译要求、格式、QML类型/导入；各最佳实践及code-style.md |
| `motion`、`scene`、`navigation`、`lighting`、`archive-pool`、`real-columns`、`presentation`、`detail-motion`、`overview`、`opening`、`track-focus`、`startup` | 独立冻结公式、边界/中断/身份/输入归属/通知及唯一帧源；对应组件契约，覆盖R36共用算法 |
| `directory-scanner`、`library-data`、`library-qml`、`online-metadata`、`media-metadata`、`json-file` | 文件系统/事务保存、模型协议、来源与取消/晚到、解码/导入失败；library-data.md。网络单元测试仅用可控响应 |
| `playback`/`playback-device`、`audio-ambience`/`audio-ambience-device`、`single-instance`、`settings`、`experiments`、`logging`、`logging-terminal`、`graphics-backend` | 可控媒体状态与独立真实设备三路验证、跨进程仲裁、持久化/模式/错误、真实终端输出及后端选择；playback/settings/logging/render-backend及相关组件契约 |
| `assets`、`branding`、`title-reels`、`cover-cache`、`rhi-atlas-queue`、`readback-queue`、`rhi-material-parameters`、`render-quality`、`render-support`、`verification-input`、`display-output-probe` | 许可/字节/几何/字体、独立品牌几何、字形与缓存、上传准入、实际材质参数、尺寸/回调寿命、探针输入结构/数值、资产批次回滚与异步显示探测完成状态；assets/branding/covers/title-reels及RHI契约 |
| `verification-window-policy`、显式可见窗口/输出检查、`macos-application-menu` | 不干扰窗口约束、实际native呈现与HDR/系统菜单；test-window-policy/display-output及R36实物事实，隐藏与可见事实分别保留 |
| RHI与封面/阵列GPU组件、`display-transfer-gpu`、`rhi-required-output` | 执行真实生产shader/pass，核对独立解析结果、附件/上传/提交/释放；对应RHI/display-output契约及R36目标后端差异 |
| QML组件回放、生产详情/总览/导航、GUI与同窗口生命周期 | 真实控件/面板/音乐舞台的交互、布局、模态、播放隔离与恢复；组件契约及R36产品接线。诊断采样入口仅按performance.md选用 |
| `runtime-materials`、`runtime-lighting` | 隔离Quick3D三表面/纸面与光场适配，R36明确保留的源参数/投影及有效输出边界；materials/lighting.md |
| `dry-tools`（含详情/总览报告的严格字段、状态集合及输出存在性）、`startup-verification`、`structure-comparison`、`test-policy`、`native-tool` | 包装器真实错误传播/失败清单、有限测量/数值错误、独立采集文件、来源结构误判、三平台CTest选集/环境隔离及工具独立编译；本契约及verification-artifacts.md |
| `launchers`、`unix-deployment`、`linux-package-tools`、`windows-deployment-report` | 参数/子进程状态、包完整性/权限/依赖/符号链接、原生strip/相对加载及部署报告的严格成功标记；平台部署契约。目标对象格式分支只在对应系统执行 |
| 冻结数学/DOM导出、`compare-ui-structure.py overview/presentation`、文字/滑尺与网络探针 | 独立来源重建、同输入结构/字体/轨迹及目标TLS/代理；web-baseline与组件契约及R36明确缺口，不是普通CTest依赖 |
| clang-tidy安全审计、资产来源核对与生成器 | 编译器定向检查所有权/未定义行为及超时/诊断错误传播、冻结hash/许可及可复现生产输入；cpp23-best-practices/assets/web-baseline。`python3 tests/tools/AssetExportTest.py`在隔离目录验证独立来源导出、滑尺操作输入、失败输出保护与已提交manifest；`node --test tests/tools/TitleVerificationTest.mjs`验证标题报告的严格成功/数值/唯一状态边界。AssetExportTest另须先构建Release prepare-assets并准备同Kit balsam，用隔离副本验证原生说明不会被重新导入覆盖；其余导出需要Node及锁定Web依赖。TitleVerificationTest需要Node；两者不加入普通CTest |

## 选择与复用

| 入口 | 范围与使用时机 |
| --- | --- |
| `ctest --preset qt-release -R 对应测试` | fast：只选择本次逻辑改动影响的源码/算法/状态/资产/模块/格式检查 |
| `ctest --preset qt-release-render -R 组件` | 按需选择真正隐藏渲染项 |
| `ctest --preset qt-release-media -R 对应测试` | 显式真实音频设备与媒体集成；纯状态/持久化仍在fast，GUI同时受窗口标签限制 |
| `ctest --preset qt-release-full` | fast+不依赖音频设备的隐藏功能集成，不含独立压力；用户明确要求完整测试/全量回归，或影响分析确认确需此完整范围时执行；该请求不包含性能采样、研究选型或独立压力 |
| `ctest --preset qt-release-visible -R 具体测试` | 显式窗口/输入/焦点/呈现/相关生命周期，不默认全选 |

- `cmake/TestPolicy.cmake`按真实入口标fast/render-integration/hidden-render/native-visible/native-media，GPU/窗口均RUN_SERIAL。改标签不能掩盖可靠检查失败。
- fast/full/render/media清YALAHOO_VERIFY_VISIBLE/FOREGROUND/WINDOW_LIFECYCLE；fast/full/render排除native-visible及native-media，fast另排除render-integration。media只选择native-media并排除native-visible。macOS默认可见OFF，disabled如实暂缓；默认窗口策略检查前台归属判定与隐藏原生窗口，不显示窗口。
- 先按被测行为、依赖、输入和运行条件判断已有结果是否仍有效，并尽可能复用。无关模块、Git版本号或重新链接导致二进制hash变化，本身不使结果失效；相关逻辑/输入/依赖或环境变化仅使对应结果失效。不影响行为的注释、文档或脚本注释改动无需专门运行测试；脚本逻辑变化检查其参数、错误传播与实际入口；代码逻辑变更只执行必要的对应检查。共享代码沿实际调用链确定范围，不自动扩大到fast集或完整回归。功能回放与性能问题分别选择；局部成本、计时、资源增长及重复重建压力先满足performance.md的静态未决门槛，不默认各跑一次。已有有效结果复用，真实失败仍保留。
- `playback`运行队列/快照、可控晚到通知和不播放的无输出媒体加载/输出脱离检查；`audio-ambience`仅运行静音事件/参数/持久化。`playback-device`保留真实暂停/seek/结束/错误及输出恢复，`audio-ambience-device`保留真实BGM交接/错误重试/QSoundEffect池；设备缺失时明确失败，不跳过为通过。startup-qml/settings-qml/playback-qml同时标native-media。
- 剩余目标系统、后端和物理设备动态检查由迁移计划R36集中管理；Linux显式OpenGL入口同样受目标能力准入约束。受影响的算法/源码/模块/构建及适用qsb变体照常检查，缺环境不减少原门槛。
- 对需要新验证的受影响行为按风险选择尺寸/主题/DPR，只覆盖其实际依赖的范围；未被改动触及的最终目标矩阵结果继续复用，未执行的目标门槛仍保留。报告区分本次实际、复用、未选、暂缓及原因。首次目标验收先区分已有共用证据与目标特有缺口，仅补后者；组件所列完整工具状态集不自动成为首次验收清单。必须实际运行才能确定的功能/画质/设备事实仍保留，不能以代表组合冒称全部通过。格式与官方qmllint的适用选择归code-style.md，同样不扩大为功能回归。
- `qml-behavior`通过ui-tests的生产QML模块执行隐藏的Qt Test单元回归，覆盖曲目按压期间的双身份复用、曲目无障碍动作/离线禁用、流派键盘焦点、零尺寸模糊采样/渐变及堆叠滑杆尺寸；不创建原生可见窗口。`production-app-root`属于fast且不依赖Review：通过生产可执行文件的`--verify-app`隐藏创建真实`App.qml`及业务接线，核对舞台、显示合成层、弹出层归属、尺寸、无QML警告、无播放和窗口策略。App根、生产QML导入/注册、合成宿主或其初始化依赖发生变化时，必须执行此检查与受影响模块的官方qmllint；组件/专用探针或仅构建成功不能替代生产根创建。实际输出与交互仍由对应集成检查证明。

## 目标欠验的合并与矩阵选择

- 执行单元先列最小检查清单和退出条件，以及唯一测试名/所选状态数；只有确需采样/压力时才列时长/次数；按工具实际参数核对计划成本，不能把TIMEOUT上界当预期耗时，也不能先跑整套来估时。无新增风险不追加无关对照或重复。只有明确采用长采样协议的具体专项才列时长，不将积欠项汇成多场景、多模式和压力一起执行的大型命令。
- 同一目标/后端/输入/相关源码下，一次有效执行可同时满足多个任务ID；按报告中的实际断言逐项对应，不能因待办分属不同小节重复运行。一次构建、格式、qmllint或qsb结果覆盖同一执行单元，修改其依赖后仅重做失效部分。不同系统/后端、物理输入和真实窗口事实不能互相代替。
- 验证包装默认消费已准备的Release产物，不自动格式化源码、切换Review配置或附带全套前置回归。只准备当前缺项所需的构建/官方qmllint/格式、冻结源输入及测试；同环境跨后端不重复生成与后端无关的源数学输入，字体/平台布局输入仍随实际目标环境校验。
- 布局改动先在DPR1选择受影响断点两侧及相关极端尺寸；首次目标输出仅补尚缺的独立布局边界，双主题在代表桌面/窄屏核对颜色、合成和主题状态，只有主题改变几何时再扩展对应尺寸。DPR1.5/2在代表桌面与受DPR影响的窄屏/文字/附件边界补验，不重复所有尺寸、交互和压力。颜色/主题耦合算法与奇数尺寸/MSAA/格式等真实组合依赖仍保留；便宜的解析/状态边界不为减少数量而删除。
- 长交互序列在代表桌面/窄屏覆盖；其他布局状态只做进入、关键几何/合成和正常返回，布局导致不同输入分支时才补对应交互。生产文字/滑尺路径必验；备用绘制路径的质量/成本对比仅在生产路径失败、具体性能调查或明确选型时显式选择，不成为每个目标的默认验收。
- 原生窗口hide/minimize/restore、物理跨屏、完整root重建和独立pass资源重建按实际需要分别证明。同一原生宿主的一次观测可覆盖多个待办；独立renderer/资源所有者的释放不能互相代替。性能/资源问题能从所有权和释放代码确定时不追加重复压力；确有未决运行行为时只观察相关所有者和必要转换，重复次数由该问题决定。
- 同一Linux系统/Kit/GPU/驱动/QRhi后端在X11与Wayland间共享构建、源数学、独立pass公式及无会话依赖的组件结果；每个QPA会话仍须真实证明上下文/swapchain、格式/MSAA/resolve、窗口/输入/焦点/DPR及呈现。窗口系统或提交分支相关的差异另验，不将整个组件矩阵再复制一次；呈现长采样的会话选择归performance.md。
- 确定性生产帧序列只补尚缺的独立状态/过渡断言，不以固定1200帧作为任务门槛；相同有效报告可共同证明各pass上传、有限值/覆盖、资源和组合输出。已有源公式/组件结果继续复用，仅目标实际输出尚有缺口时选对应pass，不因历史任务ID再逐项运行。
- 当前必要检查被工具捆绑的无关工作阻塞时，先选已有更小入口；确无可用入口才在该任务内解开最小分支并记录实际覆盖，不提前建设通用选择器/报告合并框架。不得先跑整套再称为局部，也不能把未覆盖项标为通过。已有小型解析矩阵无需为拆分增加工具复杂度。
- 组件文档所列成本采样、固定帧数和重建/换片压力是工具覆盖能力，不自动形成性能必跑清单；执行条件按performance.md。若功能/画质入口捆绑了不需要的性能或重复压力部分，先解开相关分支或使用更小入口，不因包装限制执行整套。

## 源码审查边界

沿实际入口核对构建源清单→链接/moc/qrc/qsb→QML/插件→部署加载闭包；检查C++/Qt/QML实践、路径/编码/权限、公开媒体状态、错误/取消/销毁及平台条件分支。源码行号/符号、适用输入、判定与限制足以追溯即可，不要求每轮创建固定COMMON/WIN/MAC/LNX矩阵或hash报告。性能复杂度与所有权只由performance.md的PERF清单维护。

## 门槛解释与停止条件

- 每项检查必须回答当前具体正确性、集成或目标环境问题；没有对应问题的历史报告、固定状态数、重复次数或遥测字段，不构成新增测试义务。静态结论、实际结果和缺环境分别记录；不为填满COMMON/PERF矩阵逐项新建报告或重复跑审计脚本，相关内容未变直接复用。
- 构建只覆盖实际改动目标及其必要依赖；测试需要Review工具才构建对应目标，部署检查才准备生产包。注释/文档改动不配置、编译或启动应用；格式结果可复用时不为提交再跑一次完整命令串。官方qmllint仅选受影响QML模块，不一律两组齐跑。
- `-R`使用准确名称或已确定的最小名称并集，运行前可用CTest列举模式核对展开范围；`fast`是分类，裸preset仍会选择一批测试。完整、render或visible预置均不因标签名而自动触发。TIMEOUT是失败上界，不是建议观察时长；不为压缩耗时降低正确性容差，也不靠延长超时掩盖卡住。
- 来源核对与实际运行结果分别验收；缺一份历史报告不触发重跑。当前缺口按对应组件入口选择最小状态集，不附带候选选型、成本采样或重复压力。
- 算法/结构/颜色/状态的既有正确性容差继续用于被测断言；图像数量、完整协议中的固定轮数、历史性能数字只适用于其明确范围。数值失败先检查预期/输入/生产语义；确认真实产品缺陷就修复，无效环境或采集保留限制，不改阈值追分，也不为无法测量的字段新建研究任务。
- 已有生产方案无具体缺陷或瓶颈时，不作候选API横评、最优参数搜索、额外文献/基准研究或新验证框架。当前缺口得到可靠答案、修复后的受影响断言通过即停止；不自动复核三次、补全其他后端/尺寸或追加“更稳妥”的长回归。用户另行指定的目标与范围优先。

## 当前Review入口分类

| 实际入口 | 分类 |
| --- | --- |
| rhi-bokeh-gpu、rhi-smaa-gpu、display-transfer-gpu | 直接QRhi offscreen，各平台入口均无可见窗口；实际后端能力另验 |
| startup-qml、controls-qml、experiments-qml、settings-qml、playback-qml、library-panels-qml、opening-qml、presentation-qml、overview-qml、archive-pool-gpu、real-columns-gpu、runtime-materials、runtime-lighting、rhi-opaque/shell/flicker/ao/fog/shadow/postprocess/transient-attachments/scale-gpu | macOS当前RenderControl真正隐藏；其他系统当前需native窗口，按能力变化重评 |

整场GUI显式入口为`python3 tests/runners/verify-gui.py`，范围与采集语义见[rhi-integration.md](rhi-integration.md)；它不隐式加入fast/full集合。

Review OFF不注册相应开发测试。archive/real-columns回归直接消费已提交的最小数学fixture；滑尺来源探针显式接收冻结导出文件，普通构建/CTest不依赖它。组件具体状态数/门槛只在组件契约维护。
