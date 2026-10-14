# yalahOO!!! · Qt Quick / QML 原生迁移执行计划

## 项目文档优先级

- 本计划是项目内最高优先级的实施契约；根 `AGENTS.md`、`README.md` 和 `legacy-web/` 内的开发约定、设计说明、贡献指南及其他计划、版本文档与其冲突时，以本计划为准。未冲突的约定继续适用。
- 新的用户明确要求优先于已有项目文档；长期范围、技术路线或验收要求发生变化时，同步修订本计划及相关约定。
- 历史 Web 实施和验收文档提供设计、参数与验证参照，其中的旧任务、旧授权、音频路线和发布检查不自动适用于本次原生迁移。
- 所有外观、动画与特效以冻结旧项目源码及其实际音乐入口的生产调用路径为准；源码中的布局/几何、样式、主题、运动函数、时序、材质、shader 与后处理算法是复刻依据。截图、视频、运行参数导出、数值 golden 与其他采集数据均只作为辅助参考和诊断证据，不构成高于源码的效果定义。实际渲染、浏览器/GPU、字体后端或采集过程存在问题时，不把采集结果中的缺陷固化为复刻目标；新的用户明确要求及本计划的固定差异优先于旧源码。

## 文件维护规则

**【已经完成的轮次直接移除，不允许输出过程性文字到这个文件中，除非是需要跨轮次传播的信息或者执行的任务】**

- 本文件是剩余任务清单与跨轮次实施契约，不是开发日志。禁止加入“本轮完成了”“正在处理”“检查通过”、完成勾选、流水账、聊天记录和历史进度表。
- 一个轮次的交付物及退出条件全部满足后，删除该轮次整个小节。编号保持不变，不重新编号；依赖编号指向代码、测试和验收资产，不要求被依赖的小节仍存在。
- 完成轮次涉及的接口、参数、资产路径、构建命令及固定限制，合并到本文件对应的长期契约，或保存到代码和 `docs/contracts/`；不得用保留已完成轮次代替交接。
- 未通过的验收项保留为具体任务：写清输入、修复对象、预期结果、验收方法和依赖。禁止用“继续优化”“自行研究”“视情况调整”作为任务。
- Windows 测试输出、性能样本和差异报告在本地 `verification/windows/` 生成，视觉对照在本地 `docs/media/` 生成；macOS/Linux 静态审计在本地 `verification/macos-static/`、`verification/linux-static/` 生成。这些目录及 `docs/contracts/web-scenes/` 的批量导出不进入 Git，原始产物单独归档；仓库只保留源码、产品资产、真正测试输入、必要契约和简明结论，按 `docs/contracts/verification-artifacts.md` 执行。本文件不保存过程日志。不能将私人曲库、个人配置或绝对音乐路径放入版本控制。
- **禁止再次批量提交验证产物。** 默认不提交任何运行输出；确需保留证据时，只保留证明结论不可缺少的核心文件，优先合并进现有契约：简短结论、必要指标/未决项、最小复现命令和来源标识。没有直接验证用途的文件不保留。不得提交整套截图/差分、重复报告、逐帧 JSON、全量场景导出、日志、逐机审计矩阵或完整产物 hash 清单；不得以“便于追溯”作为全量入库理由。图片或其他大体积证据只有用户明确指定个别文件时才允许例外。
- 提交前必须检查暂存文件清单、数量与体积，逐项确认新增文件属于源码/产品资产/必要测试输入或上述最小核心文件；按明确文件路径暂存，不使用全工作区暂存把生成输出带入提交。不可把运行产物改名为 fixture 或 contract 绕过规则。复现优先使用保留的生成/验证工具，原始数据只在本地或仓库外归档。
- 依次执行剩余且依赖已满足的轮次。一次执行以一个轮次为边界；不得为赶进度跳过 Windows 实机与视觉门槛。macOS/Linux 满足下述严格静态审计即可视为本阶段验收通过，结果必须标明“静态审计通过，实机待用户验证”，不能记为实机通过。

## 轮次规模与耗时约束

- 每个轮次的规模必须按执行本计划的AI代理自身的实际能力、当前上下文、工具延迟和已有验证入口评估，不采用人类工程师的开发日/工时作为轮次容量依据。一个轮次只处理一个明确集成边界。
- **每轮开始执行前必须先判断规模与耗时是否合理。** 估算代码增删行数、实现/修复、构建、测试、视觉/性能采集、审计、文档收尾及提交的总有效执行时间，并向用户简要说明范围与估算。代码修改理想不超过1000行，通常不超过2000行，**规模上限目标为两三千行；总有效执行时间上限目标为一小时**。允许必要收尾引起的小幅超出，不能机械判定3001行或61分钟不合格；如果明显将达到4000行甚至更多，或七八十分钟仍不一定足够，必须先拆分。行数按该轮最终diff的代码新增与删除相加统计，新增代码文件计入；不以净增抵消删除、不用改名或拆提交规避。生成脚本、文档与可按行计的素材暂不计入代码行数，但它们的执行/检查耗时仍计入总时间。
- 网络连接断开、用户手动终止引起的停顿间隔不计入有效执行时间；恢复时累计中断前后的实际执行时间。正常思考、工具等待、构建/测试、失败调试和重跑均计入，不能以验证、审计或后台运行名义排除。
- 预估明显超过上述规模/时间目标，或存在七八十分钟仍无法合理收尾的风险时，**必须先修改计划再开始实现**：将当前轮次一部分内容合理转移到后续具体轮次，分别写明接口、依赖、输入、修复对象、验收方法和退出条件，保留原有门槛；优先拆分实现、单后端/单DPR验证与最终选型，不能把整个复杂系统塞进“补验收”。不得开始大轮次后才靠延长会话完成。
- 执行中也持续核对规模与剩余时间，给构建、验证和提交留出余量；预计明显超出时立即停止扩展并拆分剩余任务，不得连续以“小幅超出”为由延长。已经明显超时的轮次如实报告，不能追认达标；在已有授权下只做必要的安全收尾，将未完成验收保留为待执行任务，不虚报完成、不跳过门槛。新会话或拆提交不重置同一轮次的累计时间。
- 已列出的后续轮次不是自动批准其规模；每次真正开始仍必须执行上述前置评估并按需拆分，不提前实现尚未执行的轮次。

## 每轮必读的问题与解决方案记录

- 每轮开始前必须完整读取本节及涉及的契约，再评估规模/耗时。每轮结束或拆分收尾时，必须把耗时较长才解决的问题、反复调试的陷阱写入本计划：如实、详细、准确记录问题表现、触发条件、已确认根因、解决方案、验证范围、未解决限制及避免再犯的方法。这些是跨轮次技术经验，不属于过程性文字，不需要专门豁免；不能只写“已修复”或把详细内容全部外置后仅留链接。没有确认的推断必须标明，未验证的修正不能写成通过。
- 已有记录后续被证明不准确时直接修正，并保留仍适用的触发条件与解决办法。机器日志/截图/逐帧数据保持忽略，记录保留复现入口和必要症状，不把大量采集产物纳入计划。

### R16：源文字测量探针返回陈旧字号

- 表现与触发：冻结 `setupMusicTitleLayout` 做粗搜后细搜，Chromium 中受 contain/style 影响的测量probe更新fine字号后，inline font-size与getComputedStyle仍可能不一致，后者保留前一次coarse字号。原生按fine字号实现后，窄屏字号/行框对照反复失败；只看截图容易误判为Qt字体度量错误。
- 处理：先同时记录candidate.size、inline/computed字号与glyph位置，区分请求状态与实际采集状态。导出器在stageCandidate结束、测量前，以完全相同inline CSS和children克隆并替换整个probe根，同时重绑定main/translation引用；不改粗搜/细搜算法、参数、生产样式或字体。旧未刷新数据保留在 `verification/windows/title-reels/web-raw/`，刷新数据在 `web/`，登记 `WEB-TEXT-PROBE-01`。
- 验证与限制：刷新后的fine字号与inline/computed状态一致，窄屏结构误差降至约0.12逻辑px；完整跨DPR/后端视觉仍待R16B1–B3。以后先核对源状态与实际CSS，不用改原生字号去追陈旧采集值，不为校正观测降低像素门槛。

### R16：彩色emoji在主题切换后的文字节点复用中消失

- 表现与触发：普通字形节点复用后仅更新文字颜色，彩色字体内容在昼夜切换的对照中消失；普通汉字仍存在。单看整体截图容易把问题误归为字体未加载、裁切或原生不支持emoji。逐个重建彩色节点的尝试又增加了私有字体/缓存处理复杂度。
- 处理：生产普通文字仍提交整行shaping后的glyph ID/位置；只让彩色grapheme走Qt Quick Text成熟的彩色字体路径。普通字面不再创建隐藏Text，彩色fallback显式保持Regular，匹配源font-synthesis:none；GUI只保存字体描述与字形值，render thread重新取得QRawFont并核对head身份，不迁移GUI的QRawFont/QTextLayout。私有接口集中在ReelTextNode并在配置时探测，不依赖写死Qt版本。
- 验证与限制：D3D11/DPR1两主题emoji可见，单上下文带外SSIM约0.98285/0.98172，原始/边缘带数据分别保留；这仅支持当前上下文，不能外推DPR2、OpenGL或其他系统。后续先区分普通glyph与彩色字体缓存路径，不再反复用改色遍历尝试重建彩色glyph。

### R16：整行glyph关联把行末空白并入前一个字面

- 表现与触发：改用整行glyphRuns/stringIndexes提取、合并连字后，换行末尾的space可能没有独立glyph，前一个分组的text范围延伸到line end。原生可见位置与像素正确，但结构文本出现 `gyp (Live)` 对 `gyp(Live)`、`AV gyp` 对 `AVgyp` 的差别；三个输入的两主题共六项结构门槛失败。不能凭像素接近而忽略结构失败，也不能拆字符重新shaping来迎合源观察单元。
- 处理：保留整行glyph ID/位置，非歌名单行模式按源CSS折叠并移除有效行末ASCII空白；字面text提取和宽度的cursorToX末端同时限制为 `min(nextGroupStart, inkEnd)`，而非只跳过起点已经在空白区的分组。只改text会保留约7.73–8.26逻辑px的行末空白advance，导致文本已经一致而结构宽度仍失败；不能凭正确像素放行。上下文glyph变化用sameGlyphInk比较实际描述符，不能只比较文本/字体。
- 验证与限制：修正通过title-reels/source-format及Windows Release/D3D11/实际DPR1双主题20项结构/文字对照，最大结构误差0.24087逻辑px；原失败报告保留，其他DPR/后端待R16B2/B3。以后先对齐可见行文本与whole-line关联语义，不能把结构统计问题当字体像素问题反复调参。

### R16：PowerShell直接调用GUI程序不能可靠等待退出

- 表现与触发：在PowerShell中 `& yalahoo.exe ... *> file` 可立即返回、LASTEXITCODE为空，实际GUI进程仍在采集；按这个空值继续执行会误报退出状态或启动并发GPU验证。本轮已观察到空退出值和仍存活的yalahoo进程，没有把空值当作通过。
- 处理：用捕获管线 `$output = & ... 2>&1 | Out-String` 强制等待GUI完成后读取LASTEXITCODE，再写日志并核对JSON报告。若用Start-Process，则必须 `-Wait -PassThru -WindowStyle Hidden` 并读取ExitCode；保留VerificationWindow在第一次show前置底/拒绝焦点。正常采样严格串行，不和构建或另一个GPU程序并行。
- 验证与限制：最后生产检查通过等待管线取得真实exit0，报告18项通过；所有后续GUI重建脚本必须沿用可靠等待方式，不依赖启动成功或空退出码推断采集结束。

### R16B1：隐藏候选仍重建字形节点

- 表现与触发：TitleReview切换到纯Text/Canvas候选时，生产ReelInk不可见，但其槽contentChanged仍会安排update；只核对slot.shown的updatePaintNode继续创建普通字形节点，三个候选的成本受隐藏生产工作污染。不能从父级visible=false推断所有layer/SG节点都没有工作。
- 根因与处理：ReelInk的更新入口缺少自身有效可见性检查，QML槽层仅按槽visible启用。入口加入isVisible并在隐藏时删除自身节点；layer.enabled同时核对组件和窗口可见性。内容revision匹配完整sameGlyphInk描述符，保留仍滚动的旧字面，不整槽重建；颜色、clip容量与transform独立更新，旧节点归render thread删除。
- 验证与限制：D3D11/DPR1三次生产活动诊断复用73/65/65次，纯Text/Canvas候选期间隐藏生产节点的新建计数均为零；十次恢复/释放和最终零tracked节点/renderer检查通过。该计数包含初始化/候选切换，不能宣称FPS或总显存改进；其他DPR/后端成本与生命周期仍归R16B2/B3。

### R16B1：页眉测量初始布局与附加属性

- 表现与触发：在frameSwapped回调中先改标题再立即测按钮位置，会读到尚未完成polish的中间绑定；把复用窗口resize后的第3帧当基线，又会记录1280×720暖昼的一次约0.30逻辑px准备期调整，第4帧已稳定且第8帧才发送首个换歌输入。C++ QQmlProperty读取ToolTip/Accessible附加属性还返回空值，不能把探针读取失败当作产品全名丢失。
- 处理：在发布下一帧状态前读取已呈现帧的位置；先以减少动态准备初始标题终态，在第7帧建立输入前基线并开启动画，再执行第8/21帧换歌、第15/21帧改速及第25帧减少动态。保持0.01逻辑px的位置容差，正式窗口锚点/图像门槛不变。全名由工具QML直接读取控件的真实ToolTip.text/Accessible.name，退出禁用在width仍保留的视觉退出期检查。
- 验证与限制：九尺寸×双主题18项位置、名称、定位、改速/减少动态及退出禁用全部通过；初始准备与输入阶段分开，不将准备期的复用窗口布局当成换歌位移。实际播放对象与定位执行仍依赖R24/R23，当前使用真实LibrarySession匿名ID状态，不宣称已播放音频。

### R16B1：受限环境的无头浏览器页面无响应

- 表现与边界：浏览器级CDP可连接并读版本，受限执行环境中的页面Runtime.enable不返回；换成独立页面WebSocket仍失败，相同源码/浏览器在原生权限环境成功导出20项。尚未确认Chromium内部根因，不能把超时归因于字体、源运动算法或flatten会话本身。
- 处理与复现：export-title沿用其他导出器的独立页面连接，后台/headless及外部请求拦截不变；本机发生相同症状时在授权的原生验证环境运行 `node tools/review/export-title.mjs --dpr 1`，保留受限环境失败日志，不改字体或源算法去追连接问题。原生产物仍只写本地忽略目录；没有关闭窗口/网络/像素门槛。

## 目标、范围与固定取舍

1. GUI、布局、动画、相机、材质、灯光和特效尽最大可能逼近当前 Web 音乐入口；静态相似和动画连续性分别验收，不能只凭截图宣称复刻。
2. 原生应用使用 C++23、开发机已安装的最新 Qt 版本、Qt Quick/QML、Qt Quick Controls 和 Qt Quick 3D；CMake 构建。QML 承担界面，C++ 承担模型、状态机、运动算法、文件扫描与控制器。计划中的当前版本与路径仅作为定位参考，不作为 CMake、脚本或产品的版本锁定。
3. 最终运行不依赖 Node、浏览器、WebEngine、本地 HTTP 服务、Swift 桥接、外部 FFmpeg 命令或其他第三方库。Qt 自带的插件及其内部依赖由 Qt 部署工具部署，不自行链接其内部 FFmpeg 库。
4. 音乐定位为普通播放器：播放、暂停、停止、上一首、下一首、专辑队列、进度、音量、输出设备和错误提示。全部使用 QtMultimedia 公开 API。
5. **不实现无缝播放、交叉混音、手动 PCM、重采样、音频回调、音频环形缓冲、自建音频引擎或专业输出。** 不引入 `QAudioSink`、`QAudioDecoder`、`QAudioBufferOutput`、AVAudioEngine/CoreAudio 自建路线。
6. 歌曲衔接改为普通顺序换曲；删除原三档衔接和浏览器/CoreAudio 后端选择。保留原播放按钮位置、歌曲标题和播放定位行为。音频简化不降低 GUI 复刻标准。
7. 联网能力本阶段不执行，但保留异步接口、来源模型、状态占位、配置扩展和可替换 Provider。禁止删掉以后接入联网功能所需的数据字段；不实现外部服务协议，不发送请求。
8. 暖昼/深夜、减少动态效果、0.25×–3× 动效速度、横竖屏/短屏/超宽屏、填充与真实列、列记忆、搜索不改变播放等行为保留。
9. 不迁移旧档案终端 `?original=1`、PWA、浏览器专用菜单限制、在线播放器代理和旧拆解研究入口到产品主界面。原文件保留作为历史及参照；原生产品内的详情拖动查看、画质和光效调节保留。
10. Windows、macOS、Linux 均必须正常实现，不能仅声明支持或保留空壳分支。Windows 必须严格实机构建、运行与验证；macOS/Linux 本阶段通过严格静态审计即可完成对应轮次，真实环境构建、GUI、图形和音频验证留待用户以后执行，不作为本计划收口前置条件。移动端不属于本计划。
11. 本阶段只编写与执行本地改造，不自动发布、上传私人数据、改写发行标签或删除 Web 基线。
12. 新项目正式名称为 **yalahOO!!!**，大小写及三个感叹号必须完整保留。新品牌图标和 Logo 原创设计，以 SVG 为主素材；品牌替换是用户指定的视觉差异，其余界面、动画和特效继续按原基线尽最大可能复刻。
13. Web 基线中的可见锯齿属于渲染缺陷，不属于需要复刻的设计内容。Qt 必须改善几何、玻璃、纸面及文字边缘的平滑度，同时保持轮廓位置、封面清晰度、材质、灯光与动效；不得主动重现阶梯边缘，也不得用整体模糊或噪点掩盖锯齿。

### 项目身份与品牌契约

- 用户可见名称统一为 `yalahOO!!!`：窗口标题、关于界面、桌面入口显示名、安装包说明、开场和界面 Logo。文件名、CMake 标识、数据路径及脚本参数采用稳定的安全标识 `yalahoo`，不把感叹号写入可能启用延迟展开的脚本变量或技术标识。
- CMake 项目名 `yalahoo`，目标 `Yalahoo`、`YalahooCore`，可执行输出名 `yalahoo`；QML URI `Yalahoo.Music`，源目录 `qml/Yalahoo/Music/`。在创建引擎和读取 QStandardPaths 之前设置 organizationName/applicationName 为 `yalahoo`，applicationDisplayName 为 `yalahOO!!!`；后续更改展示名称不能隐式改变用户数据目录。
- macOS bundle 展示名称为 `yalahOO!!!`，技术 bundle 标识 `yalahoo.music`；Linux desktop 文件名与图标名 `yalahoo`、显示 Name 为 `yalahOO!!!`。Qt 工程文件、部署清单和原生文档统一使用新标识；`legacy-web/` 的原产品名、版权署名、基线提交与历史资产保留原貌。
- 图标为精致的独立应用符号，Logo 为完整名称字标及图形组合。主题围绕本地音乐收藏、实体专辑盒、双 O 唱片圆环、暖昼/深夜光感；主资产必须是可编辑的真正 SVG 路径，不把 PNG 嵌入 SVG，也不直接复制上游标识或套用现成图标。
- 品牌主素材在 `resources/branding/`，原稿与生产稿均保留；桌面衍生图标在 `resources/icons/yalahoo/`，设计规范在 `docs/contracts/branding.md`，预览和验收在 `docs/media/branding/`、`verification/windows/branding/`。颜色、比例、安全区、16–32px 光学稿、稳定分组、开场中心线/弧长映射及导出命令按品牌规范执行；原创来源与 hash 在 `resources/manifests/branding.json`。R14/R20 接入页眉与开场，R30–R32 接入系统图标及平台元数据。

## 跨轮次基线与工具链

### Qt 源码风格契约

- 所有剩余轮次及后续修改强制遵守 [docs/contracts/code-style.md](docs/contracts/code-style.md)；C++/shader 由根 `.clang-format` 约束，QML 由 `.qmlformat.ini` 及同一最新原生 Kit 的 `qmlformat` 约束，编辑器采用 `.editorconfig`。维护工具使用相同的四空格缩进和逐行语句规则。
- 使用四个空格，禁止 Tab；C++ 函数、类和结构体的左花括号另起一行，控制流与命名空间的左花括号留在行尾。短函数、短分支、循环体与多个独立语句必须展开；QML 对象、属性和 JavaScript 语句分别换行。以 100 列为换行目标，字符串、URL、许可证行及语法约束可例外；不得为换行改变字面量、宏语义、声明/导入顺序或执行行为。
- 每轮提交前先执行 `cmake --build --preset qt-release --target format`，再执行 `cmake --build --preset qt-release --target check-format` 及 Release 检查；`source-format` 纳入 CTest，未通过不得宣告轮次完成或提交。不得关闭检查、临时覆盖配置或为压缩代码添加格式化豁免。
- 格式化范围为当前原生项目、测试、shader 与维护工具；`legacy-web/` 冻结源码、历史验收证据、第三方资产及构建/部署输出不格式化。仅格式整理不得修改算法、数值、资源内容或业务行为。

### 基线与参数优先级

- Web 基线提交：`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，产品版本 V0.4.1。后续工作树中的新改动不能自动改变基线。
- 原项目所有工作文件和目录统一位于 `legacy-web/`，包括原 `.github/`、`.gitignore`、`.gitattributes`、AGENTS、源码、文档、启动器和资源；根 `.git/` 是当前仓库元数据，保留原位，不嵌套另一个仓库。本计划保留根目录；根 AGENTS、README 和 Git 规则是面向 Qt 项目重新建立的文件。
- 冻结提交中的旧路径 `src/...`、`public/...`、`scripts/...`、`art/...` 在当前工作树对应 `legacy-web/src/...` 等。除明确给出根路径外，轮次中未带目录的旧模块名均指 `legacy-web/src/` 下的同名模块；旧资源均从 `legacy-web/public/`、`legacy-web/art/` 读取，新实现不得写回该基线。
- 读取冻结提交时使用 Git 中的原路径，运行工作树 Web 参照时以 `legacy-web/` 为工作目录；不能对根目录运行 npm 或把新 `src/` 当作 Web 源码。基线工具把仓库根与 Web 根作为独立参数，输出只能写到新工具/资产/验收目录。
- 产品语义首先以本计划的固定取舍为准；根 AGENTS 与 `legacy-web/DESIGN.md` 顶部、V0.4.1 专项文档仅补充未冲突的约定。外观、动画、特效及其渲染/时序参数从冻结提交源码的实际执行路径、初始化/覆盖顺序与有效分支解析；采集数据仅用于核对和诊断，不替代源码判断。
- 依据文件：`legacy-web/DESIGN.md`、`legacy-web/docs/MODEL-V0.4.0.md`、`legacy-web/docs/MOTION-V0.4.0.md`、`legacy-web/docs/MOTION-REVIEW-2026-10-07.md`、`legacy-web/docs/REAL-COLUMNS-V0.4.1.md`、`legacy-web/docs/COLUMN-POSITION-V0.4.1.md`、`legacy-web/docs/OVERVIEW-V0.4.0.md`、`legacy-web/docs/STARTUP-V0.4.0.md`、`legacy-web/docs/TITLE-CLIP-V0.4.1.md`。
- 冻结基线的复建命令、匿名输入与允许差异见 `docs/contracts/web-baseline.md`；生产模块映射为 `source-map.json`，最终运行参数索引为 `scene-parameters.json`（按 hash 引用 `web-scenes/`），固定列顺序为 `library-order.json`，运动与换片采样为 `motion-golden.json` / `presentation-golden.json`，环境输入与 GPU 样本为 `room-environment.json`。这些 JSON 均在 `docs/contracts/`；截图清单在 `docs/media/web-baseline/screenshots.json`，输入在 `tests/fixtures/web-baseline/anonymous.json`，Windows 验收在 `verification/windows/web-baseline/`。
- 不能用历史截图代替当前运行参照，更不能用任何截图或采集结果替代冻结源码。文档、截图或采集数值与源码冲突时，追踪实际生产调用链及最终覆盖值，以源码定义为准；导出的运行参数用于辅助核对，实际渲染或采集缺陷单独登记。例如当前 `music-model.ts` 中侧脊基础/暖昼粗糙度为 0.25/0.30、厚度为 0.06/0.08，不直接照搬模型日志中的旧表格。

### 工具链契约

- 当前开发机参考：Qt 安装根 `C:/Qt/`，已安装版本包括 6.11.1、6.12.0，目前最新 Kit 为 `C:/Qt/6.12.0/msvc2022_64`，qmake 报告 6.12.0；VS C++ 工具路径为 `C:/Program Files/Microsoft Visual Studio/18/Community`。这些值仅供 agent 定位，执行时重新枚举安装并选最新版本，升级 Qt 不要求修改仓库中的版本常量或路径。以 Kit 配套 ABI 和实际编译探测为准，不因为目录名 msvc2022 就切换到不存在的 VS 安装。
- 全部原生 C++ 目标与资产/审计工具采用 C++23：CMake 设置 `CMAKE_CXX_STANDARD 23`、`CMAKE_CXX_STANDARD_REQUIRED ON`、`CMAKE_CXX_EXTENSIONS OFF`，公共目标按需声明 `cxx_std_23`。必须编译实际 C++23 能力探针；编译器不满足时明确报错，不静默退回 C++20 或依赖编译器默认标准。
- Qt 自动选择由 `tools/build/` 的公共 Kit 定位逻辑负责：安装根从本机 UserPresets/环境配置读取，缺省枚举平台标准安装位置（Windows 当前参考 `C:/Qt/`）；枚举包含 Qt CMake 配置的本机原生 Kit，读取配置中的实际版本，并用 Kit 的 `qmake -query QT_VERSION` 交叉核对。按版本数值而非目录字符串排序，选择最高版本中匹配目标系统、CPU 架构与当前编译器 ABI 的 Kit；不将 Android、交叉编译 Kit 或 Qt 安装器的 Tools 目录当作桌面 Qt。
- 不在 CMake、公共 presets 或脚本中写 `6.12`/`6.12.0` 版本条件、固定版本安装目录、`EXACT` 版本要求或只允许某版本的白名单。Qt 模块用 `find_package(Qt6 REQUIRED COMPONENTS ...)` 查找，不传固定版本参数；定位出的前缀传给 CMake，具体路径只保存在本机 UserPresets/环境或构建目录。Qt6 是本项目框架系列，minor/patch 版本随本机最新安装变化。
- 本机 Qt 更新后构建入口重新检测最高版本与选中 Kit；原构建目录指向旧 Kit 时清除旧配置引用或要求重新配置，不能继续缓存旧路径。最高版本缺少必需模块、编译器 ABI 不匹配或 API/GuiPrivate 探针失败时给出明确诊断并修复，不自动降到较旧 Qt。用户显式提供 Kit 时也核对本机最高版本，过期覆盖不能绕过最新版本规则。
- `qmake`、`balsam`、`qsb`、`qmllint`、部署工具及 Qt 插件均从同一已选 Kit 或其配套 host-tools 定位并校验版本一致，不从任意 PATH 混用其他 Qt 安装；记录实际 Qt/编译器/工具版本到验收产物。
- 本机 CMake/Ninja：`C:/Program Files/CMake/bin/cmake.exe`、同目录 `ninja.exe`。使用 `vswhere.exe` 定位带 VC.Tools.x86.x64 的工具链，构建脚本导入 VsDevCmd 环境，不修改全局 PATH。
- Qt 模块：Core、Gui、Qml、Quick、QuickControls2、QuickDialogs2、Quick3D、Multimedia、Concurrent、Svg、ShaderTools；测试使用 Test、QuickTest。上述关键 Kit 模块存在；实际构建仍需 R02 的链接及运行探测。
- Qt Canvas Painter 的构建与运行闭包：CMake 通过 `find_package(Qt6 REQUIRED COMPONENTS CanvasPainter)` 和 `Qt6::CanvasPainter` 接入；QML API 使用 `import QtCanvas2D`，C++ API 使用 QCanvasPainter / QCanvasPainterItem / QCanvasPainterItemRenderer。同一最新原生 Kit 必须具备模块、QML 插件和配套工具，按实际 API 编译/导入探测，不锁定 minor/patch 或安装目录；缺模块时明确诊断，不降到旧 Kit。本机已安装信息只供定位；共用接口、失效策略、资源和验证范围见 `docs/contracts/canvas-painter.md`。
- `CMakeLists.txt`、`CMakePresets.json` 在根目录；C++ 源放 `src/`，QML 放 `qml/Yalahoo/Music/`，资产放 `resources/`，测试放 `tests/`，公共构建模块放 `cmake/`。构建输出使用 `build/debug/`、`build/release/`，部署使用 `out/windows/`、`out/macos/`、`out/linux/`；不在源目录构建。本机路径只写入被忽略的 `CMakeUserPresets.json` 或环境变量，不写死到跨平台 CMake。
- 保留统一 preset 名称 `qt-debug`、`qt-release`，不同系统用各自 UserPresets 提供 Qt 前缀和编译器环境。常规构建、测试、GUI/性能验证与验收只使用 Release；统一命令：`cmake --preset qt-release`、`cmake --build --preset qt-release`、`ctest --preset qt-release --output-on-failure`。只有明确需要获取堆栈时才编译和运行 `qt-debug`，不执行例行双配置验证；构建默认值与证据重建脚本遵循此规则，适用于后续会话。
- Web 参照可以使用仓库已有且锁定的开发依赖；不得为 Qt 应用新增第三方运行库、下载新浏览器或引入新的 npm/Python 图形比较库。
- 所有当前及后续 session 的测试、GUI/性能采集和证据重建强制遵守 `docs/contracts/test-window-policy.md`：无需真正显示的窗口保持隐藏；必须显示的 Qt 窗口在首次 show 前统一设置 `Qt::WindowStaysOnBottomHint`、`Qt::WindowDoesNotAcceptFocus`，恢复/重建后保留，禁止置顶、raise/requestActivate 或抢焦点。普通产品窗口不受测试策略影响。浏览器使用 headless，后台 helper 隐藏；采集自身渲染/读回，不采集被遮挡窗口所在的用户桌面。
- 原生构建入口、最新 Kit 覆盖/旧缓存处理、QML/qsb 别名、最小运行探针与证据重建命令见 `docs/contracts/native-bootstrap.md`；Windows 证据在 `verification/windows/native-bootstrap/`。macOS/Linux 的现有 R02 审计矩阵仅覆盖最小工程，后续轮次必须按新增实现扩展，不替代 R31/R32 的完整平台审计。

## 架构与数据契约

### 性能设计与算法审计

- 所有剩余轮次遵守 [docs/contracts/performance.md](docs/contracts/performance.md)：设计与算法审计是前置要求，benchmark是验证和定位工具。每轮按PERF-01–07展开复杂度、调用链、线程/背压、快照/增量、渲染图、缓存失效、QML/输入及生命周期；发现可确定的结构缺陷必须修复，数值通过不构成豁免。
- 保留源码效果数学和产品行为，优化调度、索引、数据布局、缓存、局部上传、批处理与资源所有权，不要求移植冻结实现中的无效工作。替代方案须有适用条件和等价性证据；减少采样、近似透明、降精度/分辨率或关闭效果不能冒充无损优化。临时解冻须用户按轮次明确授权，不向后续session延续；历史Web提交、来源资产与验收结果仍作为对照保留。
- GUI/render热路径不读盘、不解码、不等待worker、不手动转事件循环，不执行常态GPU finish/readback。不可变快照按revision更新，有界队列合并稳定ID并淘汰过期结果；只复制新内容、只提交有效范围。每帧工作量与可见池/活动动画绑定，不能随曲库总量增长。
- 资源与兼容性键分离：可见数量、相机、焦点、光照数值不能触发完整目标/PSO/SRB重建；buffer采用固定有界容量或独立容量增长策略。展开附件格式/MSAA/resolve/mips与最后使用位置，非重叠目标才可复用；阴影/实体光场/文本缓存记录完整依赖和失效规则。
- 每轮输出适用检查的源码符号/hash、设计结论、修复与局部成本及验证限制；完整产品体验/资源收敛在R10D/R28/R30集成边界验收，不把所有性能工作推迟到R28，也不强制低风险文字修改重复整套GPU矩阵。
- 维护已实现算法时，新证据使用独立目录并关联前后源码/二进制hash，旧轮次报告保留其原内容和适用范围。现有原生算法维护证据见`verification/windows/performance-review/`及对应平台同名静态审计；历史R10C证据不自动代表后来修改的源码。

### 目录及职责

```text
项目根（yalahOO!!!）/
  .git/                     当前仓库元数据；不归档、不复制
  QT_QUICK_MIGRATION_PLAN.md 剩余轮次与长期实施契约
  AGENTS.md                 Qt 项目开发约定
  README.md                 Qt 项目入口与构建/平台说明
  .gitignore / .gitattributes 原生构建忽略规则与文本/二进制属性
  CMakeLists.txt            顶层 Qt/C++23 目标、模块、测试与安装入口
  CMakePresets.json         qt-debug / qt-release 的公共 configure/build/test presets
  CMakeUserPresets.json     本机 Kit/编译器覆盖；不提交
  cmake/                    可复用的资源、部署和平台 CMake 模块
  src/
    app/        main、命令行、应用生命周期
    core/       LibrarySnapshot、Album、Track、排序/搜索、偏好、选择控制
    motion/     FrameClock、阻尼、五次轨迹、相机与阵列运动
    render/     SceneAdapter、实例表、模型、材质、图集、后处理
    media/      PlaybackController、BackgroundMusicController、InterfaceSoundController
    library/    扫描、Qt 元数据探测、缓存、旧数据导入
    services/   Provider 接口、OfflineProvider、资料缓存模型
    platform/   仅必要的平台差异；win / mac / linux 分支完整实现
  qml/Yalahoo/Music/
    App.qml                 模块主入口
    components/             Header、字轮、滑尺和基础控件
    pages/                  Browse、Detail、Overview、Opening
    panels/                 搜索、设置、曲库、光照与动效面板
    scene/                  Review 的 Quick3D 模型、材质与后处理声明；生产使用 SceneItem
  resources/
    fonts/ models/ icons/ branding/ audio/ shaders/ manifests/
  tests/
    unit/ qml/ integration/ visual/ fixtures/
  tools/
    build/ assets/ review/ deploy/ audit/ launchers/
  docs/
    contracts/              参数、接口、源模块映射与固定差异
    platforms/              Windows 使用说明；macOS/Linux 用户实机验证清单
    media/                  非私人视觉基线与成对对照图
  verification/
    windows/                Windows 实机、自动化、视觉与性能证据
    macos-static/           macOS 严格静态审计矩阵与证据
    linux-static/           Linux 严格静态审计矩阵与证据
  build/debug/ build/release/ 本机构建输出；不提交
  out/windows/ out/macos/ out/linux/ 部署输出；不提交
  启动音乐播放器.cmd / .command / .sh 普通 Qt 音乐启动器
  legacy-web/               全部原项目文件与目录；Web 基线及历史资料
```

- 此结构是各轮次创建新文件时必须遵循的目标布局；不为未执行的轮次提前生成空 CMake、假入口或占位实现。归档文件不混入新的 `src/`、`qml/`、`resources/`、`tests/`。
- `src/` 子目录各自维护 CMake 源清单；目标固定为 `YalahooCore`（模型/运动/存储）、`Yalahoo`（应用/渲染/媒体/QML）和独立测试目标。`src/platform/` 只放实际存在的平台差异，共用逻辑优先使用 Qt。
- `qt_add_qml_module` 的 URI 固定 `Yalahoo.Music`；`qml/Yalahoo/Music/App.qml` 及子目录文件使用模块相对资源别名，以 `loadFromModule("Yalahoo.Music", "App")` 加载，不依赖工作目录或文件系统 import。非 QML 资产经 qrc `/assets/` 加载；shader 从 `resources/shaders/` 使用 `qt_add_shaders` 生成 qsb。
- 新文档中的源码/资产路径均从仓库根解析；Web 资料引用显式带 `legacy-web/`。保留 `legacy-web/LICENSE`、`legacy-web/NOTICE.md` 和原署名/素材说明，新应用资源清单记录复制来源。

### 数据流与线程边界

- `AppController` 组合 `LibraryStore`、`SelectionController`、`PresentationController`、`MotionController`、`PlaybackController`、`PreferencesStore` 与资料 Provider；不建立替代 `music-app.ts` 的单个巨型类。
- 导航状态与播放状态独立：选择专辑/搜索定位/总览定位只改变导航。只有明确播放命令和当前曲目结束事件改变播放队列。
- 所有公开操作按稳定 `albumId`/`trackId`，不把列内序号、ALBUM 编号、代理模型行号当成持久 ID。
- `AlbumListModel`、`TrackListModel`、`SearchResultsModel`、`OverviewLabelsModel` 使用 QAbstractListModel；所有 begin/end 更新及 QML 属性通知在 GUI 线程。
- 扫描目录、QImage 解码、缓存读写使用受限工作线程；QMediaPlayer 探测对象放在有事件循环的专用线程，跨线程传回值对象，不在普通线程池任务中创建后等待异步媒体对象。
- GUI 线程向渲染层交付不可变 `SceneFrame` 快照：revision、camera、theme、quality、instances、selected、returning、rowLight。渲染线程不直接访问 QML/LibraryStore，不做磁盘或网络 IO。
- 每个异步任务包含 `{requestId, generation, entityId}`；发布结果前核对代次、当前实体及销毁状态。取消、扫描替换、重建、关闭必须使旧结果失效。
- 使用 Qt RAII、QObject 父子关系、QPointer 与可取消任务。GPU 资源在所属渲染线程释放；已销毁对象的晚到结果只能丢弃。

### 资料与联网扩展接口

- 保留原 `MusicAlbum` 的 description/descriptionSource、introduction、producers、creditsLookup、online、localNote；保留原查询配置字段，未知 JSON 字段读入后写回。
- `IMetadataProvider` 固定异步方法：`requestIntroduction(AlbumQuery, RequestToken)`、`requestCredits(AlbumQuery, RequestToken)`、`requestEnrichment(AlbumQuery, RequestToken)`、`cancel(RequestToken)`。以回调/信号交付 `ProviderResult`，不得让 GUI 等待同步返回。
- `ProviderResult` 固定包含 requestId、generation、albumId、kind、status、payload、source、checkedAt、retryAt、error。状态包括 Deferred、Matched、Partial、NotFound、Uncertain、Error、Cancelled。
- 本阶段唯一实现是 `OfflineMetadataProvider`：从本地缓存读取已有内容；无缓存返回 Deferred，并显示“联网资料功能暂未启用”。这不等同于 NotFound，不写入失败重试时间。
- Provider 通过构造注入或注册工厂接入，不通过 QML 判断平台/服务名称。设置保留“专辑介绍”“制作信息”“MusicBrainz”入口及状态占位；联网执行按钮禁用，来源链接显示文本且本阶段不自动打开。
- 未来 Provider 能接收联系人、开关、批量 albumIds、force 和取消令牌；预留 ProviderCapabilities，但本阶段 capabilities 统一 networkEnabled=false。全应用不创建联网请求，离线测试断言零请求。
- 值类型/未知 JSON 字段、本地 QUrl、稳定模型角色、排序列/搜索规则、导航与播放 ID、Provider 构造注入和过期结果路由见 `docs/contracts/library-data.md`；实现分别在 `src/core/` 与 `src/services/`。匿名数据与 Release 验证入口为 `tools/build/verify-r04.ps1`，跨平台新增调用链审计入口为 `tools/audit/audit-r04.ps1`。

### 存储与本地资料

- 默认数据根为 `QStandardPaths::AppLocalDataLocation`；`--data-dir` 可显式覆盖。配置、索引、封面、偏好各自分文件；偏好存 `preferences.json`，列位置仅存在会话内存。
- 旧音乐数据目录只作为用户显式选择的导入来源，不按新的 `legacy-web/` 相对路径自动推断。原根目录旁的 `../music-data-v3` 不随源码归档移动；归档后直接运行旧服务可能改变默认相对位置，参照工具必须显式配置隔离的 `MUSIC_DATA_DIR`。导入配置/索引/封面/本地备注/来源信息到新的数据根，原目录只读；不导入旧 PCM 缓存。
- JSON 使用 QJsonDocument、QSaveFile 原子提交；异步保存串行化。损坏文件保持原件并显示错误，不默默覆盖。源歌曲永不写回标签。
- ID 保留原规则：专辑 `album-` + SHA256(原生绝对目录或直属单曲路径 UTF-8) 前 24 位；曲目 `track-` + SHA256(原生绝对文件路径 UTF-8) 前 24 位。同机导入保留原 ID；跨系统路径不能自动猜测映射。
- 排序使用 Qt QCollator，zh_CN、numericMode=true、CaseInsensitive；NFKC、trim、合并空白，等价时按规范化字符串和 ID 稳定排序。不同系统的排序器可能与浏览器 ICU 有差别；固定视觉 fixture 使用冻结的列顺序，不引入 ICU/OpenCC/TagLib 弥补差异。
- 新元数据读取限于 QtMultimedia 的 QMediaMetaData；缺失信息保留 unknown/空值与现有占位，不猜测无损、位深、采样率或制作角色。已有本地索引中的丰富字段继续显示。

## 视觉、运动与渲染实施契约

### 布局与资源

- 使用真实窗口内容尺寸与 Qt 逻辑像素；不把整个应用缩放为固定 1920×1080 画布。电影式开场的参考坐标独立处理。
- 宽高比 `<1.05` 为竖屏，等于 1.05 为横屏；相机与 QML 共用一个 LayoutMetrics 对象。保留 1100/700/480 等样式密度分界，并从冻结源码解析实际规则，采集结果仅辅助核对。
- 横屏专辑信息 x=68.633333% 屏宽、右边距=max(24px,2.2% 屏宽)；短屏内容自行滚动。竖屏品牌与控件两行，返回按钮固定品牌下方。
- 横屏主页相机下移由 16:9 的 8% 连续过渡到 21:9 的 10%；向左偏移由 0 到 3.5%，更宽保持终值；详情和竖屏不套用此偏移。
- 字体先用 Qt 加载现有 MiSans WOFF2 并逐个字重验证；若字体后端不支持，固定使用 Windows 系统 DirectWrite 的 `IDWriteFactory5::UnpackFontFile` 解包仓库四个 WOFF2 为原始 SFNT，再由 Qt 加载同一组可跨平台使用的字体。资产工具仅链接系统 Dwrite.lib，读取返回的 IDWriteFontFileStream，以 Qt QFile/QSaveFile 保存；根据 SFNT 签名选择 .ttf/.otf，保留字形/字重/字距数据及输入hash。转换只在准备资产时执行，不进入产品运行路径；禁止新增字体转换库、下载替代字体或用相近系统字体冒充。
- 原功能按钮图标提取为 SVG 或 Qt Quick Shape，保留 viewBox、描边宽度、端点与颜色；不替换成系统主题图标。产品品牌标识按 R03B 原创 SVG 替换，保持原品牌容器锚点、留白、出现/退出时序与层级；不把旧品牌轮廓复制到新 Logo。
- 原生资产来源/hash、qrc 别名、模型批次/UV 约定、功能图标、音效静音事件映射与准备/验证命令见 `docs/contracts/assets.md`；生成清单在 `resources/manifests/`。原 MiSans 的 OS/2 权重与 Web CSS 权重不同，QML 使用 `MusicFonts.light/regular/demibold/bold` 的 family/styleName/weight 显式选择对应字形，C++ 使用 `assetFont(cssWeight)`，不按 CSS 数值让 Qt 自动匹配其他字体面。
- GLB 保留原资产，用 Kit 自带 balsam 导入或通过 Qt 自带导入器读取；若某 glTF 特性导入丢失，写窄用途 C++/Qt 解析器读取本项目 GLB 的 JSON/BIN、POSITION/NORMAL/TEXCOORD/indices，不引入 Assimp 等库。

### 二维绘制选型与 Qt Canvas Painter

- Qt Canvas Painter 的 C++ API 与 QML Canvas2D 均纳入实际候选，模块许可证类别、技术预览标签及使用较新的 Qt API 不作为本项目的排除理由，也不据此固定优先 C++。保留必要署名与随附文件；选型依据为源码效果/行为等价性、实际 API 能力、CPU/GPU 与资源成本、实现和维护复杂度。继续使用最新 Kit，不能为兼容旧 Qt 而放弃更适合的能力；实际缺失功能或验证失败须指出具体输入、对象和技术原因。
- 两种 API 的共用绘制契约见 `docs/contracts/canvas-painter.md`；生产滑尺刻线/路径使用 C++ Canvas Painter，固定十六槽、完整刻线的 Qt Quick 裁切、数字轨迹、统一输入路由及资源/失效规则见 `docs/contracts/navigation.md`。R20 将 Canvas Painter 用于生产二维序章的原创品牌轮廓/圆环。R16 比较文字绘制路径后确定字轮实现，不因已引入绘制模块就强制替换全部文字。普通控件/输入/焦点/Accessible 与列表虚拟化继续由 Qt Quick 承担，三维 SceneItem 的 RHI 管线按既有契约执行。
- 同组件的 C++/Canvas2D 及适用的 Qt Quick Text/Shape/ShaderEffect 对照使用同一内容、字体、路径、主题、逻辑/物理尺寸、DPR、时刻和抗锯齿目标；先满足结构、文本、运动与图像门槛，再比较成本。不能用 CPU QPainter/旧 Canvas 的测试成绩代替与实际 Qt Quick GPU 路径的比较，也不直接转抄官方 benchmark 宣称本项目提速。采用的具体组件、API、比较范围与理由写入 R14 建立的 `docs/contracts/canvas-painter.md`，原始产物只留本地忽略目录。
- 验证描边宽度/端点/连接、曲线与路径缓存、透明背景/预乘及颜色空间、渐变/遮罩/裁切、文字基线/字重/字距/shaping/emoji 和 DPR 边缘。接口不支持的效果不得擅自近似：同组件可组合 Qt Quick 合成层保持等价，或依据明确对照保留更合适的原路径。R15/R20 的生产使用是具体交付；若目标组件存在可复现的不等价或结构/资源回归，须修复或保留针对该组件的具体补修任务，不能只完成演示探针就宣告接入完成。
- 绘制只消费已有控制器/不可变状态和唯一 FrameClock，使用已缩放 dt，不新增独立 Timer/FrameAnimation/requestAnimationFrame 时钟。静态状态按内容/布局 revision 触发重绘，隐藏/遮挡后停止无效 tick；缓存按路径/样式/字体/尺寸/DPR 等实际依赖失效，动画只更新必要参数，不逐帧重建不变路径、字体布局、图像或绘制目标。C++ 同步与 QML JavaScript 成本分别记录，禁止在绘制/同步热路径读盘、解码、等待或 GPU 读回。
- 展开 Canvas Painter 与 Qt Quick 的真实渲染/合成顺序、离屏附件、MSAA/resolve、纹理上传、缓存容量、资源所有者及 hide/restore/resize/重建/退出路径，新增资源纳入累计 ResourcePlan。Windows 使用 Release、置底/不接受焦点窗口和自身渲染读回验证 D3D11 及 OpenGL；macOS/Metal、Linux/OpenGL 按相同源码、qsb 和部署闭包严格静态审计，标明“静态审计通过，实机待用户验证”。局部绘制成绩不代替 R28 的完整产品验收。

### 连续运动

- 时间单位统一秒，核心计算 double。唯一 FrameClock 接收 QML FrameAnimation 的帧间隔：`dt=min(max(dt,0),0.05)`；普通动效 `motionDt=dt*speed`，速度只缩放一次。片头和音频不受 speed 影响。
- 临界阻尼原样移植：`d=x-target`，`j=v+r*d`，`e=exp(-r*dt)`；`x=target+(d+j*dt)*e`，`v=(v-r*j*dt)*e`。普通真实列 rate=3.7，详情换片 rate=9，总览 rate=6.5。
- 搜索/跨列远记忆/归中按现有五次多项式接续 x/v/a，终点 v=a=0；时长 `1.4+clamp((hypot(lanes,rows/8)-1)/7,0,1)` 秒。系数直接移植 `music-archive-motion.ts`、`music-real-columns.ts`，不能用 Qt SpringAnimation 默认参数替换。
- 同目标不重启轨迹；坐标 rebase 只平移位置与多项式常数项，时间、速度、加速度不变。减少动态效果直接完成目标并处理回调/焦点，不能只把 duration 设成 1ms。
- 运动内核、唯一 QML 帧源、时钟/轨迹 API、revision 完成语义及冻结数值对照见 `docs/contracts/motion.md`；Windows 证据在 `verification/windows/motion/`，新增 oracle 在 `tests/fixtures/motion/kernel-golden.json`，macOS/Linux 对应审计在各平台 `motion/` 子目录。后续视觉模块只消费同一 FrameClock 的 dt/motionDt，不重复缩放速度；实际列布局和开场界面分别由 R13/R20 接入。
- 共用 LayoutMetrics、SceneFrame/SceneAdapter、相机目标/阻尼、逻辑像素投影及包围体拾取接口见 `docs/contracts/scene.md`；几何与相机的 Windows Release 重建入口为 `tools/build/verify-r06.ps1`，macOS/Linux 新增调用链审计为 `tools/audit/audit-r06.ps1`。QML 只消费同一场景对象的相机参数与布局指标，后续阵列/抽取/总览扩展该接口，不建立第二套常数。
- UI 曲线采用原 cubic-bezier，通过求解横坐标获取 progress，不把 CSS 控制点直接解释为均匀参数 t。改速从当前进度继续，不重新设置起点。
- 表面进入/退出采样当前 opacity/transform；被打断时基础时长 `max(72ms,baseDuration*sqrt(opacityDistance))`。右侧进入 24px/退出 32px，上下进入 10px/退出 6px；有独立图层的面板分别过渡。
- PresentationPhase 保留 archive/opening/detail/switch-hiding/switching/hiding/returning/selecting；退浏览文字后才能抽取，盒体和相机落稳后才能进详情字。退场中只保留最后目标，旧完成事件按 revision 丢弃。
- 字轮基础 460ms，按当前显示位置和速度重定向；按 Unicode grapheme 分段，不能按 UTF-16 单元拆 emoji/组合字。QTextLayout 的整行 shaping 位置用于文字测量，不能逐字符独立测宽破坏字偶距。

### 几何、材质、封面与光场

- 盒体 4.45×3.35×0.14，中心 (0,1.85,0)；封面纸面 3.06×3.06，中心 (0.17,1.85,0.027)。后托前面 -0.032，前盖背面 0.052、正面 0.070；不能共面，不能扁平化为无厚度卡片。
- Three 与 Qt Quick 3D 都按右手、Y 向上处理；用显式矩阵和 camera aim 对齐，不全局翻转 Z。导入节点变换只应用一次，先以已知顶点验证坐标再加入动画。
- 模型分 Frosted_Polymer、Ivory_Edges、Optical_Diffuser 三壳体批次，封面单独批次。复制项目已有几何，不重新凭视觉建模。
- 主盖透射基础/暖昼 0.96/0.94，粗糙度 0.34/0.40，厚度 0.018/0.022，IOR 1.46，clearcoat 0.08；详情粗糙度趋向 0.27/0.34。其他最终参数按冻结源码的有效分支及最终覆盖顺序确定，R01 的 scene-parameters.json 只用于辅助核对。
- 纸面局部距离 `d=max(abs(localXY-(0.17,1.85))-(1.53,1.53))`；`p=1-smoothstep(0.015,0.15,d)`；透射 LOD 因子 `mix(1.0,0.16,p)`。它随实例/抽取盒移动，不能在屏幕固定区域模糊，不能增加噪点掩盖材质误差。
- 纸面为独立漫反射，参与既有光场且不自发光；最终颜色上限限制在原图颜色范围，不能用加白增亮改变封面。透明留白、1/64 内缩、contain 与采样钳制在阵列/选中/归位/查看中共用。
- 光带移植 `music-row-lighting.ts` 和 `music-lighting.ts` 的世界空间函数：rowSpacing=0.62、laneSpacing=5.2、laneRadius=27；横排高斯半高宽按 ln(2)，列衰减按六次幂指数。默认 guided、strength=1.3、width=1.35、offset=0.3、fill=1、transitionDuration=1.1。
- light follow rate=`4.743864518390578/duration`，同向速度限幅后接续；跨列换算旧世界原点，真实列使用独立列原点。详情把行光按原轨迹收退，不新增不受主题控制的白色高光。
- 环境取基线 RoomEnvironment 的方向性与颜色，R01 从现有 Web 渲染路径导出无私人资源的环境输入/样本。Qt 内置 PBR 的强度单位不与 Three 直接等同，按固定环境和 ROI 进行限定标定。

- 灯光、世界空间光带、独立列/rebase 跟随、650ms 连续主题及纸面/壳体共享世界原点函数见 `docs/contracts/lighting.md`，冻结源码/算法 hash 在 `lighting.json`。音乐生产环境/半球/方向/spot/area 使用原源单位的自定义 GLSL；不得与 Qt 内置灯重复叠加。LTC 资源与来源在 `resources/manifests/lighting.json`，Windows Release 复建为 `tools/build/verify-r08.ps1`，对应证据在 `verification/windows/lighting/`、`docs/media/lighting/`；平台新增审计在各静态目录 `lighting/`。R12 实例与 R18 抽取/归位继续消费同一实体 origin、绝对 row/lane 和 palette，不重建灯光或重新滤波整列平移。

### 首选管线与确定的补救路径

- SceneAdapter 的产品接口固定，不因渲染实现变化修改 QML/导航。生产使用 QQuickRhiItem + QRhi/qsb；Qt Quick 3D View3D/CustomMaterial/Effect 仅用于 Review 来源对照。图形私有接口集中在 RHI 边界与能力探测，不进入 QML/导航。
- 正常管线：阴影目标 → 不透明背景/纸面/结构与供透射采样的捕获 → 磨砂壳体合成 → SSAO合成 → 景深 → 按画质SMAA → 色调映射/输出；以冻结基线的实际通道顺序和例外为准。QML文字与阅读遮罩在三维合成之后，不参与景深。
- Qt 的 SCREEN_TEXTURE/SCREEN_MIP_TEXTURE 不包含透明材质和使用该屏幕采样的物体；不得假设它会递归捕获其他玻璃。先采用与当前 Web 一致的屏幕空间近似，不要求真实光线追踪。
- CustomMaterial 显式实现局部纸面 LOD、侧脊密度/高光、印刷光场和按主题混合；不只填 PrincipledMaterial 参数就宣布完成。Effect 移植基线 SSAO/Bokeh/SMAA 的有效算法及参数，避免 Qt 默认后处理改变画面。
- 线性渲染与 sRGB 输出分开，盒体跟随原 ACES 曝光；纸面 `toneMapped=false` 的路径单独核对。避免 View3D 与自定义 Effect 重复色调映射。
- 材质标定固定用五个ROI：盖内纸面、盖外留白、侧脊、外框边缘、盒体后方背景；暖昼/深夜、正视/低角度同时计分，不能只优化单张图。损失为所有ROI等权的RGB平均绝对误差，加上轮廓平均距离/255；先通过几何门槛再开始标定。
- 先校准光强单位：每类Qt灯相对源码确认的有效强度尝试10^-4至10^4的十倍间隔倍率，按固定ROI损失选单位换算因子，暖昼/深夜共用因子，不能分别作弊。随后采用确定性坐标搜索：按曝光/环境强度/灯强度/透射/粗糙度/厚度/衰减距离顺序，依次试当前值、±10%、±5%、±2%、±1%，选跨场景损失最低且不破坏硬门槛的值，相同损失取距基线最近值；完整扫描最多3遍。透射/粗糙度范围0–1、厚度为正，IOR固定1.46、几何/封面纹理/主题颜色不参与搜索。三遍后仍有结构误差按R10触发补救；只剩参数残差时新增具体ROI补修轮次，不无限随机调参。
- 单盒材质的参数/线性捕获/局部 LOD/印刷限制、固定环境和重建命令以 `docs/contracts/materials.md` 为准，冻结源码与算法 hash 在 `materials.json`，资源清单在 `resources/manifests/materials.json`；同输入 fixture 为 `tests/fixtures/materials/single-case.json`。Windows 材质证据与残差在 `verification/windows/materials/`、成对图在 `docs/media/materials/`；平台矩阵在 `verification/macos-static/materials/` 和 `verification/linux-static/materials/`。后续灯光、图集与后处理接入保留纸面入射量、一次 ACES/一次 sRGB 输出和独立侧脊参数契约。
- 阴影/SSAO/RenderQuality 的有效参数、seed=417 核、RGB24 线性深度/独立 view normal 预通道、四预设及 8.3MP 限制见 `docs/contracts/occlusion.md`；资源来源/hash 在 `resources/manifests/occlusion.json`，同输入为 `tests/fixtures/occlusion/`，Release 复建为 `tools/build/verify-r09.ps1`。Windows 证据和成对图在 `verification/windows/occlusion/`、`docs/media/occlusion/`，平台新增审计在各静态目录 `occlusion/`；公开阴影投影/偏置/滤波、Unshaded 壳体接收/邻盒透射与背景输出的具体后续输入以 `docs/contracts/occlusion-limitations.json` 为准。画质只改变渲染资源，不重建业务状态；后续实例池须向两个几何预通道供应同一实体 origin/位置。
- 景深 41 点 Bokeh、SMAA 1x Medium/lookup、最终 ACES/sRGB 与三个固定公开 Effect 通道布局见 `docs/contracts/postprocess.md`；资源来源/hash 在 `resources/manifests/postprocess.json`，Release 复建为 `tools/build/verify-r09b.ps1`。Windows 九尺寸/双主题及真实 DPR 1/1.5/2 的 105 状态证据在 `verification/windows/postprocess/`、成对图和局部放大图在 `docs/media/postprocess/`；新增平台静态审计在各静态目录 `postprocess/`。原始画质开启实际几何 MSAA，高/极高追加 SMAA；纸面透射捕获边界及邻盒背景 ROI 的 R10 输入见 `docs/contracts/occlusion-limitations.json`，不能以 SSAA 诊断或整体模糊替代生产修复。
- 渲染目的后端固定为 QQuickRhiItem + QRhi/qsb，实施边界为 R10A–R10D；接口、固定参数、源捕获语义及各项补修输入/验收见 `docs/contracts/render-backend.json`、`render-backend.md`。Windows 70 状态证据在 `verification/windows/render-backend/`、成对图在 `docs/media/render-backend/`。生产唯一运行实现为 RHI SceneItem，`SceneAdapter.renderBackendContract` 的 implementationReady=true / activeImplementation=rhi；集成、纯值资源键/预载缓存、累计资源和输入/正常VSYNC帧入口按 `docs/contracts/rhi-integration.md` 执行。Quick3D oracle 只在 Review 构建打包，普通产品不链接 Quick3D 或打包第二套场景。R10 的历史图集 GPU 传输字节保持未知；当前 RHI 的实际 tile 目标传输字节以 `verification/windows/rhi-postprocess/` 为准，不把 CPU payload/调用次数当预算证据。不能直接删效果、自动降低默认画质或换成 WebEngine。
- RHI 不透明基础、不可变 SceneFrame 同步、阴影实际 PCF/Vogel 算法、std140/qsb 和目标/读回所有权以 `docs/contracts/rhi-opaque.md` 为准，代码在 `src/render/rhi/`；历史 R10A 六 shader 证据在 `verification/windows/rhi-opaque/`、`docs/media/rhi-opaque/`，历史重建使用对应验收提交的 `tools/build/verify-r10a.ps1`。R10B 七 shader 历史复建同样使用对应验收提交。当前源树由 `tools/build/verify-r10c.ps1` 复建 24 shader 并回归不透明基础、材质与后处理，历史证据 hash 不回写。单采样基础检查不代表产品替换或完整后处理，生产接入与测量边界以 `rhi-integration.md` 为准，基础 Review 不等于完整播放器。
- RHI 磨砂捕获、4× MSAA resolve/mips、局部纸面 LOD、显式三壳体材质、近到远不透明/远到近全局壳体稳定 ID 排序按 `docs/contracts/rhi-shell.md` 执行；当前 816-byte std140 保留 R10B 的 752-byte 前缀，新增边界按 `rhi-postprocess.md` 执行；源参数/hash 在 `resources/manifests/rhi-shell.json`。独立捕获不含透射壳体，独立合成目标禁止反馈；contain 可见边界由 centroid 几何 MSAA 抗锯齿，预乘滤波避免透明黑 texel 形成黑边。Windows 61 状态、原 58/59 邻盒通道及五 ROI 残差在 `verification/windows/rhi-shell/`、`docs/media/rhi-shell/`，对应静态矩阵在各平台 `rhi-shell/`。当前 61 pre-stage 与 50 final-output 同输入重放、五 ROI/轮廓和逐项残差在 `verification/windows/rhi-postprocess/`、`docs/media/rhi-postprocess/`，R10D 按当前及历史 `sourceIndex` / ROI 逐项复验；平均误差降低不等于所有 ROI 通过。
- RHI 图集 generation/lease/serial、256² tile 的 2MiB 稳态/16MiB 初始化队列、112-byte 几何实例、独立 AO/DOF 几何、RGBA16F 颜色、保持相同 half 精度的 R16F 标量AO、32/64 原始核及 Bokeh/SMAA/一次输出以 `docs/contracts/rhi-postprocess.md`、`resources/manifests/rhi-postprocess.json` 为准。Windows 实际 D3D11 destination box/row pitch 和已完成 GPU 整页读回证明单 tile 262144 bytes、稳定帧零、稳态批次 2097152 bytes、初始化批次 16777216 bytes；不是物理总线流量。432 实例的 120 秒未限速原生呈现与 CPU 提交分别记录，GPU timestamp 在验收运行中不可用；系统 compositor 和产品交互仍由 R10D 验证。平台静态审计见各 `rhi-postprocess/`，用户实机步骤见 `docs/platforms/`。
- 补救实现是 QQuickRhiItem + Qt 自带 QRhi/qsb：仍为 Qt Quick 原生应用。QRhi 使用所选最新 Kit 的 GuiPrivate，编译探测实际接口并将版本差异隔离在 `src/render/rhi/`；升级 Qt 后重新探测、适配并复验，不通过固定版本或退回旧 Kit 绕过私有接口变化。不引入第三方渲染引擎，不改 QML 业务接口，不让私有类型泄漏到 core/media。
- R10A–R10D 的触发已由实际对照固定；后续轮次按合同执行，不再讨论框架选择。保持源屏幕空间透射近似：Three 音乐壳体为 FrontSide，不透明捕获同样不含透射壳体，不能误增递归玻璃捕获；修复对象为 shadow/resolve/排序/深度写入及各 ROI 残差。

### 纹理与实例预算

- 填充可见池 9×48=432，COLUMN_SPACING=5.2、ROW_SPACING=0.62；真实列按有效单元显示，数量大于可见池时按当前位置虚拟化，保持唯一逻辑身份。
- RealColumns、ColumnMemory、NavigationAvailability 的独立深度、五次归中、真实单元过滤、实际显示深度选择及会话记忆遵守 `docs/contracts/real-columns.md`；输入统一进入 SelectionController。只保存偏好开关/阵列模式，不保存会话行位；PreferencesStore 串行合并后台原子保存并保留未知字段和损坏原件。Windows Release 复建与平台审计入口为 `tools/build/verify-r13.ps1` / `tools/audit/audit-r13.ps1`。
- QML 不创建每张专辑一套三维节点；共享几何、材质和 C++ 实例表。选中与归位副本拥有独立纹理租约，不能在换片时换错封面。
- 封面后台尺寸最长边 256、选中 1024；同时解码最多 3 张，已完成 CPU 图像 LRU 16MiB。图集使用固定容量分页面，单边不超过 GPU 上限；每格保留采样边界和透明留白。
- 以上为有来源的工作集/突发上限，不是整体流畅保证。R11分别约束待解码、在途展开图、完成缓存、待上传及选中/归位纹理的容量和字节，按身份合并与generation取消；不能将16MiB完成缓存宣称为总内存预算。
- 图集背景禁止跨格 mipmap/各向异性，选中纹理保留最多 8×；常态每帧最多上传 8×256² RGBA=2MiB，初始化每批最多 16MiB且让出一帧；稳定帧无重复上传。
- CoverCache、AtlasAllocator、SelectedCoverLease 的请求/来源 generation、取消、分页、槽位就绪、不可变像素、独立租约和各阶段容量按 `docs/contracts/covers.md` 执行。Windows Release 复建入口为 `tools/build/verify-r11.ps1`、平台审计为 `tools/audit/audit-r11.ps1`；原图展开保护、selected 与 atlas 分开记账，不把 LRU 或 descriptor 估算当总内存/显存实测。生产填充池的合并页面、高清分块、增量上传、独立归位租约、统一实例/拾取/rebase 坐标和数值残差处理按 `docs/contracts/archive-pool.md` 执行，复建与平台审计入口为 `tools/build/verify-r12.ps1` / `tools/audit/audit-r12.ps1`。
- Qt Quick 3D TextureData 更新可能导致整页上传；必须实测。若超过上述预算，触发 R10A–R10D 的局部上传实现，不把 setTextureData 次数当成实际 GPU 上传证据。
- 全分辨率中间目标限制 8,294,400 像素及硬件尺寸；画质配置沿用 render-quality.ts 的范围和预设。限制发生时明确显示，不偷偷保存更低的用户设置。
- R10D/R11给出附件、MSAA/resolve/mips、图集、buffer、lookup、Qt输出及resize同时存活峰值的资源清单，共享附件只计一次。估算与实测分开，像素上限不替代累计GPU资源预算；设备不支持时明确失败，不编造跨设备通用显存阈值。

## 验收契约

### 固定测试数据与可重放输入

- fixture 仅使用内置演示封面与合成/匿名标题：空库、单专辑、两张偶数列、三张奇数列、短列/长列、多于12张列、超过432张库；方形/竖版/横版封面；长中文、英文下伸字符、括号、emoji 和组合字。
- Web 与 Qt 使用同一 JSON 场景输入：viewport、DPR、theme、quality、arrayMode、albumIds、selection、motionTime、输入事件序列。fixture 不读取用户真实 music-data-v3。
- Qt `--review-fixture`、`--review-time`、`--review-script` 仅在 Review 构建启用；冻结时钟可按精确时间采样，不使用睡眠截图代替确定状态。
- 每份视觉产物携带基线提交、fixture hash、Qt/浏览器版本、图形后端、内容尺寸、DPR、画质、时间和 ROI；不能比较不同封面或不同相机状态的截图。

### 尺寸、图像和运动门槛

- Windows 实机必测内容尺寸：1920×1080、1280×720、995×858、960×900、930×900、390×844、320×720、2560×1080、3440×1440；DPR 1/1.5/2 在代表性尺寸覆盖。平台窗口装饰不进入比较。以下截图、运动、图像与性能门槛在本阶段以 Windows 真实 Qt 运行产物验收；macOS/Linux 按静态审计检查共用逻辑及平台差异实现，不要求伪造对应平台的图像。
- 先验收结构：主锚点误差≤1逻辑像素、三维轮廓/投影锚点误差≤2逻辑像素；布局分界、文本断行、裁切、控件可点击范围必须一致。
- 图像、运动与时序采集对照均为验证源码移植的辅助工具；任何指标或采样不能覆盖源码定义，不能为了贴合采集瑕疵改坏正确实现。差异判定先核对源码的有效参数、数学函数和执行顺序，再用成对产物定位问题。
- 在同尺寸同输入的 sRGB PNG 上，UI ROI SSIM目标≥0.98；三维关键 ROI SSIM目标≥0.97，平均 RGB 绝对误差目标≤6/255；文字边缘仅允许固定 1px 抗锯齿带单列统计，不能大面积遮掉差异。
- 抗锯齿验收以消除 Web 基线缺陷为目标：因边缘更平滑产生的像素差异不能单独判为复刻失败，复制锯齿不能作为相似度合格的依据。比较时把已确认的锯齿边缘区域单列为改善项，记录具体场景、局部 ROI 和成对放大图；轮廓/投影位置仍遵守结构门槛，非边缘区域继续遵守原图像门槛，不扩大排除区域掩盖几何、材质或光照偏差。基线缺陷登记见 `docs/contracts/web-baseline-defects.json`。
- 原品牌改为 `yalahOO!!!` 的字形、图形以及系统应用图标不与旧品牌做像素相等比较；在固定场景清单中仅排除实际品牌绘制边界，仍比较容器锚点、留白、布局、动画进度和相邻控件。原名出现区域以具体矩形登记，不能屏蔽整条页眉或开场画面；新 SVG 另按 R03B 的精度、缩放与主题门槛验收。
- SSIM用 Qt/C++实现11×11高斯窗口σ=1.5，C1=(0.01×255)²、C2=(0.03×255)²；另生成绝对差分图、边缘距离和分区误差。禁止新增 OpenCV/Pillow/scikit-image。
- 数值门槛是验收目标，不是预先保证。图形算法仍有差异时，先按几何→取景→颜色空间→灯光→材质→后处理顺序修复；只允许记录具体、可复现、无法进一步缩小的残余差异，不用一句“肉眼差不多”放行。
- 运动数学对照：double轨迹误差≤1e-6（位置/速度/加速度分别记录）；投影误差≤2逻辑像素；时序事件差≤1个60Hz帧。连续曲线按20/30/60/120fps与长帧输入测试。
- 必测打断序列：打开后立即返回、退字中连续换目标、详情连续三次换片及反向、搜索替换/取消、总览中切列、改速、动画中开启减少动态、扫描替换与窗口跨1.05分界。
- 动画验证除关键帧外，每段以1/120秒采样状态；用同输入生成Web/Qt成对序列，禁止“完成态一致”替代过程一致。

### 性能与平台边界

- 按用户明确修订，R10D 集成边界的性能实测为记录项，20ms 保留为参考而不阻止本轮完成或提交；结构/算法、功能、视觉、上传、资源与生命周期要求仍须通过。保留全部超限和环境不确定样本，不宣称实际性能已达标。复建使用显式 record-only 模式，普通 Review 默认仍保留历史数值门槛；完整产品预算与噪声按 R28 建立。


- 按性能契约分开执行静态/算法审计、局部诊断和实际产品验收。保留Windows参考机60Hz、原始画质/1920×1080、预热后120秒混合交互p95≤20ms及稳定阶段不重复出现>50ms的目标；20ms是工程容差，不是稳定60fps证明。RHI关闭VSYNC的Review仅验收吞吐，不能替代正常VSYNC产品节奏和系统compositor。
- 完整产品增加p99≤两个目标刷新周期的尾部目标（60Hz为33.33ms），逐场景报告一/两/三周期长帧、连续卡顿、输入收到→状态提交→对应帧提交及可测漏帧/显示延迟；不能以混合总p95覆盖局部失败。缺少真实显示时刻时标明估算/输入到Qt提交，R10D建立测量入口，R28前交付实际基线、噪声与各预算依据。
- 原始画质两主题为主要目标，高/极高、大尺寸和DPR2另列压力结果及资源峰值，不能直接套用1080p门槛，也不能省略检查。冷启动、封面就绪、扫描交接、稳定、重载、恢复分开采集，固定片头时长单列；准备期仍检查输入响应/队列，禁止扩大准备期排除卡顿或无限推迟就绪。
- 正式场景串行采样至少三次，保存每次原始时间序列/分布，优化用同机同Kit同输入前后二进制交错对照；不挑最快值、不并行构建或GPU测试。临界失败仅进行一次固定三次复核批次，按有效性、重复性和具体事件归因，保留失败样本。调整预算必须有设计推导、全样本和体验影响证据，不能为过关调数字或无限重跑。
- GUI/同步、CPU提交、GPU完成、资源重建、上传、解码与帧率分开；CPU提交不得称为GPU耗时或与GPU分位相加。不可用GPU timestamp写null，frameSwapped不是屏幕扫描时间；窗口遮挡/节流导致无效采样时明确限制，继续遵守置底/不接受焦点策略。
- 连续10次界面重建和30次详情切换后，资源回落且无单调增长；隐藏/最小化暂停视觉更新，歌曲继续播放。
- Windows 必须有真实 Release 构建、Qt 窗口交互、实际 GPU 视觉/资源/性能证据、QtMultimedia 播放与输出设备检查，以及部署产物脱离开发路径的实际运行。Debug 仅在明确需要获取堆栈时使用，不作为常规验收前置条件。静态审计、模拟媒体、CI 配置或仅 configure 成功均不能替代；失败项以具体补修任务保留。
- macOS/Linux 代码、CMake、QML/资源、shader 和部署脚本必须完整实现；严格静态审计通过即可完成本阶段对应轮次，不因缺少真实平台环境保留轮次。真实环境验证由用户以后执行，清单放 `docs/platforms/`，不作为本阶段未完成任务重新塞回计划。

### macOS / Linux 严格静态审计契约

- 每个平台形成独立 `audit-matrix.json`、`audit-report.md` 和用户实机验证清单。矩阵逐项包含 checkId、requirement、platform、sourcePath、lineOrSymbol、evidence、result、limitations；每条结论必须指向实际代码分支、CMake 条件、资源清单、脚本行或所选最新 Qt 版本的官方 API 依据。审计同时核对 C++23、Qt 自动选版、无固定版本路径及完整平台分支。只写“跨平台 Qt 已支持”、只列文件名或只靠字符串搜索不算证据。
- 审计工具放 `tools/audit/`，使用 C++/Qt 或系统现有脚本能力，输入为当前源码与规则清单，输出机器可读覆盖/发现报告；自动检查之后逐项人工推演实际调用链、平台预处理分支、资源解析与错误路径。审计修复要同步检查共享代码在 Windows 的受影响运行行为。
- **COMMON-01 构建闭包：** 从顶层 CMake 展开 WIN32/APPLE/UNIX 分支，核对每个目标、源清单、链接模块、moc/qrc/qsb、安装路径和测试注册。Windows API、Dwrite/MSVC 选项必须仅存在 Windows 分支；Apple 与 Linux 分支引用的文件/符号必须真实存在，不得 `#error`、空实现、无条件提前返回或 silently disable 产品功能。禁止把运行时后端强制写死 D3D11 到所有系统。
- **COMMON-02 路径与资源：** 检查所有文件通过 QFile/QDir/QUrl/QStandardPaths 正确处理 Unicode、大小写、空格、路径分隔符和可写目录；不依赖盘符、当前目录或硬编码 Kit。QML 模块别名、qrc 字体/图标/模型/音频、插件相对路径、打包后读取位置必须形成完整解析链；DirectWrite 字体转换只作为 Windows 资产工具，转换产物供三平台共用。
- **COMMON-03 渲染闭包：** 检查相机矩阵、clip-space 修正、纹理 Y 方向、深度范围、MSAA resolve、透明排序、屏幕采样和 shader binding/location；Qt Quick 3D 自定义片段按 Qt 跨后端生成规则审计，独立 shader 与条件 QRhi 路径检查 Metal/GL 所需代码和 qsb 变体。Windows 上可执行的 qsb 跨目标生成与产物清单核对必须运行；生成 MSL/GLSL 只作为静态证据，不能宣称目标 GPU 已验证。无法在 Windows 执行的目标编译器检查逐项写明限制，不要求安装第三方交叉工具链。
- **COMMON-04 媒体与生命周期：** 从命令到 QMediaPlayer/QAudioOutput/QSoundEffect 的调用链逐项检查 source/设备/错误/EndOfMedia/取消/销毁；保留真实公开 API 实现，不用模拟播放器作为非 Windows 产品后端。默认设备及设备消失回退、不可 seek、未知格式、资源错误、后台与恢复都有可执行逻辑；不引用 Swift、外部 FFmpeg 命令或手动 PCM。
- **MAC-01 平台集成：** 审核 bundle 目标、Info.plist、可执行文件与 Resources/Frameworks/PlugIns/qml 安装位置、macdeployqt 调用与输入存在性；确认 Metal 选择/默认行为、Retina 逻辑像素、Command 快捷键、菜单/关闭语义与可写数据目录实现。普通 `.command` 从自身路径解析产物并正确引用带空格路径，不依赖 Node、开发 Kit 路径或系统音频改造。
- **LNX-01 平台集成：** 审核 Linux 的 Qt 平台插件及依赖清单、xcb/Wayland 选择、OpenGL 默认和可选 Vulkan 的探测/回退；核对共享库搜索路径、插件/QML/媒体部署位置、`.sh` 文件的 LF/可执行位/参数透传/带空格路径及 XDG 数据路径。媒体依赖以所部署 Qt 插件的真实要求为依据，不把 Windows DLL 名称照搬成 Linux 库；权限/设备错误有提示，不静默安装软件或修改系统配置。
- **COMMON-05 证据与缺陷门槛：** 全部适用项均有可追溯证据，所有发现的功能缺失、构建/链接/资源错误、平台不兼容、部署缺陷与未实现路径必须修复；N/A 必须说明代码不适用的具体原因。Windows 共享测试、qmllint 和能在本机运行的资产/qsb 检查提供真实结果，但不充作 macOS/Linux 编译或运行结果。单纯缺少对应系统实机不构成缺陷，平台实现或审计证据缺失构成阻塞。
- **PERF-01–07性能闭包：** 按性能契约逐项审计复杂度/索引、线程/队列、复制/上传、资源/失效、算法/缓存、QML/输入和生命周期。展开目标平台实际调用链与资源计算，Windows结果不宣称目标GPU性能；新增优化记录源码hash及受影响功能/视觉证据。
- 审计报告结论固定写“静态审计通过，实机待用户验证”；报告记录审计提交/内容 hash、Qt 版本、检查覆盖及无法静态证明的运行风险。用户验证清单给出从配置、构建、部署到启动、GUI、播放和设备检查的准确命令与预期结果。后续用户实机发现问题时再开具体修复任务；本阶段不承诺未运行平台的实际性能、编码支持或像素结果。

## 待执行轮次

下列轮次按AI代理自身能力评估并遵守上述规模与时间约束，允许小幅收尾浮动；真正开始前重新估算，不把人类开发日作为容量依据。达不到退出条件时先拆出具体补修轮次。

### R16B2 · D3D11标题DPR与成本验收

- 依赖R16B1；预估代码≤300行、有效时间≤45分钟，开始前须重新评估。只处理D3D11的实际DPR1/1.5/2、双主题、九尺寸页眉与生产路径，不扩大其他系统。
- 校准并核对实际DPR，完成文字/裁切/resize/改速/减少动态与图标固定位置验收；三个绘制路径GUI状态提交/活动tick、同步/绘制、缓存与资源成本串行三次独立采样，保存各次原始序列，GPU不可测写null。静止无逐帧重排；不从微探针推导完整产品FPS。
- 完善脚本的单后端/DPR入口，记录所有失败和未运行项；受影响macOS/Linux字体/层资源/Metal/GL调用链及qsb闭包展开审计，不把Windows结果充作目标平台运行。
- 退出：D3D11三个DPR适用门槛全部通过且成本差异明确；仅把该后端的简明结论交接至R16B3。

### R16B3 · OpenGL补验收与文字路径选型收口

- 依赖R16B2；预估代码≤300行、有效时间≤45分钟，开始前须重新评估。只对OpenGL重复相同DPR/主题/文字/页眉/生命周期输入与三次成本采样，保留全部原始结果，不重跑已通过且未受改动影响的D3D11矩阵。
- 修复后端相关失败，按PERF-01–07审计；与D3D11分别解释，核对macOS/Linux共享分支与qsb变体，结论固定“静态审计通过，实机待用户验证”。
- 基于源码文字/运动门槛及全部成本结果选择生产绘制路径，在共用绘制契约写明连字/组合字、彩色字体、基线/字距、缓存/失效与资源差异。当前候选不满足门槛时保留具体补修任务，不能降低门槛收口。
- 退出：原R16全部适用门槛完成、简明结论更新；移除此轮后R17的R16前置条件才满足。

### R17 · Presentation状态机与详情静态界面

- 依赖R14、R16。交付PresentationController、SurfaceTransition、Detail.qml/TrackList.qml。
- 逐状态移植music-presentation.ts；open先退browse再enterCamera，detail就绪才进文字，close先退文字再归位；新请求覆盖pendingSelection，revision隔离完成信号。
- 详情保留编号、曲目、时长、介绍/本地备注、制作人员与来源字段；歌单用ListView虚拟化，视图滚动不影响专辑架导航。
- 异步准备布局完成后再开始文字动效，关闭浮层恢复焦点；隐藏层disabled，视觉退出期间防止底层误操作。
- 性能设计与验证：歌单仅创建可见/预留委托；状态机一次应用目标快照和批量通知，过期布局/资料结果不重置界面，相同状态不重复发信号或重建详情层。
- 退出：退字期间连选/取消只执行最后目标，快速open/back不误关闭新详情，减少动态完成路径一致。

### R18 · 详情抽取、直接换片、归位与拖动查看

- 依赖R12、R17。交付DetailMotion、ReturningCasePool、PointerDragController。
- 相机和抽取按基线同步轨道，不能改成先抽完再侧移。详情换片：退旧字→同构图内旧盒归位/新盒抽出→新字进入，不绕主页。
- 归位副本维持自身封面与状态，朝向落稳后才下降；快速换片管理有限副本池，回收只能发生在归位完成/明确失效。
- 拖动查看保留轴向、角度限幅、惯性/回正，与PointerHandler手势区分点击；按实体transform更新拾取，不用二维假旋转。
- 性能设计与验证：归位池有限容量与明确回收，分别统计纹理租约/GPU驻留；拖动/换片只更新变换和必要光场，不重建材质/图集。三次换片的输入/就绪延迟、突发上传与资源峰值分开验证。
- 退出：连续三次换片、反向、途中返回/搜索、拖动中切片和减少动态均无跳变/错封面。

### R19 · 总览取景、边缘折叠与列名两步进入

- 依赖R13、R16、R18。交付OverviewController、OverviewLabelsModel/Overview.qml。
- 总览progress rate=6.5；span=max(16.8,17.5/aspect)、distance=172、yaw=28、elevation=43，沿当前相机状态连续拉远；真实列居中与相机共用progress。
- 移植music-overview-edge.ts：外侧列在±4.5处收叠，overview/0.65完成折叠；extent≤0.12不可拾取。
- 标签按projectWorldPoint跟随整列，选中/展开优先，保留.06/.12滞回、边界缓冲与碰撞规则。第一击只展开进入按钮，第二击定位并返回标准视图。
- 性能设计与验证：标签只处理可见候选，共享每帧投影/文本尺寸；碰撞算法注明候选上界与复杂度，避免全库或无界两两比较，总览转换不重新分配完整渲染图。
- 退出：V/Esc/点专辑返回一致，播放歌名保留/按钮固定，总览换列标签不抖动，窄屏不重叠。

### R20 · 二维序章、三维开场和交互交接

- 依赖R03B、R08、R12、R14、R17。交付OpeningController、Opening.qml、原创品牌轮廓/圆环路径动画。
- 二维序章的品牌轮廓/圆环路径采用 R14 绘制契约验证的 Canvas Painter，按该组件的状态归属与实际成本选择 C++ 或 Canvas2D。复用原创 SVG 主素材的中心线/弧长与稳定分组，缓存不变路径，只更新揭示/描边/透明度等必要状态；与适用的 Qt Quick Shape/ShaderEffect 路径同输入比较，保留准确轮廓、抗锯齿、合成层级和既定时序，不通过改写三维管线换取二维绘制收益。
- 按musicOpeningFrame逐参数采样，不重写近似时间表：轮廓0.24–1.64s、标语2.72–3.58s进入/3.58–5.70s静读/5.70–6.32s退出，标语“一张一张，慢慢听”。
- 二维底纸6.32s开始消退，三维6.40s从源时间21.92开始，11.60s落稳；主页错峰720ms完成后解锁。主品牌节点复用，不通过两份品牌交叉淡入。
- 揭示 R03B 提供的新 Logo 中心线和分组，曲率/分段映射按品牌规范；原时间窗、位置、目标占位与交接节奏保持，旧品牌几何不进入生产资源。品牌区域另验收新 SVG，其余画面继续按 Web 成对对照。
- View3D准备完成后才起片头；0–6.32s完全遮住时不提交三维帧，必要时显式暂停View3D/RHI更新而非只设opacity=0。
- 性能设计与验证：资源准备/首帧/固定片头/可交互分段，事件就绪谓词启动；取消/跳过不创建第二套场景。核对完全遮挡时实际三维提交为零，不只凭opacity判断节省工作。
- 退出：跳过/重播/后台恢复/中途减少动态有明确终态、正确焦点与一次交接；20–120fps时序golden通过；Canvas Painter 已走实际二维序章路径，品牌描边/揭示、双主题/DPR、遮罩交接和重复播放资源回落通过成对验证，静止/退出后无持续重绘。

### R21 · 本地扫描、身份规则与缓存写入

- 依赖R04。交付DirectoryScanner、LibraryStore、原子JSON缓存和扫描取消。
- 保留原扩展名集合、根目录直属文件各自单曲、子目录含曲即专辑、多CD子目录分别识别；不跟随目录符号链接造成重复/循环，隐藏目录沿原规则忽略。
- 原生绝对路径UTF-8哈希与原fingerprint规则测试；根目录最多64，嵌套根去重，离线根保留旧索引。扫描只读文件，不上传内容。
- worker形成不可变快照，GUI一次应用增量/重置；保存串行，扫描中切根使旧结果失效；不重载整个窗口。
- 性能设计与验证：worker维护稳定ID/路径索引，扫描通知合并有界，GUI不随每文件重排全库；序列化/缓存写入不占帧热路径，取消释放在途任务，记录扫描期间输入响应。
- 退出：合成目录测试覆盖重扫/重命名/删除/离线/取消，UI保持响应，源文件hash不变。

### R22 · Qt元数据探测、封面优先级与旧数据导入

- 依赖R11、R21。交付MediaMetadataProbe、LegacyImporter及兼容性测试。
- 最多2个探测QMediaPlayer，每个任务独立generation、无QAudioOutput、不调用play；等待LoadedMedia/metadata/duration事件，8秒超时或InvalidMedia结束。不可探测保留文件名/未知艺术家，不删曲目。
- 映射QMediaMetaData中的Title/AlbumTitle/AlbumArtist/ContributingArtist/TrackNumber/Date/Genre/Duration及CoverArtImage；不使用弃用的ThumbnailImage，按所选最新 Qt 的实际公开字段与弃用说明核对。不访问Qt内部解码器。
- 目录封面cover/folder/front优先；直属单曲先嵌入封面再同名图；图片大小上限30MiB。现有字段/制作信息可从旧索引保留，Qt缺失字段不能覆盖为假值。
- 导入只读music-data-v3到新dataDir，复制必要封面，保持本地备注/来源/未知JSON字段；浏览器偏好无法自动读时提供显式JSON导入入口，不读取用户浏览器数据库。
- 性能设计与验证：复用两个探测器，队列去重/有界且取消可释放；超时异步处理，不阻塞GUI等元数据。30MiB压缩输入上限不代替图片展开检查，导入一次换代快照，不逐条重建模型。
- 退出：无声探测、损坏文件、超时、并发取消、索引v3/v4导入和unicode路径通过；播放能力与可索引能力分别显示。

### R23 · 音乐库/搜索面板与歌曲定位

- 依赖R16–R19、R22。交付LibraryPanel、SearchPanel、TrackFocusController。
- 复刻音乐库目录/扫描状态、搜索框/结果/空状态/流派过滤；结果按稳定ID，匹配规则沿music-search.ts，不添加网络搜索。
- 搜索目标路径：必要时退详情→归架→五次定位→打开→滚到曲目，目标行高亮按music-track-focus.ts；当前专辑直接滚动，不重抽盒。
- 点击当前播放歌名走同一定位接口；任何定位不调用play/pause/setSource。搜索替换只提交最后目标。
- 性能设计与验证：按快照revision建立有界搜索预处理/ID索引，保留trim/lowercase和冻结匹配语义；结果合并/增量通知，避免每次按键重复归一化全库和全界面重置，验证连续输入与取消代价。
- 退出：搜索中连续输入/取消、切原歌手列、当前曲目定位和无结果状态无跳变，焦点/快捷键正确。

### R24 · 普通歌曲播放、队列、进度与输出设备

- 依赖R04、R17、R22。交付PlaybackController和QtMultimedia集成测试。
- 单个歌曲QMediaPlayer+QAudioOutput；本地文件QUrl::fromLocalFile，上一/下一首由C++队列维护，EndOfMedia只处理一次。无曲时Space播放当前专辑；暂停/停止行为沿原产品。
- 重建source操作使用递增revision；来源替换先断开旧操作连接，延迟事件核对当前source/待播放意图，暂停/停止不能被晚到LoadedMedia恢复播放。
- position/duration/seekable来自公开信号，用户拖动时暂时保持拖动值，提交setPosition后恢复跟随；不可seek禁用进度拖动。playbackRate固定1，动效速度不修改歌曲。
- QMediaDevices列设备，QAudioOutput::setDevice切换；设备消失回系统默认并提示。错误只显示Qt原因和可操作的换曲/重试，不增加外部解码后备。
- 性能设计与验证：媒体信号只更新相关状态/传输控件；导航/逐帧渲染不查询或重建媒体对象，拖动seek只提交必要请求，不为图形优化改音频路线。
- 退出：支持格式的本地样本实际播/暂停/seek/停止/换曲/设备变化通过；不支持格式明确提示，没有任何PCM、无缝或专业输出代码。

### R25 · 氛围音乐、界面音效与音频设置简化

- 依赖R03、R24。交付BackgroundMusicController、InterfaceSoundController、音频设置UI。
- BGM用独立QMediaPlayer/QAudioOutput循环播放现有资源，独立音量/开关；歌曲播放前220ms淡出BGM，暂停安静，停止/队列结束650ms按BGM自身音量恢复。淡变只调公开volume，不处理样本。
- UI音效用QSoundEffect及固定小池，音量独立；可直接播放的短资源缺失时该事件静音。保留事件接口供后续加资源，不借用第三方合成器或手写PCM。
- 输出设置改为系统默认/Qt设备列表和普通格式错误信息；去掉无缝三档/外部FFmpeg/CoreAudio探测。旧songTransition/songFade只作为被忽略的旧字段保留，绝不迁回无缝行为。
- 性能设计与验证：淡变/音效用独立有界事件与小池，不新建每帧定时器/媒体实例；隐藏停止视觉任务，保留必要音量淡变与播放事件。
- 退出：歌曲/BGM/音效音量互不影响，暂停不恢复BGM，浏览定位不影响声音，最小化后歌曲继续。

### R26 · 完整设置、离线资料占位与未来联网插槽

- 依赖R04、R09B、R13、R23、R25。交付PreferencesStore、SettingsPanel和Provider注入集成。
- 复刻设置排版/控件/折叠/画质/主题/音量/排列/列记忆/减少动态/速度/重播；偏好schemaVersion固定迁移，未知字段保留，新JSON原子提交。
- 资料区域保留介绍/制作信息/MusicBrainz的标题、来源、查询状态与配置模型；已有缓存内容正常展示，执行按钮禁用并提示暂未启用。不返回伪查询成功/无结果。
- 接上IMetadataProvider异步结果路由、requestId取消、批量进度模型与ProviderCapabilities；用测试Provider注入Matched/Uncertain/Error证明UI无需改动即可接入未来实现，产品只注册OfflineProvider。
- 原设置重载只重建视觉树，不重建正在播放的媒体对象，不清除用户画质或数据；偏好与会话列记忆仍分开。
- 性能设计与验证：偏好按变化字段路由，原子保存合并且不占GUI帧；主题/速度只更新uniform/状态，画质只改对应资源，审计缓存失效与分配峰值。
- 退出：持久化/导入/坏文件处理、设置反向打开、测试Provider过期结果和零网络请求断言通过。

### R27 · 光照/速度调节面板与演示入口

- 依赖R08、R19、R20、R26。交付LightingPanel、MotionPanel、演示/研究启动参数。
- 按music-lighting-lab/music-motion-lab的既有范围接入baseline/area/guided/hybrid及强度/宽度/偏移/过渡时间；只改变已有参数，不重新创建材质或实例。
- 全局倍率改动保持当前轨迹，片头/歌曲不变；恢复默认回guided固定预设。调试信息显示正确frame分位与上传/解码计数，不泄露私人路径。
- 普通音乐入口在根目录；演示和光照研究入口在tools/launchers/，对应`--demo`/`--lighting-lab`。演示不自动读真实库或启动播放。
- 性能设计与验证：诊断开启时才采样，统计批量发布，面板不逐帧序列化大JSON或抓GPU；显示吞吐/VSYNC/CPU/GPU范围和不可用值。
- 退出：实验面板滚动不导航专辑、各倍率往返连续、默认复位一致，启动参数功能分离。

### R28 · 冷启动、异步生命周期与性能收敛

- 依赖R11–R27。交付StartupCoordinator、资源统计和生命周期/性能检查。
- 加载提示期间准备模型、可见封面与真实渲染通道；提交两帧初始化目标后才启动开场时钟。扫描完成发生在准备中时，只应用一个一致快照，交接后再允许普通更新。
- 隐藏暂停视觉tick/封面上传/非必要扫描通知，播放继续；恢复清统计时间而不清状态。重载/退出取消旧generation，资源释放幂等，QObject/GPU销毁顺序明确。
- 将实际采用的 Canvas Painter 路径纳入产品测量：滑尺连输/反向、字轮（若采用）、序章跳过/重播、主题/DPR/resize、隐藏恢复和十次重建；分别记账脚本/同步/绘制/合成、路径/文字缓存、离屏目标与上传峰值。验证共用时钟、静止零无效重绘及资源回落，不以局部绘制 benchmark 推断完整产品性能。
- 用96张合成大封面和超432匿名库验证预算；不以关景深/降低默认画质解决冷启动，必须消除重复解码、全图上传和重复实例创建。
- 性能设计与验证：先收口已有子系统PERF-01–07及具体缺陷，不能集中到本轮才首次发现架构问题。 先用同 Kit/设备/fixture 和独立正常 VSYNC/吞吐样本解释 R10D 的 18.35/20.98ms 观测差异，记录并发 GPU 占用与测量噪声、确认预算依据；不得把 R10D 的记录项视为性能达标。交付R10D输入/呈现测量的实际基线、各阶段预算依据和累计资源清单；固定场景正常VSYNC/未限速串行分开采集，至少三次并保留失败/原始样本，临界不稳定项列具体补修任务。
- 退出：性能契约、10次重建/30次换片资源回落通过；初始化耗时与稳定帧数据各自记录。

### R29 · 全GUI成对重放与视觉差异修复

- 依赖R28。交付覆盖九个尺寸/双主题/两阵列模式的成对截图与动画数值报告。
- 执行所有验收序列，按区域比较头部/文字/滑尺/盒体/光带/背景/总览；分开报告字体边缘、三维着色与音频简化导致的允许UI变化。
- 修复顺序固定为几何、布局/相机、字体、颜色、光场、材质、后处理、时序/打断；每次修复仅重跑受影响场景和关联行为检查。
- 不把扫描元数据缺失、Qt支持格式或音频简化当作GUI缺陷；不把真实玻璃差异归因于音频范围变化。若超过两日仍有差异，拆具体ROI/事件补修轮次再继续。
- 性能设计与验证：视觉修复涉及采样、通道、资源或调度时复核对应PERF项及局部成本；图像通过不能覆盖新增overdraw、重复资源或输入卡顿。
- 退出：结构/运动硬门槛满足，图像目标最大程度达到，残余差异有成对证据和不能进一步缩小的具体原因；不得使用笼统“已完全复刻”。

### R30 · Windows部署、普通启动器与干净环境验证

- 依赖R29。交付Release目录、部署脚本、根目录Windows普通启动器及Qt使用说明。
- `tools/deploy/` 使用 windeployqt，`--qmldir` 指向根 `qml/`，按生产导入与链接闭包部署 Qt Quick、Multimedia 及 Qt 随附媒体依赖，生产不打包 Quick3D 研究场景/插件；打包 qrc 资产与 MSVC 运行库要求到 `out/windows/`，不复制整个 Qt 安装目录。
- 纳入实际 Canvas Painter 生产闭包：部署 Qt6::CanvasPainter 运行库及 Canvas2D 被采用时的 QtCanvas2D QML 模块/插件与其真实依赖；即便主界面使用 C++ 注册绘制项，也要核对打包后资源/shader 与 RHI 初始化。脱离开发 Kit 运行滑尺及二维序章，检查无缺失模块/插件、正确透明合成和重建行为，不把 Review 探针或未采用的比较实现一起打包。
- 接入 R03B 原创 ICO 到 Windows 可执行资源，Qt 窗口图标及关于页使用同源 SVG/PNG；显示名 `yalahOO!!!`、输出文件 `yalahoo.exe`。实机检查 Explorer、窗口/任务栏及大/小尺寸图标；macOS ICNS/bundle 和 Linux desktop 图标由 R31/R32 按同一品牌清单接入并静态审计。
- 用无Qt开发PATH、无Node/FFmpeg路径的临时环境运行；验证离线启动、demo、unicode音乐路径、设备列表、可支持样本和字体加载。
- 普通启动器只找合法安装/构建产物并启动Qt程序，不后台启动Web服务；演示/光照启动器继续在tools/launchers/。
- 性能设计与验证：部署Release通过正常启动路径复核主要性能/输入场景、冷启动和资源峰值，区分开发Review/产品开销，记录实际尺寸/DPR/刷新/VSYNC，不转抄旧benchmark。
- 退出：Windows Release 实际脱离开发环境运行，GUI/动画/特效、真实本地音频播放、暂停/seek/换曲/音量/设备行为与代表性尺寸/DPR验证通过；记录 OS/GPU/Qt/后端及运行证据。数据路径符合QStandardPaths，部署包不含私人库/缓存/调试基线。静态检查不能代替本轮实机验收。

### R31 · macOS完整实现与严格静态审计

- 依赖R30。交付同一 C++23/Qt 源码的 macOS CMake 分支、`tools/build/` 与 `tools/deploy/` macOS 脚本、根普通 `.command`、`verification/macos-static/` 审计产物及 `docs/platforms/macos.md`。使用该开发机安装的最新原生 Qt Kit，不固定版本；不要求当前具备 macOS 实机。
- 完整实现 bundle/Info.plist、资源路径、macdeployqt QML/媒体插件部署、Retina 逻辑像素、Command 菜单/快捷键、关闭与后台语义；产物放 `out/macos/`，不引用 Swift 桥接。根普通 `.command` 新建为 Qt 启动器；原脚本继续在 `legacy-web/` 保留，Qt 演示/研究入口在根 `tools/launchers/`。
- 按 COMMON-01–05 与 MAC-01 逐项展开实际源码/构建/部署链，审核 Metal/字体/qsb 资源与媒体设备逻辑；共享功能在 Windows 运行回归。修复所有可确定缺陷，禁止空分支、直接禁用功能或用 Windows 截图作为 macOS 图像证据。
- 补齐 Canvas Painter 的 Metal/Retina、绘制项与三维 SceneItem 合成次序、资源/shader 和实际 Canvas2D 导入（若采用）的 bundle/macdeployqt 闭包；审计同步/线程所有权、透明与裁切、DPR/cache 失效和重建释放。用户实机清单覆盖实际滑尺/序章及采用的文字路径，不能仅凭模块导入成功认定绘制通过。
- 用户验证清单列出 Qt Kit 设置、configure/build/test、macdeployqt、bundle 启动，以及代表性浏览/详情/总览/片头/玻璃 ROI、本地播放/暂停/seek/设备变化的命令或步骤及预期结果；其中真实执行结果留待用户填写。
- 性能设计与验证：补齐PERF-01–07在Metal/Retina上的尺寸/峰值、同步、缓存失效和生命周期推演；用户清单包括正常VSYNC/吞吐、输入和准备时延，不用Windows成绩填写目标结果。
- 退出：平台实现与脚本完整，严格静态审计所有适用项通过，缺陷清零，结论为“静态审计通过，实机待用户验证”；满足后删除本轮。缺实机环境不阻止本阶段验收，不宣称实际 macOS 构建、Metal 或音频已通过。

### R32 · Linux完整实现与严格静态审计

- 依赖R30。交付 Linux CMake 分支、`tools/build/` 与 `tools/deploy/` Linux 脚本、根普通 `.sh`、`verification/linux-static/` 审计产物及 `docs/platforms/linux.md`。不要求当前具备 Linux 实机。
- 完整实现 C++23、该开发机安装的最新原生 Qt Kit 与 CMake/Ninja 构建及 `out/linux/` 部署，不固定 Qt 版本；明确 QML/平台/媒体插件、共享库搜索位置与 Qt 所需系统依赖。优先 OpenGL，可选 Vulkan 经探测可回退，X11/xcb 与 Wayland 所需路径和配置均实现。普通 `.sh` 从自身路径解析产物，保留参数，设置正确 Git 可执行位。
- 按 COMMON-01–05 与 LNX-01 检查大小写/Unicode/XDG/权限路径、字体与 shader、触摸板 pixelDelta、默认设备及错误处理；展开所有非 Windows 分支而非只审阅入口文件，修复每项静态可确定缺陷。系统包要求按实际 Qt 插件写明，脚本不静默修改系统音频/驱动配置。
- 补齐 Canvas Painter 的 OpenGL/所启用 Vulkan、X11/Wayland 合成、资源/shader 和实际 Canvas2D 导入（若采用）的库/插件闭包；审计同步/线程所有权、透明与裁切、DPR/cache 失效和重建释放。用户实机清单覆盖实际滑尺/序章及采用的文字路径，Windows OpenGL 结果不代替 Linux 实机绘制。
- 用户验证清单列出 configure/build/test/部署/启动命令、X11 与 Wayland、DPR/窗口尺寸、浏览/详情/总览/片头、默认设备与播放/seek/设备变化检查及预期结果；真实环境检查以后由用户执行。
- 性能设计与验证：补齐PERF-01–07在GL/可选Vulkan及X11/Wayland上的格式/尺寸/峰值、同步与节流推演；用户清单含正常VSYNC/吞吐与输入/准备时延，不从D3D11推断目标GPU效率。
- 退出：平台实现与部署闭包完整，严格静态审计所有适用项通过，缺陷清零，结论为“静态审计通过，实机待用户验证”；满足后删除本轮。缺实机环境不阻止本阶段验收，不宣称实际 Linux 构建、图形或音频已通过。

### R33 · 原生主入口收口与最终文档

- 依赖R30–R32及所有视觉补修任务。交付根启动器、README/开发说明、源模块映射和最终离线功能清单。
- 默认产品入口指向Qt；全部 Web 源码/基线/研究资产继续隔离在 `legacy-web/`，Qt 源码、QML、资源与测试遵循根目录结构，普通用户流程不再要求Node、浏览器、Swift或外部FFmpeg。
- 文档明确普通播放、Qt支持格式边界、联网接口暂未启用、导入旧数据方式、Qt构建/平台部署命令和仍可复现的视觉残差；不恢复本计划已排除的能力。
- 最终核对 Windows 实机构建/资源/GUI重放/离线Provider/普通播放/部署证据，以及 macOS/Linux 完整实现、严格静态审计矩阵和用户实机验证清单。Windows 未运行或未通过的必需检查保留具体任务；macOS/Linux 静态审计通过即满足本阶段要求，不因用户未来实机验证尚未执行而阻止收口。
- 性能设计与验证：收口核对性能设计审计、资源清单、逐场景多次原始样本、预算依据/测量范围与残余风险；不能以平均FPS、CPU提交或Review报告承诺所有设备完整体验。
- 退出：所有必需轮次有真实交付及对应平台要求的验收资产，Windows 主入口严格实机可用，macOS/Linux 标明“静态审计通过，实机待用户验证”；本文件删除已完成轮次，只保留未来联网接入合同和必要的长期约束。

## Qt官方实施参考

以下仅作为公开接口语义依据；执行时优先读取所选最新 Qt Kit 配套文档并核对实际 API，计划中的本机版本参考不构成版本要求。不因查阅文档而在产品中启用联网功能。

- [QML与C++集成](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html)
- [Qt Quick性能建议](https://doc.qt.io/qt-6/qtquick-performance.html)、[Qt隐式共享](https://doc.qt.io/qt-6/implicit-sharing.html)和[QQuickRhiItemRenderer同步/资源边界](https://doc.qt.io/qt-6/qquickrhiitemrenderer.html)
- [Qt Quick帧动画](https://doc.qt.io/qt-6/qml-qtquick-frameanimation.html)
- [Qt Canvas Painter 模块](https://doc.qt.io/qt-6/qtcanvaspainter-index.html)、[QCanvasPainterItem](https://doc.qt.io/qt-6/qcanvaspainteritem.html)和[Canvas2D](https://doc.qt.io/qt-6/qml-qtcanvas2d-canvas2d.html)
- [Qt Quick 3D自定义材质及屏幕纹理限制](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)
- [Qt Quick 3D后处理Effect](https://doc.qt.io/qt-6/qml-qtquick3d-effect.html)
- [Qt Quick 3D实例表](https://doc.qt.io/qt-6/qml-qtquick3d-instancing.html)
- [QQuickRhiItem及QRhi版本兼容边界](https://doc.qt.io/qt-6/qquickrhiitem.html)
- [Windows系统DirectWrite字体容器解包](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefactory5-unpackfontfile)
- [QMediaPlayer](https://doc.qt.io/qt-6/qmediaplayer.html)、[QAudioOutput](https://doc.qt.io/qt-6/qaudiooutput.html)、[QSoundEffect](https://doc.qt.io/qt-6/qsoundeffect.html)、[QMediaMetaData](https://doc.qt.io/qt-6/qmediametadata.html)
