# yalahOO!!! · Qt Quick / QML 原生迁移执行计划

## 项目文档优先级

- 本计划是项目内最高优先级的实施契约；根 `AGENTS.md`、`README.md` 和 `legacy-web/` 内的开发约定、设计说明、贡献指南及其他计划、版本文档与其冲突时，以本计划为准。未冲突的约定继续适用。
- 新的用户明确要求优先于已有项目文档；长期范围、技术路线或验收要求发生变化时，同步修订本计划及相关约定。
- 历史 Web 实施和验收文档提供设计、参数与验证参照，其中的旧任务、旧授权、音频路线和发布检查不自动适用于本次原生迁移。
- 所有视觉工作强制遵守下一节“外观复刻的绝对基准”；原项目实机图仅作大致参考，禁止逐像素对比或复制其渲染缺陷。

## 外观复刻的绝对基准（所有轮次强制遵守）

**所有外观方面的内容——GUI、布局、字体与样式、几何、相机、主题、材质、光照、shader、后处理、特效和动画——都必须以原项目的算法和代码及其实际生产调用路径为绝对基准。目标是精确到每个像素地复现其源码理论上应有的渲染效果，也就是不包含渲染瑕疵的理想状态；不是复刻某台机器、某次浏览器或某张截图的输出。用户明确要求的新品牌和固定产品差异仍优先。**

**原项目的实机渲染图、截图和视频只作大致视觉参考。其存在较多锯齿、模糊等渲染缺陷，绝不允许与这些实机图进行逐像素对比，也绝不允许逐像素复制其中的缺陷。不得以SSIM、RGB误差、差分图、文字边缘带或浏览器GPU读回逐字节相等，驱动实现、调参、选型、补修或验收；即使用作“诊断”也不例外。**

- 先逐项还原源码的布局/几何、数学函数、有效参数、执行顺序、颜色/合成语义、材质/光场、状态和时序。通过源码推导及直接执行源码生成的数学/状态测试验证实现，不从实机图反推字号、基线、模糊、采样、几何、光照或运动参数。
- 原生实际运行用于检查实现是否完整、质量是否正确，以及漏画、错位、裁切、错误合成、可见锯齿、异常模糊等产品问题；发现问题必须回到算法、代码和渲染链定位原因。应消除渲染瑕疵，保持清晰、平滑的理论效果。源码有意定义的景深、背景模糊、透明度和运动模糊属于设计，须与非预期的模糊/失真分别判断。
- 源码可直接推导的坐标/投影、布局尺寸、运动位置/速度/加速度和时序数值仍可严格核对；它们是代码/数学验证，不是截图逐像素匹配。源码与实际输出有矛盾时先查源码调用链、原生实现、后端及采集问题，不能迁就有缺陷的实机输出。
- 本条覆盖本计划后文、所有历史契约、验证脚本和R34中旧的逐像素/SSIM/RGB/边缘带要求。历史图像数字仅作历史记录，不再执行相应比较，不保留为待修目标或完成门禁，也不改写旧记录为通过。已有工具若耦合这些比较，须分离后只执行源码、结构、行为、实际质量、资源及性能检查，不能以工具的旧设计为理由继续逐像素比较。

## 文件维护规则

**【已经完成的轮次直接移除，不允许输出过程性文字到这个文件中，除非是需要跨轮次传播的信息或者执行的任务】**

- 本文件是剩余任务清单与跨轮次实施契约，不是开发日志。禁止加入“本轮完成了”“正在处理”“检查通过”、完成勾选、流水账、聊天记录和历史进度表。
- 一个轮次的交付物及退出条件全部满足后，删除该轮次整个小节。编号保持不变，不重新编号；依赖编号指向代码、测试和验收资产，不要求被依赖的小节仍存在。
- 完成轮次涉及的接口、参数、资产路径、构建命令及固定限制，合并到本文件对应的长期契约，或保存到代码和 `docs/contracts/`；不得用保留已完成轮次代替交接。
- 未通过的验收项保留为具体任务：写清输入、修复对象、预期结果、验收方法和依赖。禁止用“继续优化”“自行研究”“视情况调整”作为任务。
- 各平台实际运行输出在本地`verification/windows/`、`verification/macos/`、`verification/linux/`生成，严格静态矩阵在对应`verification/*-static/`生成，视觉对照在`docs/media/`生成；路径表示证据所属环境，不规定验证方式。这些目录及`docs/contracts/web-scenes/`批量导出不进入Git，原始产物单独归档；只保留源码、产品资产、真正测试输入、必要契约和简明结论，按`docs/contracts/verification-artifacts.md`执行。不能将私人曲库、个人配置或绝对音乐路径放入版本控制。
- **禁止再次批量提交验证产物。** 默认不提交任何运行输出；确需保留证据时，只保留证明结论不可缺少的核心文件，优先合并进现有契约：简短结论、必要指标/未决项、最小复现命令和来源标识。没有直接验证用途的文件不保留。不得提交整套截图/差分、重复报告、逐帧 JSON、全量场景导出、日志、逐机审计矩阵或完整产物 hash 清单；不得以“便于追溯”作为全量入库理由。图片或其他大体积证据只有用户明确指定个别文件时才允许例外。
- 提交前必须检查暂存文件清单、数量与体积，逐项确认新增文件属于源码/产品资产/必要测试输入或上述最小核心文件；按明确文件路径暂存，不使用全工作区暂存把生成输出带入提交。不可把运行产物改名为 fixture 或 contract 绕过规则。复现优先使用保留的生成/验证工具，原始数据只在本地或仓库外归档。
- 当前平台主线按列出的顺序和实现依赖连续执行，一次以一个轮次为边界；不得因困难跳过当前环境可执行的任务。按下述动态验证规则，将当前环境确实无法执行或无法可靠验证的独立任务移入最后R34专项轮次，保留输入、原门槛、失败记录和依赖；移交不等于通过，也不取消实机要求。

## 当前环境能力、动态验收与执行顺序

- 当前主力环境为macOS/Apple Silicon；Windows旧报告和各平台历史静态结论只说明当时的源码/设备/范围，不决定后续验证方式。每轮开始先逐项核对实际系统/CPU、最新原生Kit、后端/设备、工具与匿名输入、采集入口和不干扰桌面策略，给出可执行性依据；不能只按平台名称推断可以或不可以验证。
- 能确实可靠验证的任务必须在当前轮真实执行并通过，例如本机Release构建/CTest、原生字体与资源、真实Metal离屏、公开媒体事件和本机部署检查中已具备可靠入口的部分。离屏是真实GPU运行，但其结果只证明对应画面/状态/资源，不证明窗口曝光、正常VSYNC、实际显示、输入到光子或输出设备听感。
- 当前环境无法执行或不能可靠验证的部分暂用COMMON-01–05、目标平台集成项和PERF-01–07严格静态审计；必须在原轮删除前把实机任务移交最后R34，写明原轮/任务ID、目标平台/后端/设备、能力缺口及依据、输入与源码版本、修复对象、运行入口/预期结果、原门槛、实现依赖及受影响主线回归。不存在可靠样本不等于通过；已知失败不得改成未验证，也不得因审计通过丢掉补修对象。
- 可安装的常规开发工具缺失先补齐；当前源码或可靠本机入口已经能够确定并修复的产品缺陷、当前可靠检查的失败、缺少应由本轮提供的验证入口不能作为跳过理由。确实依赖其他系统/后端/硬件或不可靠采集条件的诊断与修复，才允许后置；源码能确认的缺陷仍在当前轮修复。任务部分可执行时完成该部分，只移动不可执行子任务，不将整个轮次无故跳过。
- 当前主线顺序为`R19D → R19E → R19F → R19G → R19H → R20 → … → R33 → R34`。R19A–R19H是用户要求的历史源码复查，必须紧接R19、在R20前全部完成，不以继续新功能或后置R34替代。仍须满足各轮实现依赖，不能因为R21等依赖较早满足就越过当前平台的前序轮次。所有原来的“依赖R16”在R17–R33中指当前平台实现和阶段验收；不把R34内跨后端最终收口作为前置，从而形成循环或阻断主线。
- R30–R32按顺序完成可执行实现与严格审计；当前没有目标环境的构建/部署/运行部分移交R34，后续依赖只等待实现与阶段交接。当前macOS的R31可靠构建与部署入口必须真实执行，不因历史“仅静态”约定省略；尚不可靠的可见/呈现检查仍后置。若环境变化，下一轮重新评估可验证项；已经移交R34的任务默认集中在专项阶段处理；用户明确要求提前补可见窗口验收时，只处理已实现的指定欠验边界，通过条目从R34删除，不提前实施后续功能。
- R34是计划最后的特殊轮次，承接全部未执行平台/后端问题修复及必需实机验证。执行前依据真实目标环境逐项评估、按依赖串行处理并遵守单轮规模上限，必要时拆成明确的小轮次；没有相应环境时保留未完成任务，不能删除R34或宣称全平台最终验收完成。主线阶段完成、静态审计通过与最终实机完成分别报告。

## 验证门禁分层与去重（所有后续轮次）

- 日常`ctest --preset qt-release`只运行快速源码/算法/状态、资产/模块及格式门禁；通过不等于GUI/全平台验收。`qt-release-full`运行当前可可靠隐藏的完整自动回归，排除需要显示的窗口；`qt-release-render`只选真正隐藏的渲染集成项，可再用`-R`选择受影响组件。实际分组见`cmake/TestPolicy.cmake`与`docs/contracts/test-gates.md`，不以平台名称假定所有工具都能离屏。
- 原生窗口检查使用显式`qt-release-visible -R 具体测试`或具体产品验证入口，只在相关窗口/焦点/输入/呈现/资源改动需要时执行；不能因开启`YALAHOO_MACOS_VISIBLE_TESTS`或需要一次完整回归，就把所有可见历史测试混入日常门禁。前台仍须当轮用户授权；不授权时遵循置底/拒绝焦点策略，无法可靠验证的目标按原规则交接。
- 修改后先跑受影响的源码/数学/结构与隐藏组件检查，最终跑一次快速回归；共享状态/渲染/模块/资源或构建集成变更时补一次完整自动回归。相同二进制/同环境已有效通过的结果可复用，不因文档、格式或没有语义影响的收尾反复重跑全部矩阵；出现新改动、失败或未决风险才扩大回归。
- 功能事件序列通常一次有效运行即可。三次独立采样只用于正式性能、成本比较或具体不稳定问题的复核，不把每个后端×DPR×主题的功能测试机械重复三遍。尺寸/双主题/DPR矩阵按改动风险选择：字体/采样/布局/后端共享变更覆盖对应完整范围，其余先代表性与边界；各阶段明确的最终目标平台矩阵保留，不把缩小日常门禁写成已验收未运行范围。
- 十次生命周期/三十次换片等压力序列在相关所有权、缓存、图形上下文或状态机边界执行；同一实现无需在每个尺寸/DPR/主题和每次功能重复中再做整套重建。没有新增相关风险时复用有效结果，原生窗口专项避免反复show/抢焦点，隐藏资源验证保持真正隐藏。
- 旧原图比较门禁不进入任何测试preset，解除耦合由R19A–R19H按组件完成；数学/合成输入的独立工具单元测试不等于运行原实机图匹配。禁止以“测试已存在”为理由继续图像追分，也不为减少耗时删除有必要的功能、资源、实际质量或目标平台最终检查。
- 报告明确列出本次实际执行、复用、未选择和暂缓范围及理由，不把被筛选项记为通过。GPU/窗口集成串行运行，不能用并发增加桌面干扰或污染成本样本；所有生成物继续保持本地忽略。

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

### R16：PowerShell GUI等待与退出码必须分别确认

- 表现与触发：直接`& yalahoo.exe`可能立即返回，输出管线可用于等待，但不能保证WIN32 GUI更新调用者的LASTEXITCODE。R16B2V诊断的console ui-audit先返回像素失败6，随后实际GUI浏览report.passed=true，包装函数却继续读到陈旧6；不能把旧非零值当作本次GUI失败，也不能把之前的0当作GUI通过。
- 处理：GUI验证使用ProcessStartInfo（UseShellExecute=false、CreateNoWindow=true、WindowStyle=Hidden、ArgumentList逐项传参）；stdout/stderr同时ReadToEndAsync，WaitForExit后读取该Process.ExitCode，再核对真实JSON报告。`tools/build/verify-r16.ps1::RunGui`直接记录actual GUI exit，不依赖LASTEXITCODE。也可Start-Process -Wait -PassThru -WindowStyle Hidden后读ExitCode；Qt窗口仍必须首次show前使用VerificationWindow置底/拒绝焦点。console工具继续核对其实际exit及报告，诊断的像素6不豁免其他门禁。
- 验证与边界：本轮同一DPR1.5生产浏览报告18项通过，Process实际退出0，证明原包装错误来自陈旧退出码。输出等待和退出码证明是两件事；所有后续GUI重建脚本必须使用进程对象，不依赖启动成功、空值或管线结束推断GUI退出状态。
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

### R16B2：隐藏Canvas候选污染分组成本与恢复失效

- 表现与确认根因：原`CanvasTitleProbe`将controller.changed直接连接update；即使QML候选不可见，Canvas仍在普通glyph与Text候选的120帧区间绘制约96–120次。原进程级Canvas计量混合了这些工作，只补入分组差值才暴露问题。GUI只加isVisible守卫后仍有2–3次初始化/已排队的绘制回调；不能据此把其余路径的原成本当作隔离结果，也不能假定隐藏会自动取消Qt已排队的工作。
- 处理：GUI的内容/有效可见性请求只在item可见时update；render同步先保存item/window有效可见性，隐藏时不遍历faces，paint在reset后返回、不调用字体或fillText。Canvas计数明确统计可见文字同步/绘制，Qt可能的初始化/空清理由整体Qt同步/CPU提交区间承担。窗口更换断开旧visibleChanged连接，show/restore重发当前状态，避免只检查item可见性而漏掉窗口恢复。分组成本加入GUI准备及Canvas同步/绘制差值、每组120条事件原始序列与隐藏文字绘制为零的门禁。
- 验证边界：修复前D3D11/DPR1文字与页眉通过，但成本被隐藏Canvas污染；DPR1.5有明确像素失败。仅GUI守卫版本在DPR1留下失败成本计数，DPR1.5/2及NativeRendering试验出现退出7且无最终report，失败和原始图保存在`verification/windows/title-reels/r16b2/`，未算作通过。生产CurveRendering不变；窗口恢复、字体栅格与高DPR完整验证仍依R16B2V/W，不能把未完成试验当作选型证据。
- 诊断限制与避免再犯：高DPR文字比较遍历物理像素，工具整体完成预算按DPR²扩展；这只改变验证驱动的超时，不改变像素、运动或性能门槛。现有超时尚未证实是CPU比较还是恢复/readback停滞，不能归因于GPU慢或宣称可见性修复已解决所有高DPR问题。超时另写`timeout.json`记录step、warmFrames、请求/在途读回、drain和窗口策略；后续先定位阶段与资源所有者，再修改产品渲染，所有候选的隐藏工作都须独立检查。

### R16B2V：混合回退字体抬高普通字形基线

- 表现与确认根因：20项结构/文本一致时，emoji混排的case10/11普通MiSans字形仍出现带外差异。原布局按主字体ascent计算top，却让QTextLine以混合字体的较大ascent排glyph；28px输入的原报告普通glyph基线为31.078125；修正后约30.109375，无emoji同字形为30.09375。混合行ascent增大后旧top仍减主字体ascent，导致普通字形基线向下移动。不能将结构行框通过视为绘制基线正确。
- 修正：主CSS行框和粗搜/细搜、字号、整行glyph ID/关联不变，line.setPosition减去实际line.ascent乘局部scale，普通glyph回到主字体基线。彩色Text保留原回退行基线，在colorGlyph描述符中补回line.ascent与主字体ascent之差；不把普通字体的基线修正施加到彩色字体。单元回归比较有/无emoji前缀时普通字形ID和竖向位置，覆盖DPR1/1.5/2的布局输入；QTextLine使用QFixed坐标，setPosition在不同混合行ascent下可能相差一个1/64逻辑px步长，新回归按该已确认量化步长检查，仍会拒绝原近1px的偏移，原像素/结构/运动门槛不变；这些单元输入不代表三个DPR均已像素验收。
- 验证边界：完整Windows D3D11/DPR1.5诊断与受影响DPR1回归由本轮入口核对；剩余普通字形采样差异依R16B2V-P。后续先区分primary/fallback度量和具体绘制路径，不拆字符重新shaping，不用换字号去追混合行ascent。

### R16B2V：恢复测试沿用最终超宽页眉窗口而停帧

- 表现与范围：采集完成60项三路径静态图、9组活动成本和18项页眉后，在第一恢复周期step88停止；warmFrames=1、requested=false、inFlight=false、drain=0、visible=true，窗口策略正常。CPU比较已经完成、没有读回在途，增加DPR²预算不能解决该停帧。原测试沿用最终3440×1440页眉窗口与root大小，只改变300/320px标题宽度。
- 修正与已验证区别：恢复场景先hide/release，将window/root明确恢复为标题宽度加2、高度152；sceneGraphInvalidated后排队showNormal，保持置底/拒绝焦点，再请求正常Qt帧。单独等待invalidation的试验仍在同阶段超时；重设标题场景尺寸后多个独立完整试验均完成十次恢复/最终释放。已确认驱动场景尺寸不一致；Qt/compositor内部为什么停止后续frameSwapped尚未确认，不能宣称GPU驱动故障或归因于CPU耗时。timeout报告补exposed/restorePending，采集按正常事件与一个有界在途读回驱动，不加轮询/抢焦点。

### R16B2V：局部栅格改善不能替代完整文字矩阵

- NativeRendering在20px普通字形的局部图更接近源，但分数拟合/译名字号仍失败；原生透明层试验出现彩色子像素边缘，显式GrayAntialiasing去除彩边后仍未满足全部门槛。重设分数QRawFont字号/补偿transform、将层改为实际DPR也没有全通过。统一按DPR取整度量使case6/7带外SSIM从约0.976下降到约0.941，不能将其作为普遍CSS度量规则。
- CurveRendering四倍离屏层让case6/7、14/15改善，却使44px标题下19.8px译名的case16/17失败；分数度量取整、重设Curve字号与mipmap面积滤波也未全通过。原因仍需实际层/最终输出亚像素相位与源字体度量诊断；不能据某个局部通过保留新生产路径。所有这些试验已撤回，失败报告/成对图在本地r16b2v目录保留；不把采样门槛降低、1px带扩大或新增逐case偏移作为解决方案。
### R16M1：macOS窗口激活、遮挡与离屏驱动

- 表现与确认边界：仅设置Qt置底/拒绝焦点flags时，测试窗口仍干扰用户工作。Qt Cocoa的windowLevel不处理WindowStaysOnBottomHint，启动委托可主动激活进程。原生低层级后，窗口完全遮挡时isExposed=false，Canvas异步读回没有完成；静态grabWindow曾仍可得到自己的内容，因此不能从缺少frameSwapped推断应用被系统终止。Qt/compositor各自的停帧内部原因尚未确认。
- 处理：测试在QGuiApplication之前关闭Qt foreground transform/启动激活，隐藏原生窗口使用Prohibited应用策略、NSNormalWindowLevel-1、拒绝key与忽略系统鼠标输入。NSView.window/NSWindow.level同步观察在Qt重设/重建时保持层级，销毁surface前撤销观察。Accessory试验虽保留低层级，却检测到applicationActive=true及前台变化，已撤回，不能作为安全显示方案。当时将macOS可见窗口测试暂缓到最后R34；当前按动态能力规则判断，尚未建立可靠入口的对应CTest继续暂缓，不能写成已运行通过。
- 离屏修正：Review用QQuickRenderControl与真实Metal RGBA8+depth/stencil单采样目标，不调用macOS原生show；有界事件请求驱动sync/render和自身texture读回，业务仍使用原FrameClock。离屏目标必须显式设置devicePixelRatio，否则DPR2图像尺寸虽正确，文字/背景却只占左上四分之一，像素失败来自验证投影而非字号。hidden窗口下TitleReel/Header/Transport与Canvas探针需要显式offscreenRendering消费状态，生产默认false；漏接Transport会让全部页眉退出期宽度不保留。修正后18项页眉通过，十次离屏恢复/最终释放和原生隐藏窗口策略通过。
- 验证限制：离屏frame完成事件不叫present，GUI线程sync/render局部CPU与offscreen endFrame等待分开；不宣称正常VSYNC、跨线程渲染或产品性能。R16M1曾有Metal/DPR2两项emoji几何和八项像素失败；字体身份修正见下节，剩余六项像素收口依R16M2P，门槛不变。所有失败/原图保持本地忽略；后续先核对目标DPR、完整隐藏状态依赖和真实字体，再研究产品字形采样。

### R16M2：macOS默认emoji回退选中不同的系统字体面

- 表现与确认根因：同机冻结源码在28px、letter-spacing=-0.6px下，浏览器的四个彩色grapheme使用`Apple Color Emoji`，CDP实际PostScript身份为`AppleColorEmoji`；Qt整行glyph run却为`.Apple Color Emoji UI`，advance约31.40625而源约27.40625逻辑px，后续普通字形累积偏移16.078125。两个系统字体面的选择差异已由实际run与CDP证据确认；Qt/CoreText为什么默认选择私有UI面尚未追踪，不能归因于字号、DPR投影或字体资产损坏。私有family别名未必能通过QRawFont::fromFont重新取得，旧报告中用描述符重新查询的metrics不应冒充原始run的字体表证据。
- 处理：`initializeAssetFonts`完成四个MiSans物理字形验证后，仅macOS通过公开`QFontDatabase::setApplicationEmojiFontFamilies`一次性指定系统`Apple Color Emoji`；缺少该family显式失败并回滚原字体注册。设置早于控制器/QML创建，使整行QTextLayout与彩色Qt Quick Text使用同一策略。保留粗搜/细搜、请求字号、整行glyph ID/position、普通字形CSS基线、grapheme关联和两倍DPR层；不按case挪位置、不拆字符shaping、不改其他平台的fallback。CDP导出实际字面/family/PostScript身份并及时释放远程对象，原生诊断保留测量run的head hash与glyph ID/position，后续先对齐实际字体身份再研究栅格。
- 验证范围：arm64 Release/Metal/实际DPR2真正隐藏RenderControl，case10/11最大结构误差降至0.078125逻辑px，固定1物理px带外SSIM为0.999285581/0.999253834，原始ROI SSIM为0.962329861/0.943559874，不能混用两种统计。20项结构/文本、18项页眉、九组活动/缓存、隐藏Canvas零文字绘制、十次恢复/最终释放通过。字体身份/advance回归覆盖12、28、29.75、44px与DPR1/1.5/2输入，修复前失败、修复后通过；这不表示全部DPR像素已验收。case6/7、14/15、16/17仍失败，具体采样收口依R16M2P；普通CurveRendering和层采样未改变，当前缺少可靠Windows环境的后端补修和原实机要求保留到R34。离屏CPU诊断不代表present/VSYNC或产品性能；所有原图/报告保持本地忽略，重建与静态审计入口见`docs/contracts/title-reels.md`。

### R16M2P：字体框顶部定位与半行距小数

- 表现与已确认根因：同机Metal/DPR2完整基线复现case6/7、14/15、16/17六项带外像素失败，整行字形、字号和结构已通过。冻结生产字面读取的Range顶部相对face为整逻辑px，Canvas的fontBoundingBoxAscent/Descent与Qt主字体缩放后取整度量一致；原生却保留CSS半行距的小数。例如20px/28px行框，源顶部0、ascent21，原生baseline21.5；44px/61.6px行框，源顶部1、ascent46，原生约47.796875。问题是主字体baseline没有匹配源字体框定位，不能仅因结构≤1px就认定绘制正确，也不能继续把垂直位移归因于CurveRendering或层过滤。Chromium内部具体在哪个绘制阶段取整尚未追踪；这里依据真实生产字面几何和字体度量，不把推断写成浏览器内部实现事实。
- 处理与避免再犯：导出器补实际CSS font/lineHeight、Range顶部/高度和Canvas字体ascent/descent，仍保留PostScript身份；不改冻结布局/样式。macOS在主字体CSS基线计算中对半行距使用floor，单位为逻辑CSS px，保留分数行高、请求字号、粗搜/细搜、整行glyph ID/位置关联、混合字体ascent补偿、彩色Text与两倍DPR层。Windows/Linux预处理分支不变，不外推该规则到未经验证的字体后端。先用源字体框核对实际基线，再研究采样；不能以DPR取整整行度量、换字号或逐case偏移追截图。
- 验证与限制：原生Release/Metal/实际DPR2的正常入口不带diagnostics，20项文字全部通过；带外SSIM最低0.996912214、RGB最高0.264246/255，原始ROI最低SSIM0.869280017、RGB最高10.617510/255，固定1物理px带未扩大。逐个普通字面的实际baseline与同机源字体框在Qt的1/64逻辑px量化步长内一致；18项页眉、三路径各三次120帧、隐藏Canvas零文字绘制、十次离屏恢复/最终释放通过。基线单元输入覆盖12.6/19.8/20/24.609375/28/29.75/44px和DPR1/1.5/2，旧计算实际失败、修正后通过；这不是DPR1/1.5像素验收，完整矩阵仍依R16M3。全部23项启用Release CTest、格式、两组qmllint和qsb变体通过；受限环境CTest曾因无屏幕中止，原生权限完整执行通过，不修改测试/Qt平台插件绕过。四项可见runtime仍归R34；离屏成本不代表present/VSYNC或产品性能。复现及严格审计入口见`docs/contracts/title-reels.md`，原失败和成对图只在忽略目录保留。

### R16M3：macOS OpenGL 的旧默认上下文与绑定重入

- OpenGL表现与定位：同机隐藏RenderControl的DPR1入口收到SIGSEGV；日志反复请求GLSL120，但Kit的Canvas shader只有GLSL130/150/ES300等变体，图形pipeline创建失败。系统崩溃报告落在Canvas绘制提交链；已确认旧默认上下文与shader不兼容，崩溃内部具体解引用原因未追踪，不把它归为字体像素或环境不可执行。
- OpenGL处理：共用`selectGraphicsBackend`只在macOS显式OpenGL分支、QGuiApplication创建前，通过QSurfaceFormat请求OpenGL4.1/CoreProfile；Metal默认分支及其他系统不变。不能补一个GLSL120近似shader或降低Qt版本绕过。原失败日志保留在`verification/macos/title-reels/r16m3/opengl-dpr1/`；窗口和正常VSYNC仍不能从隐藏离屏成功推断，其他系统OpenGL能力也须分别验证。
- 绑定表现与根因边界：完整文字/页眉报告曾通过，日志却出现TransportTitle.opacity与caption.visible绑定环。caption从容器宽度/透明度推导visible，active=false时TitleReel同步finish并发出changed，自然宽度也共用每帧changed，状态通知回到布局/可见性求值链。只把自然宽度通知拆开仍有两个绑定警告；caption改用有标题或退出动画未结束的状态后，opacity警告消失但visible仍重入。隐藏完成改为有界排队后全部警告消失；这些分步日志保留，不能把第一项改动单独描述为已解决整个环。
- 处理与避免再犯：naturalWidth只有实际测量结果变化才通知；caption退出仍保留完整字面、容器原宽度/opacity/offset曲线和禁用条件。hide立即停止advance，仅一个对象所属的queued finish待处理，restore在交付前取消其完成行为，销毁自动取消回调；不另加时钟或轮询。单元检查真实宽度通知、十次合并hide、恢复取消和最终静止；完整验证入口额外拒绝绑定环、缺GLSL及pipeline失败日志。像素、源码轨迹和资源验收仍独立执行，不能只靠日志没有警告宣称视觉等价。
- 资源估算陷阱：OffscreenSurface原按RGBA8+深度模板合计8bytes/物理像素记账，未覆盖8bytes的D32FS8深度模板格式。现采用RGBA8的4bytes加深度模板8bytes上界，共12bytes，并保留resize旧/新计划重叠；字段明确标记保守估算和实际driver allocation未知。Metal的memoryless附件不据此声称系统显存分配量；不改变真实附件、sampleCount或画质。最终六上下文重跑资源与释放门禁，历史8bytes结果保留但不当作完整上界。
- 验证与限制：修正后的Release/arm64/真正隐藏Metal及macOS OpenGL，实际DPR1/1.5/2六个完整正常矩阵通过，无绑定环或pipeline失败；每项20文字/18页眉/三路径各三次120帧/十次恢复及最终释放。生产带外SSIM最低0.996912214，原固定1物理px带不变；23项启用CTest、格式、两组qmllint与文本qsb变体通过。四项可见runtime仍disabled；Windows/Linux共享隐藏通知回归及可见/present/VSYNC仍依R34，离屏成本不代表正常窗口或GPU性能。

### R17：ListView 跨模型类型复用、header 起点与委托计数

- 表现与定位：同一ListView启用reuseItems，歌单使用带required角色的QAbstractItemModel委托，切介绍时同时改为QVariantList制作人员与另一个委托。隐藏Metal已写出18项静态图后在切tab发生SIGSEGV，macOS栈落在QtQmlModels/QMetaProperty::read，经QQuickItemView::setModel进入；这证明故障发生在模型/角色交接，Qt内部哪个缓存指针失效尚未追踪，不能归因于Metal渲染或字体。
- 处理与避免再犯：TrackList的Loader分别创建曲目/制作人员两个ListView组件，每个实例保持固定模型类型和delegate角色契约，保留可见/复用池与100px预留，不通过关闭虚拟化回避。Loader返回值显式转为ListView，使官方Yalahoo_qmllint/ui-audit_qmllint targets能验证forceLayout/定位接口；运行时warnings另查，不自行组装qmllint命令。
- 布局与计数陷阱：有header的ListView起始contentY不一定为0；强设0让header位于可视区上方，锚点/标题字符串检查仍可能通过。改为布局完成后positionViewAtBeginning，并验收实际title的场景Y在内容顶端60px内。QObject::findChildren曾报告0个委托，但自身PNG已经绘出曲目；计数改走QQuickItem::childItems实际视觉树，不能把QObject父子树当成委托可见池。
- 验证边界：相同Release/arm64隐藏Metal、实际DPR1入口的九尺寸双主题、10,000曲目跳转、介绍切换和快速重开通过，实际可见/复用池31个、QML warnings=0；失败栈/原图与正常报告只保留本地。十次离屏重建与完整Release回归由presentation-qml/CTest复现；离屏不证明真实焦点恢复、窗口呈现或其他系统。同步运动由DetailPorts接入；后续真实设备与目标平台集成按R34，回放边界见docs/contracts/detail-motion.md。

### R18：场景状态与封面 polish 的交接不能跨帧混读

- 表现与确认范围：真实 Metal/threaded 产品回放执行减少动态和连续三十次换片后，GUI 的选中/归位实体已只有一个，但 frameSwapped 的排队回调仍可能读取上一轮 polish 保留的两个 CPU 封面租约；第一份报告的 stress-resource-return 明确失败。后续同一次回放的下一帧封面已回落到一个，未发现资源持续增长。这里确认的是采集观察了不同交接阶段，不能把前台帧事件当作所有 GUI/资源状态的共同 revision，也不能用这个失败样本宣称产品泄漏。
- 处理：产品 Review 的 ready 同时核对当前实体的 objectId/albumId 集合、CPU 封面租约的同一集合及选中 1024px 资源，再检查回落；不延时猜测、不把上界从一改成二。实际绘制仍通过原不可变 SceneFrame/上传 lease 接入，运动不为采集而暂停。保留首份失败及后续报告在 verification/macos/detail/r18/；固定窗口下目标/后处理 generation、每批上传、真实上下文重建与完整渲染结果分别验收。
- 复现与避免再犯：使用 tools/build/verify-detail.py 的正常产品入口及减少动态三十次序列；校验身份而非只比数量。renderer 的回调可跨引擎清理阶段，跨线程元数据使用共享原子值且退出前解除连接；不让直接 render 回调引用 Review 栈变量。该生命周期修正来自静态审计，不能写成曾实际发生崩溃。实际采样范围与尚欠目标后端/设备按 docs/contracts/detail-motion.md 和 R34 保留。

### R18：OpenGL basic 的 surface 恢复与完整窗口重建

- 表现与定位：macOS/OpenGL/basic 在减少动态、三十次切片后的第一次 hide/release/destroy/create/show 中，窗口仍 exposed/onScreen 且 frameSwapped 持续，但 SceneItem 的 frameSubmitted 停在重建前；120秒报告明确失败，sceneGraphInitialized 只有一次。未见 shader/renderError，不能把它写成平台无法验证，也不能从窗口帧推断三维仍在更新。
- 已确认语义：所选 Kit 的 QQuickWindow 文档明确 setPersistentGraphics/setPersistentSceneGraph 是可忽略的 hint，basic 为多个窗口共享设备/上下文，surface 重建不保证产生新图形上下文；只有删除最后一个 QQuickWindow 才保证释放图形资源。最初探针把十次 surface 重建等同于十次 context 初始化，属于验收入口假设错误，不能强迫产品每次恢复销毁可复用设备。另一方面，原 SceneItem 恢复只排 polish，静止/减少动态场景没有重新推动三维提交；Qt 内部 dirty/polish 状态在哪一步保留尚未追踪，不把推断当作已确认内部根因。
- 修正：SceneItem 的 visible 恢复事件排一个对象所属回调，核对仍可见后 ensurePolished 并 update，重新提交当前不可变 SceneFrame/cover lease，不加持续轮询、不改变画质或资产。修正后十次 surface 恢复实际恢复了三维帧；basic 仍共享一个上下文，这个观测保留，不伪报十一次。Review 另做十次完整 App/QML 根窗口销毁和重建，业务控制器继续存活，重新绑定输入路由；旧窗口排队回调按 windowEpoch 丢弃，回调上下文随 Review 销毁，渲染线程元数据仅持有共享原子值。新窗口在首次 show 前落实 VerificationWindow 策略。
- 验证与复现：`python3 tools/build/verify-detail.py --backend opengl --dpr 1 --runs 1 --foreground --output 全新目录` 的完整窗口入口实际通过，十次 surface 恢复加十次完整根窗口重建观察到十一次上下文初始化，重建后的详情换片也通过；首份停帧、只修恢复但旧 context 假设仍失败的报告，以及中断的旧二进制尝试均保留在 verification/macos/detail/r18/，后者不计入通过。其他后端/DPR与串行三次结论按 detail-motion.md 的实际范围记录。后续不能只等待 sceneGraphInvalidated 或 frameSwapped；分别验证自身三维提交、真实对象重建、资源/图像与窗口策略。

### R18：减少动态必须清除旋转副本的角度与高度等待

- 原有阵列GPU回归在returning-night之后报告Returning covers did not release；新增reducedRotatedSnapshotReturnCompletesImmediately在Release中复现：三个归位副本里仍剩一个旋转副本。冻结returnStep的reduced分支只是rate=35的指数衰减，dt=0不会改变角度，不能把它当作立即完成。只取消returnY等待仍留下非零yaw，回收条件依旧不满足。
- 归位副本在减少动态下直接yaw=0、清除returnY、lift.finish(0)，同次发布回收；普通运动仍保留源码returnStep以及对齐帧的高度，不提前下降。共享PointerDragController对正在查看的选中盒仍按源码保留手动目标角，不能把归位副本规则错误施加到它。新增单元修正前失败、修正后通过，Metal原有阵列GPU回归实际通过；其他范围按detail-motion.md记录。后续减少动态审计要区分解析衰减与精确终点，不能用“rate很大”替代完成与资源释放。

### R18：副本材质状态不能用全局详情进度替代

- 源码审计确认：原生产SceneItem把adapter.detail作为全局clarity，Renderer的材质模板将它用于所有实体。冻结scene/decryption实际为选中盒的独立揭示状态，归位副本捕获自身clarity并按exp(-9*dt)回雾，阵列保持磨砂；直接换片时全局详情构图不变，全局clarity因而无法代表这些状态。此项来自调用链审计，不声称此前做过失败的同机像素对照。
- 修正：CaseClarity移植六个REVEAL节点及单调Hermite、ready门禁、34.12–39.56秒参考时间和1.5倍率；静态导数编译期缓存。ReturningCase独立保存并回收clarity，SceneFrame交付不可变实体值；生产阵列模板clarity为0，选中/归位主盖仅覆盖原uniform的粗糙度。lidRoughness与既有ShellMaterial共用同一公式和R07校准，不改shader布局、光学参数或重建材质/图集/PSO。固定状态Review保留原全局输入以维持既有oracle，不能把它当生产时序。
- 验证边界：冻结decryption执行生成的1200个1/120秒样本覆盖进入、离开、回收、已清晰重选、改速和减少动态，最大误差约1.33e-14；独立副本/反向回收和既有210项材质参数实际通过。实际后端回放在清晰后开始拖动和换片，保留实体clarity序列；完整三维源码与实际质量检查仍按R29/MAC-VISIBLE-3D执行；历史图像比较不再执行，不把数值验证写成完整产品质量通过。后续不能把相机放置进度、材质揭示和封面CPU/GPU就绪合并成一个ready值。

### 可见验证：原生策略、OpenGL drawable/MSAA 与采集生命周期

- 授权与范围：用户明确要求可见验证即授权指定轮次前置/激活/焦点；`YALAHOO_VERIFY_FOREGROUND=1` 只作用于显式启动的验证进程，普通播放器不调用验证 helper。报告必须区分授权前台与默认置底策略；结束恢复可见 CTest OFF 并真实回归隐藏策略。Prohibited/Accessory 不可靠方案不能冒充可见通过；原生 visible、WindowServer onScreen、occlusion、Qt exposed、实际帧事件分别记录，不能用首次 grab 得到图像推断持续呈现。
- Metal 静态读回：Review 只在提交读回时安排一次 update；Metal 异步完成还需要后续真实帧，静止封面/阵列因此超时。每个 Pending 以 QRhi 的 MaxAsyncReadbackFrames+1 约束续帧，最多两个在途请求；不重复 submit、不轮询、不改效果。产品没有 captured 回调时不产生该工作。封面/真实列 GPU 已实际复现失败并在修复后通过；图像来源对照仍须独立验收。
- macOS 可见 OpenGL：当前系统的崩溃栈明确报告 NSOpenGLContext setView 必须在主线程，隐藏 RenderControl 不触发该 drawable 路径。macOS 显式 OpenGL 在应用构建前使用 Qt basic 循环与 4.1/CoreProfile；Metal 保持 threaded，不能混称线程隔离或把两者成本池化。当前 QRhi 报告多采样纹理可用，但创建调用 glTexStorage2DMultisample，而 macOS 4.1 上该入口不存在，生产浏览在 makeTarget 空函数地址崩溃。分配阶段核对实际入口，缺少时改用 QRhi 多采样 color renderbuffer，保持原采样数、RGBA16F/RGBA8 和 resolve，核对 backingFormat，并纳入实际资源清单；不降采样/精度，不锁 Kit 版本。窗口浏览已在适配后退出0；完整三维源码/后处理质量及长时测量仍按剩余专项收口，不再执行源图ROI比较。
- 详情与观测：header 高度在字体/宽度 polish 后改变，ListView 的原点随之变化，出现标题 Y=205.42 对目标210；准备期间保持顶部，header 变化合并一次排队定位，滚动、键盘索引、滚动条或显式跳转取消顶部保持，回调执行时再次检查取消状态。三路径页眉的 frameSwapped GUI 回调曾读到下一 tick 尚未 polish 的 Row，中间一帧按钮偏0.842px；测量改在 afterSynchronizing、GUI 被阻塞时取得已 polish 的值，不改生产按钮布局和门槛。保存按钮以 QPointer 跟随销毁，不让最终清理帧读悬空对象。
- basic 生命周期：native frameSwapped 回调排队到 GUI，避免在 basic render 调用栈重入状态发布。basic 的 hide/release 不保证发出所等待的 sceneGraphInvalidated；文字 Review 显式 destroy/create 实际 surface，保持外层业务对象。最终删除 QML 后先提交真实清理帧，让 Qt 在有效上下文执行排队 renderer 删除，再隐藏/销毁；不能把仍存活的 Canvas renderer 写成已归零。失败/超时和修复前后采集只保留本地 verification/macos/visible-window，正常入口保持全部结构、像素、隐藏工作与释放门禁。
- 几何探针与输入语义：geometryOnly原先直接按width×DPR申请目标，绕过RenderQuality的8294400像素保护；DPR1.5的3440×1440触发分配失败后renderer停止，后续944×900仅98个错误提示文字像素，其他错误画面竟因提示文字超过100而误通过。几何模式改为显式使用原生DPR请求但共用像素/尺寸保护，偏好值保持，limited状态与实际目标尺寸进入报告；模型检查排除顶/底提示区域并强制renderError为空，不降低100像素门槛。单元输入覆盖正常→超大受限→正常恢复，修正后的Metal可见实际DPR1/1.5/2完整几何矩阵也通过；正常与受限目标尺寸/limited状态分开记录，不称为全部尺寸都以未受限分辨率绘制。旧ArchivePool Review未接InputRouter，接入后边界键/滚轮按现行语义消费事件且不改变稳定ID；验收核对两者，不以event.accepted=false代替未越界。
- 三维来源重建：旧生成 wrapper 漏改 frozen-git 相对导入，浏览器又固定 ANGLE/D3D11；已适配路径和当前系统后端。macOS ANGLE/Metal 成功进入冻结 harness 后，Room PMREM 导出与提交的 rgba32f 精确字节仍不同，原因未确认。这是历史采集差异，不再要求Room GPU字节相等或浏览器witness，不再安排图像参照校准。禁止据此覆盖正确产品资产；后续按冻结源码核对环境算法、方向/精度、材质/后处理和真实质量，正式性能任务保留下面具体专项。


### 总览验证：macOS授权前台须在应用创建前选择激活策略

- 表现与条件：真实App总览验证窗口在Qt侧visible，但Cocoa侧hidden，首阶段没有有效呈现帧；独立可见窗口策略测试却能通过。两者启动调用顺序不同，不能据独立探针推断真实产品已显示，也不能把无帧当作总览算法失败。
- 已确认调用链与处理：`prepareNativeApplication()`原先在`QGuiApplication`之前一律选择Prohibited，仅在`prepareWindow()`中为授权验证切换Regular。授权分支现在在应用创建前直接选择Regular，并继续由统一窗口入口设置普通层级/允许激活；默认分支仍为Prohibited、置底且拒绝焦点。具体AppKit内部隐藏时序未追踪，不将其推测写为已确认根因。
- 验证边界与复现：`tools/build/verify-overview.py --foreground`使用真实App/SceneItem提交与原生窗口状态；独立`verification-window-visible`及默认`verification-window-policy`分别回归。子进程显式设置授权开关，不在后续会话继承；结束恢复`YALAHOO_MACOS_VISIBLE_TESTS=OFF`。自己的窗口读回不证明屏幕扫描时刻或完整产品性能。
- 外观检查不能由旧实机图反推定义：字体行框取原CSS结构和原生字体度量，背景模糊取源码设计，投影/运动取实际算法。锯齿、异常模糊或采集误差不是复刻目标，禁止逐像素比较及据图调参；新数值输入只由直接执行冻结代码生成。

### R19B：隐藏重建的资源代次与窗口策略诊断

- `ArchivePoolAudit` 接入真正 RenderControl 后，初次新断言要求重建前后 `resourceGeneration` 不同，有限列失败；该字段是每个 renderer 自身的目标重建计数，新的 renderer 也从 1 开始，不能当作跨 renderer 的全局身份。检查改为观察 `sceneGraphInvalidated` 的实际事件，并核对重建后的逻辑实体、上传排空及实际 cover fallback/lease；不以代次数值相等推断没有释放或使用图像分数验收。回调计数的寿命覆盖 window/surface 销毁，原生 threaded 分支使用原子计数。
- 同一次隐藏完整回归的 filled 在 far-night 曾触发 windowPolicy 失败，旧日志没有具体 native 字段，根因未确认；不能推断为 GPU 故障、产品主动激活或用户切前台。入口补充失败策略 JSON，本地保留原失败；修正后的两个真实 Metal 隐藏入口通过，记录 native visible/active 均 false、foregroundUnchanged=true。后续遇到策略失败先保留具体原生状态、定位触发条件，不能提高窗口层级、显示窗口或绕过 helper。本轮结果不代表正常 VSYNC/物理输入，也不关闭 BLUR-01/FLICKER-01。

### R19C1：参数辅助函数通过不代表实际提交值正确

- 表现与根因：R10B的210组shellMaterial测试检查未补偿的transport，Renderer::uniformTemplate却继续乘R07 ROI拟合系数，R18逐实体lid又独立乘1.01；辅助函数与有效uniform不一致仍能全通过。QML和Review也把expected乘同一系数，形成自洽而无源码依据的验证。不能用参数golden通过证明实际材质提交正确。
- 处理：从冻结configure/appearance/theme/clarity完整模块执行45组连续参数，核对真正加载的QML与C++；删除QML/RHI/逐实体lid的ROI乘数，Review拒绝factors/calibration并直接核对源值。显式capture按draw.offset读取真正上传的uniformScratch，逐可见壳体检查；只在采集生成有界JSON，不增加生产逐帧读取或资源。
- 验证与限制：本机Release/Metal隐藏完整回归及实际uniform通过，旧标定入口无写入失败、产品两类标定输入在show前退出12。该结论不证明BRDF/LOD/透明排序、连续闪烁或小大目标清晰度，R19C2/D/H仍须按各自边界收口，不能用静态图或零warnings关闭用户问题。后续检查任何参数迁移必须同时追到最终upload/调用override，不能只测辅助值。

### R19C2A：离屏Quick3D失效与完整场景重建

- 表现与触发：新MaterialReview的源参数、99个材质/纸面和48个灯光状态能实际绘制；在同一View3D上直接调用RenderControl invalidate/initialize后，只剩背景，foreground=0。补`window.releaseResources()`、将同一item从contentItem脱开后重新挂接的两个尝试仍为空白，不能把真实invalidation或零QML warnings当成恢复成功。
- 处理与确认边界：独立Review执行完整场景重建：先删除拥有Quick3D场景的QML root，再释放离屏上下文/窗口资源，重新创建同一GeometryScene并应用相同源状态。实际输出恢复；材质十次、灯光一次均有真实sceneGraphInvalidated与有效输出。报告区分`qmlRootRecreated=true`和scene graph重建，不宣称同一View3D对象恢复。已确认的是手动离屏复用旧root会漏画、完整root重建可恢复；Qt内部scene-manager失效原因尚未确认，不能写成已修复Qt缺陷。生产SceneItem/RHI不走这条Review QML路径。
- 避免再犯与限制：为新离屏组件建立生命周期入口时，同时核对真实失效、所有者重建和有效输出，不能只看事件或计数。遵守[Qt RenderControl](https://doc.qt.io/qt-6/qquickrendercontrol.html)的隐藏窗口、initialize/invalidate及帧边界；不得通过show/抢焦点修复采集。macOS隐藏完整root重建不证明原生窗口恢复、目标平台或正常VSYNC；对应R34保留。另一次初期灯光运行仅foregroundUnchanged=false，其余原生visible/active及activationObserved均false，具体前台变化原因未确认，失败日志保留，不归因于产品主动激活或用户操作。

##### R19C2C：多pass Equal深度必须同时保证计算顺序和位置不变性

- 已确认visibility先写depth而shell颜色Equal拒绝相同表面会形成随运动变化的漏画。原独立九实例漏14646点；只统一GPU运算仍漏17171点；只加invariant在简化输入通过，却在生产177可见实体仍漏1191点。不能用单盒、静态图或零warnings关闭大量重叠问题。
- 两个geometry顶点shader统一复用`p=model*vertex`再`nativeVP*p`并声明invariant；MSL1.2不支持该属性，只有这两个shader升到MSL2.1。删除无用CPU mvp乘法但保留ABI槽。实际诊断执行生产光学helper，检查visibility覆盖没有在Equal颜色pass丢失；正常动效/数量/透明度/深度阈值保持，未加偏置或epsilon。
- 代表双主题/大小视口的静止、波浪、浏览和总览进退960帧须同时有正常光学与逐pass有限值/覆盖数据。封面ready依实际帧事件等待，不能在异步解码前快速重复若干帧便判产品失败；相同源参数不保证跨pipeline编译的浮点位置相同。目标其他GPU、呈现/VSYNC仍需原专项，R19H核销时保留范围。

## R19C2B2：数值探针必须保留坐标域与量化域

- 源Three geometryRoughness取view-space normal的最大分量导数；world-space无穷范数在旋转后不等价。用已知normal梯度和相机roll0/45的独立解析测试直接执行生产helper，不能用正视静态图掩盖该差异。LTC零切线/相反light-view单独检查有限奇点，不能未定位就声称其为阵列闪烁根因。
- 新alpha探针曾将连续期望界限与RGBA8整数码值比较，1×出现48点假黑边；应按原整数码值门槛qRound(expected×255)-1比较。修正测试量化域后1×/4×黑边均0，原一代码值容差、产品shader和采样未改；保留失败与修复日志，不扩大门槛。
- shader生成须覆盖basic/early全部变体并验证生成+格式化一致；Review数学probe复用生产helper、以现有float读回在RGBA8转换前取值，避免显示量化掩盖公式误差。probe只编入Review，不能变成生产逐帧诊断或新增目标重建。

## R19C2B1：采集语义字段不等于独立GPU资源

- 原RhiItem为13个Capture字段逐项提交readback并调用imageOf，fallback使多个字段指向同一纹理；不透明且captureAtlas=false时12个字段实际只有color/normal/depth/shadow四张纹理。继续按字段计remaining会把去重后的批次永久挂起；因此必须同时按真实纹理身份分组、只计算实际异步回调数，并在最后一组完成后一次deliver。跳过atlas不是一个待完成回调。
- 实现以每批最多13项指针表建立owner索引，完成后只转换一次、向各语义字段赋同一不可变QImage值；Qt隐式共享由constBits一致实际核对，不深复制。GPU资源仍归render，GUI只得到值；最多2批及backend读回帧上限不变。Metal实际四资源/十二字段、resize/alias、30次连续状态和十次真实重建及完整隐藏回归通过；目标D3D/GL另见R34，不能从本机外推。
- 生成器陷阱：旧prepare-rhi-shaders.ps1从共享GLSL重建时不包含后续CPU原点场提升，会静默恢复逐片元重复计算。当前转交prepare-rhi-paper.py，明确保留entityLight0.xy；--check核对完整适配后的token，实际生成再format能逐字复现当前shader。以后修改生成脚本必须同时验证生成结果和提升依赖，不能仅证明脚本可解析或旧图分数未变。

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
10. Windows、macOS、Linux 均须完整实现。当前主力开发与验证环境为macOS；每轮按实际可用的系统、Kit、后端、设备、采集入口及桌面策略动态决定验证方式，不固定某个平台必须实机或只能静态。当前能可靠执行的构建/功能/渲染检查必须真实执行；不可执行或不可可靠验证的部分暂以严格静态审计交接，并把必需实机任务保留到最后R34。移动端不属于本计划。
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

- QML 静态诊断统一构建 Qt/CMake 自动生成的 `Yalahoo_qmllint`、`ui-audit_qmllint` 等对应 target；禁止自行组装命令行或直接调用 qmllint 程序。运行时 warnings/日志检查另行保留，不能替代官方 target。

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

- 历史Windows开发机定位参考：Qt 安装根 `C:/Qt/`，已安装版本包括 6.11.1、6.12.0，目前最新 Kit 为 `C:/Qt/6.12.0/msvc2022_64`，qmake 报告 6.12.0；VS C++ 工具路径为 `C:/Program Files/Microsoft Visual Studio/18/Community`。这些值仅供 agent 定位，执行时重新枚举安装并选最新版本，升级 Qt 不要求修改仓库中的版本常量或路径。以 Kit 配套 ABI 和实际编译探测为准，不因为目录名 msvc2022 就切换到不存在的 VS 安装。
- 当前主力macOS/Apple Silicon开发机定位参考：用户安装的 Qt Kit 为 `/Users/zhaoyuhang/Qt/6.12.0/macos`，安装根为 `$HOME/Qt`，已纳入默认自动发现。此路径只供后续 session 定位，不是版本要求；每次配置仍重新枚举最新原生 Kit。现有 LLVM 的 clang-format 位于 `/opt/homebrew/opt/llvm/bin/clang-format`，不在 PATH 时通过本机 cache 变量 `YALAHOO_CLANG_FORMAT` 指定，不写入公共 presets。
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
- 所有当前及后续 session 的测试、GUI/性能采集和证据重建强制遵守 `docs/contracts/test-window-policy.md`：无需真正显示的窗口保持隐藏；默认需要显示的 Qt 窗口在首次 show 前统一设置 `Qt::WindowStaysOnBottomHint`、`Qt::WindowDoesNotAcceptFocus`，恢复/重建后保留，禁止置顶、raise/requestActivate 或抢焦点。用户明确要求可见验证的指定轮次按下条授权例外执行。普通产品窗口不受测试策略影响。浏览器使用 headless，后台 helper 隐藏；采集自身渲染/读回，不采集被遮挡窗口所在的用户桌面。
- 当前macOS优先以真正Qt离屏渲染验证画面、状态和资源生命周期，已有arm64 Release/Metal运行结果按实测范围记录。可见窗口、正常VSYNC、真实输入/呈现、媒体设备或部署检查能否可靠执行逐项判断；不可靠的部分先严格审计并保留到R34。离屏CPU成本不代表present、VSYNC或产品render-thread性能。现有`YALAHOO_MACOS_VISIBLE_TESTS=OFF`是默认非干扰策略；用户明确要求可见窗口验证即授权指定轮次前置/激活/焦点，无需再次确认，可显式启用`YALAHOO_VERIFY_FOREGROUND=1`和可见CTest。报告区分授权前台与置底策略，结束恢复默认并回归隐藏策略；未运行的disabled明确记为暂缓。换平台或验证入口可用性改变后重新评估，不沿用Windows必需实机/macOS仅静态的旧固定分工。
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
    platforms/              各平台构建/部署说明与实机验证清单
    media/                  非私人视觉基线与成对对照图
  verification/
    windows/                Windows 实际运行证据（可用时生成）
    macos/                  macOS 实际运行证据（按覆盖范围记录）
    linux/                  Linux 实际运行证据（可用时生成）
    windows-static/         Windows 严格静态审计矩阵与证据
    macos-static/           macOS 严格静态审计矩阵与证据（不代替已有实测）
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
- 两种 API 的共用绘制契约见 `docs/contracts/canvas-painter.md`；生产滑尺刻线/路径使用 C++ Canvas Painter，固定十六槽、完整刻线的 Qt Quick 裁切、数字轨迹、统一输入路由及资源/失效规则见 `docs/contracts/navigation.md`。R20 将 Canvas Painter 用于生产二维序章的原创品牌轮廓/圆环。R16 比较文字绘制路径后确定字轮实现，不因已引入绘制模块就强制替换全部文字。R16M3当前平台阶段生产字轮采用整行glyph＋彩色Qt Quick Text、CurveRendering及两倍DPR槽层，macOS Metal/OpenGL三个DPR已按同机源参照验收；完整比较、缓存、通知/隐藏规则及成本边界见`docs/contracts/title-reels.md`。其他目标后端补修和最终跨平台选型依R34。普通控件/输入/焦点/Accessible 与列表虚拟化继续由 Qt Quick 承担，三维 SceneItem 的 RHI 管线按既有契约执行。
- 同组件的 C++/Canvas2D 及适用的 Qt Quick Text/Shape/ShaderEffect 对照使用同一内容、字体、路径、主题、逻辑/物理尺寸、DPR、时刻和抗锯齿目标；先满足源码定义、结构、文本、运动和实际渲染质量，再比较成本；禁止通过实机图逐像素匹配验收。不能用 CPU QPainter/旧 Canvas 的测试成绩代替与实际 Qt Quick GPU 路径的比较，也不直接转抄官方 benchmark 宣称本项目提速。采用的具体组件、API、比较范围与理由写入 R14 建立的 `docs/contracts/canvas-painter.md`，原始产物只留本地忽略目录。
- 验证描边宽度/端点/连接、曲线与路径缓存、透明背景/预乘及颜色空间、渐变/遮罩/裁切、文字基线/字重/字距/shaping/emoji 和 DPR 边缘。接口不支持的效果不得擅自近似：同组件可组合 Qt Quick 合成层保持等价，或依据明确对照保留更合适的原路径。R15/R20 的生产使用是具体交付；若目标组件存在可复现的不等价或结构/资源回归，须修复或保留针对该组件的具体补修任务，不能只完成演示探针就宣告接入完成。
- 绘制只消费已有控制器/不可变状态和唯一 FrameClock，使用已缩放 dt，不新增独立 Timer/FrameAnimation/requestAnimationFrame 时钟。静态状态按内容/布局 revision 触发重绘，隐藏/遮挡后停止无效 tick；缓存按路径/样式/字体/尺寸/DPR 等实际依赖失效，动画只更新必要参数，不逐帧重建不变路径、字体布局、图像或绘制目标。C++ 同步与 QML JavaScript 成本分别记录，禁止在绘制/同步热路径读盘、解码、等待或 GPU 读回。
- 展开Canvas Painter与Qt Quick的真实渲染/合成顺序、离屏附件、MSAA/resolve、纹理上传、缓存容量、资源所有者及hide/restore/resize/重建/退出路径，新增资源纳入ResourcePlan。按当前环境能力实际验证可用后端；D3D11、Metal、OpenGL、平台合成器和部署闭包不可执行的部分严格审计并移交R34。共享源码/qsb生成不代替目标设备运行，局部绘制成绩不代替完整产品验收。

### 连续运动

- 时间单位统一秒，核心计算 double。唯一 FrameClock 接收 QML FrameAnimation 的帧间隔：`dt=min(max(dt,0),0.05)`；普通动效 `motionDt=dt*speed`，速度只缩放一次。片头和音频不受 speed 影响。
- 临界阻尼原样移植：`d=x-target`，`j=v+r*d`，`e=exp(-r*dt)`；`x=target+(d+j*dt)*e`，`v=(v-r*j*dt)*e`。普通真实列 rate=3.7，详情换片 rate=9，总览 rate=6.5。
- 搜索/跨列远记忆/归中按现有五次多项式接续 x/v/a，终点 v=a=0；时长 `1.4+clamp((hypot(lanes,rows/8)-1)/7,0,1)` 秒。系数直接移植 `music-archive-motion.ts`、`music-real-columns.ts`，不能用 Qt SpringAnimation 默认参数替换。
- 同目标不重启轨迹；坐标 rebase 只平移位置与多项式常数项，时间、速度、加速度不变。减少动态效果直接完成目标并处理回调/焦点，不能只把 duration 设成 1ms。
- 运动内核、唯一 QML 帧源、时钟/轨迹 API、revision 完成语义及冻结数值对照见 `docs/contracts/motion.md`；Windows 证据在 `verification/windows/motion/`，新增 oracle 在 `tests/fixtures/motion/kernel-golden.json`，macOS/Linux 对应审计在各平台 `motion/` 子目录。后续视觉模块只消费同一 FrameClock 的 dt/motionDt，不重复缩放速度；实际列布局和开场界面分别由 R13/R20 接入。
- 共用 LayoutMetrics、SceneFrame/SceneAdapter、相机目标/阻尼、逻辑像素投影及包围体拾取接口见 `docs/contracts/scene.md`；几何与相机的 Windows Release 重建入口为 `tools/build/verify-r06.ps1`，macOS/Linux 新增调用链审计为 `tools/audit/audit-r06.ps1`。QML 只消费同一场景对象的相机参数与布局指标，后续阵列/抽取/总览扩展该接口，不建立第二套常数。
- 详情同步放置、同构图换片、有限归位池、实体竖轴拖动和独立材质揭示由DetailPorts/DetailMotion/ReturningCasePool/PointerDragController/CaseClarity接入；单帧顺序、1.45/1.05秒C2轨道、rate=9、80ms场景门禁、容量/回收、REVEAL曲线、不可变逐实体材质及复现/范围见`docs/contracts/detail-motion.md`。固定状态Review输入与生产请求接口分开，不用全局detail替代副本状态。
- 总览取景/折叠/标签与FrameClock顺序、rate=6.5、真实列共用progress、底边补偿、extent≤.12不可拾取、九候选/十八槽、两步进入及局部模糊资源边界见`docs/contracts/overview.md`。只投影可见物理邻域，复用view-projection和现有三维目标；普通功能与显式生命周期压力分开，目标平台欠验见R34/R19-P。
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
- 环境取基线 RoomEnvironment 的方向性与颜色，R01 从现有 Web 渲染路径导出无私人资源的环境输入/样本。Qt 内置 PBR 的强度单位不与 Three 直接等同，按冻结环境/BRDF算法、光强单位和颜色空间换算确认等价性，禁止按实机图ROI损失标定或搜索参数。

- 灯光、世界空间光带、独立列/rebase 跟随、650ms 连续主题及纸面/壳体共享世界原点函数见 `docs/contracts/lighting.md`，冻结源码/算法 hash 在 `lighting.json`。音乐生产环境/半球/方向/spot/area 使用原源单位的自定义 GLSL；不得与 Qt 内置灯重复叠加。LTC 资源与来源在 `resources/manifests/lighting.json`，Windows Release 复建为 `tools/build/verify-r08.ps1`，对应证据在 `verification/windows/lighting/`、`docs/media/lighting/`；平台新增审计在各静态目录 `lighting/`。R12 实例与 R18 抽取/归位继续消费同一实体 origin、绝对 row/lane 和 palette，不重建灯光或重新滤波整列平移。

### 首选管线与确定的补救路径

- SceneAdapter 的产品接口固定，不因渲染实现变化修改 QML/导航。生产使用 QQuickRhiItem + QRhi/qsb；Qt Quick 3D View3D/CustomMaterial/Effect 仅用于 Review 来源对照。图形私有接口集中在 RHI 边界与能力探测，不进入 QML/导航。
- 正常管线：阴影目标 → 不透明背景/纸面/结构与供透射采样的捕获 → 磨砂壳体合成 → SSAO合成 → 景深 → 按画质SMAA → 色调映射/输出；以冻结基线的实际通道顺序和例外为准。QML文字与阅读遮罩在三维合成之后，不参与景深。
- Qt 的 SCREEN_TEXTURE/SCREEN_MIP_TEXTURE 不包含透明材质和使用该屏幕采样的物体；不得假设它会递归捕获其他玻璃。先采用与当前 Web 一致的屏幕空间近似，不要求真实光线追踪。
- CustomMaterial 显式实现局部纸面 LOD、侧脊密度/高光、印刷光场和按主题混合；不只填 PrincipledMaterial 参数就宣布完成。Effect 移植基线 SSAO/Bokeh/SMAA 的有效算法及参数，避免 Qt 默认后处理改变画面。
- 线性渲染与 sRGB 输出分开，盒体跟随原 ACES 曝光；纸面 `toneMapped=false` 的路径单独核对。避免 View3D 与自定义 Effect 重复色调映射。
- 材质、光场与颜色按冻结shader、生产有效参数、光强单位、颜色空间和合成算法逐项移植；保持纸面、留白、侧脊、外框和背景的理论关系，在双主题/视角中检查实际质量。禁止用实机图ROI/RGB损失标定或搜索参数。
- 跨渲染API的光强或材质语义转换须由源码公式、单位与解析/数值测试推导，记录转换依据；不能扫描参数去追浏览器像素。IOR、几何、封面纹理、主题颜色和各项效果按生产代码保留；确有算法/能力缺口时按R10既定管线修复，不以图像残差新增补修轮次。
- 单盒材质的参数/线性捕获/局部 LOD/印刷限制、固定环境和重建命令以 `docs/contracts/materials.md` 为准，冻结源码与算法 hash 在 `materials.json`，资源清单在 `resources/manifests/materials.json`；同输入 fixture 为 `tests/fixtures/materials/single-case.json`。Windows 材质证据与残差在 `verification/windows/materials/`、成对图在 `docs/media/materials/`；平台矩阵在 `verification/macos-static/materials/` 和 `verification/linux-static/materials/`。后续灯光、图集与后处理接入保留纸面入射量、一次 ACES/一次 sRGB 输出和独立侧脊参数契约。
- 阴影/SSAO/RenderQuality 的有效参数、seed=417 核、RGB24 线性深度/独立 view normal 预通道、四预设及 8.3MP 限制见 `docs/contracts/occlusion.md`；资源来源/hash 在 `resources/manifests/occlusion.json`，同输入为 `tests/fixtures/occlusion/`，Release 复建为 `tools/build/verify-r09.ps1`。Windows 证据和成对图在 `verification/windows/occlusion/`、`docs/media/occlusion/`，平台新增审计在各静态目录 `occlusion/`；公开阴影投影/偏置/滤波、Unshaded 壳体接收/邻盒透射与背景输出的具体后续输入以 `docs/contracts/occlusion-limitations.json` 为准。画质只改变渲染资源，不重建业务状态；后续实例池须向两个几何预通道供应同一实体 origin/位置。
- 景深 41 点 Bokeh、SMAA 1x Medium/lookup、最终 ACES/sRGB 与三个固定公开 Effect 通道布局见 `docs/contracts/postprocess.md`；资源来源/hash 在 `resources/manifests/postprocess.json`，Release 复建为 `tools/build/verify-r09b.ps1`。Windows 九尺寸/双主题及真实 DPR 1/1.5/2 的 105 状态证据在 `verification/windows/postprocess/`、成对图和局部放大图在 `docs/media/postprocess/`；新增平台静态审计在各静态目录 `postprocess/`。原始画质开启实际几何 MSAA，高/极高追加 SMAA；纸面透射捕获边界及邻盒背景 ROI 的 R10 输入见 `docs/contracts/occlusion-limitations.json`，不能以 SSAA 诊断或整体模糊替代生产修复。
- 渲染目的后端固定为 QQuickRhiItem + QRhi/qsb，实施边界为 R10A–R10D；接口、固定参数、源捕获语义及各项补修输入/验收见 `docs/contracts/render-backend.json`、`render-backend.md`。Windows 70 状态证据在 `verification/windows/render-backend/`、成对图在 `docs/media/render-backend/`。生产唯一运行实现为 RHI SceneItem，`SceneAdapter.renderBackendContract` 的 implementationReady=true / activeImplementation=rhi；集成、纯值资源键/预载缓存、累计资源和输入/正常VSYNC帧入口按 `docs/contracts/rhi-integration.md` 执行。Quick3D oracle 只在 Review 构建打包，普通产品不链接 Quick3D 或打包第二套场景。R10 的历史图集 GPU 传输字节保持未知；当前 RHI 的实际 tile 目标传输字节以 `verification/windows/rhi-postprocess/` 为准，不把 CPU payload/调用次数当预算证据。不能直接删效果、自动降低默认画质或换成 WebEngine。
- RHI 不透明基础、不可变 SceneFrame 同步、阴影实际 PCF/Vogel 算法、std140/qsb 和目标/读回所有权以 `docs/contracts/rhi-opaque.md` 为准，代码在 `src/render/rhi/`；历史 R10A 六 shader 证据在 `verification/windows/rhi-opaque/`、`docs/media/rhi-opaque/`，历史重建使用对应验收提交的 `tools/build/verify-r10a.ps1`。R10B 七 shader 历史复建同样使用对应验收提交。当前源树由 `tools/build/verify-r10c.ps1` 复建 24 shader 并回归不透明基础、材质与后处理，历史证据 hash 不回写。单采样基础检查不代表产品替换或完整后处理，生产接入与测量边界以 `rhi-integration.md` 为准，基础 Review 不等于完整播放器。
- RHI 磨砂捕获、4× MSAA resolve/mips、局部纸面 LOD、显式三壳体材质、近到远不透明/远到近全局壳体稳定 ID 排序按 `docs/contracts/rhi-shell.md` 执行；当前 816-byte std140 保留 R10B 的 752-byte 前缀，新增边界按 `rhi-postprocess.md` 执行；源参数/hash 在 `resources/manifests/rhi-shell.json`。独立捕获不含透射壳体，独立合成目标禁止反馈；contain 可见边界由 centroid 几何 MSAA 抗锯齿，预乘滤波避免透明黑 texel 形成黑边。Windows 61 状态、原 58/59 邻盒通道及五 ROI 残差在 `verification/windows/rhi-shell/`、`docs/media/rhi-shell/`，对应静态矩阵在各平台 `rhi-shell/`。当前 61 pre-stage 与 50 final-output 同输入重放、五 ROI/轮廓和逐项残差在 `verification/windows/rhi-postprocess/`、`docs/media/rhi-postprocess/`，这些图像指标保留历史记录，不再重跑像素匹配；后续核对源码材质/合成与实际质量。
- RHI 图集 generation/lease/serial、256² tile 的 2MiB 稳态/16MiB 初始化队列、112-byte 几何实例、独立 AO/DOF 几何、RGBA16F 颜色、保持相同 half 精度的 R16F 标量AO、32/64 原始核及 Bokeh/SMAA/一次输出以 `docs/contracts/rhi-postprocess.md`、`resources/manifests/rhi-postprocess.json` 为准。Windows 实际 D3D11 destination box/row pitch 和已完成 GPU 整页读回证明单 tile 262144 bytes、稳定帧零、稳态批次 2097152 bytes、初始化批次 16777216 bytes；不是物理总线流量。432 实例的 120 秒未限速原生呈现与 CPU 提交分别记录，GPU timestamp 在验收运行中不可用；系统 compositor 和产品交互仍由 R10D 验证。平台静态审计见各 `rhi-postprocess/`，用户实机步骤见 `docs/platforms/`。
- 补救实现是 QQuickRhiItem + Qt 自带 QRhi/qsb：仍为 Qt Quick 原生应用。QRhi 使用所选最新 Kit 的 GuiPrivate，编译探测实际接口并将版本差异隔离在 `src/render/rhi/`；升级 Qt 后重新探测、适配并复验，不通过固定版本或退回旧 Kit 绕过私有接口变化。不引入第三方渲染引擎，不改 QML 业务接口，不让私有类型泄漏到 core/media。
- R10A–R10D 的触发已由实际对照固定；后续轮次按合同执行，不再讨论框架选择。保持源屏幕空间透射近似：Three 音乐壳体为 FrontSide，不透明捕获同样不含透射壳体，不能误增递归玻璃捕获；修复对象为源码及真实运行确认的 shadow/resolve/排序/深度写入错误和实际渲染缺陷，不含像素残差追分。

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
- 每份视觉产物携带基线提交、fixture hash、Qt/浏览器版本、图形后端、内容尺寸、DPR、画质、时间和 ROI；用于说明自身输出与采集范围，不进行截图逐像素比较。

### 尺寸、源码效果、运动与实际质量

- 各平台适用的内容尺寸为1920×1080、1280×720、995×858、960×900、930×900、390×844、320×720、2560×1080、3440×1440；代表性尺寸覆盖DPR1/1.5/2。当前可靠的结构、运动、功能与实际渲染质量检查必须真实执行；不可执行的目标环境严格审计并移交R34，不以其他平台结果替代。
- 布局按源码推导：主锚点误差≤1逻辑px、三维轮廓/投影锚点≤2逻辑px，核对分界、断行、裁切和点击范围。坐标来自代码/数学状态，禁止通过截图逐像素匹配确定位置。
- 几何、取景、颜色空间、光照、材质、后处理与时序以源码理论效果为准。真实输出检查缺失、错位、异常合成、锯齿和模糊；原项目实机图只供大致参考，不计算图像相似度、不生成像素差分、不设置图像匹配门槛。
- 抗锯齿以消除可见渲染缺陷、达到理论上平滑的轮廓为目标，不复制旧图锯齿或用整体模糊掩盖边缘。源码有意的景深/背景模糊保留；错误分辨率、缩放、采样、字体回退或采集引起的非预期模糊须定位修复。
- 原品牌改为`yalahOO!!!`，新SVG按R03B精度、缩放与主题要求验收；容器锚点、留白、布局、动画与相邻控件仍还原生产源码，不设置旧品牌图像排除区或像素比较。
- 运动数学：double轨迹误差≤1e-6，分别记录位置/速度/加速度；代码投影误差≤2逻辑px、时序事件差≤一个60Hz帧。覆盖20/30/60/120fps与长帧；每段按1/120秒采样状态，直接对照原算法执行结果，不以完成态代替过程。
- 打断序列包括立即返回、退字连选、详情三次换片/反向、搜索替换/取消、总览切列、改速、途中减少动态、扫描替换和跨1.05布局分界。功能、源码数学、资源和质量要求分别证明，不用图像分数代替。
- 不新增OpenCV/Pillow/scikit-image等第三方图像库；验证产物仍只在本地忽略目录保留。

### 性能与平台边界

- 按用户明确修订，R10D 集成边界的性能实测为记录项，20ms 保留为参考而不阻止本轮完成或提交；结构/算法、功能、视觉、上传、资源与生命周期要求仍须通过。保留全部超限和环境不确定样本，不宣称实际性能已达标。复建使用显式 record-only 模式，普通 Review 默认仍保留历史数值门槛；完整产品预算与噪声按 R28 建立。


- 按性能契约分开执行静态/算法审计、局部诊断和实际产品验收。保留Windows参考机60Hz、原始画质/1920×1080、预热后120秒混合交互p95≤20ms及稳定阶段不重复出现>50ms的目标；20ms是工程容差，不是稳定60fps证明。RHI关闭VSYNC的Review仅验收吞吐，不能替代正常VSYNC产品节奏和系统compositor。
- 完整产品增加p99≤两个目标刷新周期的尾部目标（60Hz为33.33ms），逐场景报告一/两/三周期长帧、连续卡顿、输入收到→状态提交→对应帧提交及可测漏帧/显示延迟；不能以混合总p95覆盖局部失败。缺少真实显示时刻时标明估算/输入到Qt提交，R10D建立测量入口，R28前交付实际基线、噪声与各预算依据。
- 原始画质两主题为主要目标，高/极高、大尺寸和DPR2另列压力结果及资源峰值，不能直接套用1080p门槛，也不能省略检查。冷启动、封面就绪、扫描交接、稳定、重载、恢复分开采集，固定片头时长单列；准备期仍检查输入响应/队列，禁止扩大准备期排除卡顿或无限推迟就绪。
- 正式场景串行采样至少三次，保存每次原始时间序列/分布，优化用同机同Kit同输入前后二进制交错对照；不挑最快值、不并行构建或GPU测试。临界失败仅进行一次固定三次复核批次，按有效性、重复性和具体事件归因，保留失败样本。调整预算必须有设计推导、全样本和体验影响证据，不能为过关调数字或无限重跑。
- GUI/同步、CPU提交、GPU完成、资源重建、上传、解码与帧率分开；CPU提交不得称为GPU耗时或与GPU分位相加。不可用GPU timestamp写null，frameSwapped不是屏幕扫描时间；窗口遮挡/节流导致无效采样时明确限制，继续遵守置底/不接受焦点策略。
- 连续10次界面重建和30次详情切换后，资源回落且无单调增长；隐藏/最小化暂停视觉更新，歌曲继续播放。
- 最终实机要求对所有适用平台保留：真实Release构建、Qt窗口交互、GPU视觉/资源/性能、QtMultimedia播放与设备变化、部署包脱离开发路径运行。当前可靠项目必须执行，其他项目按动态规则严格审计并排入R34；模拟媒体、CI配置、仅configure或其他平台截图不能代替这些最终检查。Debug仅在明确需要堆栈时使用。
- 各平台代码、CMake、QML/资源、shader和部署脚本必须完整实现；当前确实无法执行的目标构建、后端补修或部署运行可阶段性移交R34。严格审计须先修复能由源码确定的缺陷，不得把已在可靠本机检查中失败的产品功能改判为环境不可用。实机清单与R34待办保持一一对应，静态交接只解除当前主线对不可执行检查的等待，不构成最终产品实机验收。

### 所有平台的严格静态审计契约

- 每个平台形成独立 `audit-matrix.json`、`audit-report.md` 和用户实机验证清单。矩阵逐项包含 checkId、requirement、platform、sourcePath、lineOrSymbol、evidence、result、limitations；每条结论必须指向实际代码分支、CMake 条件、资源清单、脚本行或所选最新 Qt 版本的官方 API 依据。审计同时核对 C++23、Qt 自动选版、无固定版本路径及完整平台分支。只写“跨平台 Qt 已支持”、只列文件名或只靠字符串搜索不算证据。
- 审计工具放 `tools/audit/`，使用 C++/Qt 或系统现有脚本能力，输入为当前源码与规则清单，输出机器可读覆盖/发现报告；自动检查之后逐项人工推演实际调用链、平台预处理分支、资源解析与错误路径。审计修复须真实回归当前可用平台的共享路径；不可执行的其他平台回归列入R34，不能宣称已通过。
- **COMMON-01 构建闭包：** 从顶层 CMake 展开 WIN32/APPLE/UNIX 分支，核对每个目标、源清单、链接模块、moc/qrc/qsb、安装路径和测试注册。Windows API、Dwrite/MSVC 选项必须仅存在 Windows 分支；Apple 与 Linux 分支引用的文件/符号必须真实存在，不得 `#error`、空实现、无条件提前返回或 silently disable 产品功能。禁止把运行时后端强制写死 D3D11 到所有系统。
- **COMMON-02 路径与资源：** 检查所有文件通过 QFile/QDir/QUrl/QStandardPaths 正确处理 Unicode、大小写、空格、路径分隔符和可写目录；不依赖盘符、当前目录或硬编码 Kit。QML 模块别名、qrc 字体/图标/模型/音频、插件相对路径、打包后读取位置必须形成完整解析链；DirectWrite 字体转换只作为 Windows 资产工具，转换产物供三平台共用。
- **COMMON-03 渲染闭包：** 检查相机矩阵、clip-space 修正、纹理 Y 方向、深度范围、MSAA resolve、透明排序、屏幕采样和 shader binding/location；Qt Quick 3D 自定义片段按 Qt 跨后端生成规则审计，独立 shader 与条件 QRhi 路径检查 Metal/GL 所需代码和 qsb 变体。当前环境可执行的qsb跨目标生成与产物清单核对必须运行；生成 MSL/GLSL 只作为静态证据，不能宣称目标 GPU 已验证。当前环境无法执行的目标编译器检查逐项写明限制，不要求安装第三方交叉工具链。
- **COMMON-04 媒体与生命周期：** 从命令到 QMediaPlayer/QAudioOutput/QSoundEffect 的调用链逐项检查 source/设备/错误/EndOfMedia/取消/销毁；保留真实公开 API 实现，不用模拟播放器作为非 Windows 产品后端。默认设备及设备消失回退、不可 seek、未知格式、资源错误、后台与恢复都有可执行逻辑；不引用 Swift、外部 FFmpeg 命令或手动 PCM。
- **WIN-01 平台集成：** 展开MSVC/C++23、WIN32入口、PE资源/原创ICO、windeployqt的QML/CanvasPainter/媒体插件与随附依赖闭包、运行库要求、Unicode/空格路径和QStandardPaths数据目录；检查根启动器从自身位置解析安装/构建产物，参数引用与退出语义正确，不依赖开发PATH/Node/外部FFmpeg。静态审计不生成或宣称已运行目标PE/DLL；缺少可靠Windows环境的真实构建、后端、设备、图标及干净部署检查逐项移交R34。
- **MAC-01 平台集成：** 审核 bundle 目标、Info.plist、可执行文件与 Resources/Frameworks/PlugIns/qml 安装位置、macdeployqt 调用与输入存在性；确认 Metal 选择/默认行为、Retina 逻辑像素、Command 快捷键、菜单/关闭语义与可写数据目录实现。普通 `.command` 从自身路径解析产物并正确引用带空格路径，不依赖 Node、开发 Kit 路径或系统音频改造。
- **LNX-01 平台集成：** 审核 Linux 的 Qt 平台插件及依赖清单、xcb/Wayland 选择、OpenGL 默认和可选 Vulkan 的探测/回退；核对共享库搜索路径、插件/QML/媒体部署位置、`.sh` 文件的 LF/可执行位/参数透传/带空格路径及 XDG 数据路径。媒体依赖以所部署 Qt 插件的真实要求为依据，不把 Windows DLL 名称照搬成 Linux 库；权限/设备错误有提示，不静默安装软件或修改系统配置。
- **COMMON-05 证据与缺陷门槛：** 全部适用项均有可追溯证据，所有发现的功能缺失、构建/链接/资源错误、平台不兼容、部署缺陷与未实现路径必须修复；N/A 必须说明代码不适用的具体原因。当前平台共享测试、qmllint和可执行的资产/qsb检查提供真实结果，但不充作其他平台编译或运行结果。单纯缺少对应系统实机不构成缺陷，平台实现或审计证据缺失构成阻塞。
- **PERF-01–07性能闭包：** 按性能契约逐项审计复杂度/索引、线程/队列、复制/上传、资源/失效、算法/缓存、QML/输入和生命周期。展开目标平台实际调用链与资源计算，Windows结果不宣称目标GPU性能；新增优化记录源码hash及受影响功能/视觉证据。
- 纯静态检查结论写“静态审计通过，实机待验证（R34）”；实际运行检查单列系统/Kit/后端/设备/尺寸/DPR、覆盖范围和真实结果，不把整个macOS或其他平台统称为只通过静态审计。报告记录源码hash、Qt版本、覆盖及无法静态证明的风险。实机清单给出configure/build/部署/启动、GUI、播放与设备检查的准确入口和预期结果；缺少实机任务必须保留在R34，不因阶段性主线收口而删除。

## 待执行轮次

下列轮次按AI代理自身能力评估并遵守上述规模与时间约束，允许小幅收尾浮动；真正开始前重新估算，不把人类开发日作为容量依据。达不到退出条件时先拆出具体补修轮次。

### 历史源码复查契约（R19之后立即执行）

- 用户要求对R01至R19及全部已实施子轮次逐项复审，防止此前为图像相似度引入的锯齿、模糊、字体/采样补偿等缺陷进入产品。**所有剩余复查轮次完成后才进入R20。** 本轮R19仍须纳入复查；不以其最近实现或已有测试通过豁免。
- 从`git log -- QT_QUICK_MIGRATION_PLAN.md`、各版本计划与对应实现diff恢复已移除的原任务、有效生产路径及后续修正。覆盖编号见各轮；R16B2V-P/B2W/B3等未完整完成的历史任务只复审已经存在的实现，不恢复已取消的像素门禁。格式、性能优化、平台迁移和未改计划的后续提交也追踪，不能只读最初实现提交。
- 每项外观算法、参数、资源及可疑补偿记录：原源码文件/符号、实际生产调用及有效override，原生位置，引入/修改提交和理由，跨技术栈差异、判定与验证。简明矩阵保存在`docs/contracts/source-fidelity-review.md`；批量历史导出、逐帧/截图/机器报告仍只放忽略目录。覆盖表须逐个原轮次/子轮次列出实际审查对象和结论，不以分组标题或“同前”替代逐项审查。
- 判定须区分三类：①有源码/算法证据的设计，保持其理论语义；②Qt/C++/RHI、字体与平台API所必需的适配，说明为何需要、语义如何等价、实际质量与生命周期证据，不能因代码形式或数值不同就机械回退；③仅由截图/SSIM/RGB/边缘差异推动、没有代码或独立技术依据的调参/补偿，移除或修复。历史图像通过不是保留理由，历史分数失败也不是回退正确实现的理由。
- 对出处或必要性不明的改动，继续追踪原调用链、数学推导、Qt实际API、原生字体/采样与资源路径并做有界实验，查明后处理；不能凭“可能必要”保留，也不能见到偏移/倍采样/平台分支就全部删除。源码有意的模糊、景深和风格化必须保留，消除错误DPR、采集、缩放、合成或采样造成的非预期瑕疵。
- 每轮先给出覆盖范围、可疑项、规模/耗时与验证能力，按现有规模契约执行；必要时在该轮开始前拆分为明确连续子轮，保持全量覆盖和R20前置关系。R19的时间/改动规模和可见窗口授权不继承给复查轮次。
- 修复必须附原理、受影响源码/状态/数学测试与当前可靠环境的实际质量/交互/资源回归；不生成原实机图像差分，不进行任何像素匹配或据图调参。共享渲染变更回归相关材质/灯光/后处理/封面/场景/文字/界面路径、格式、两组官方qmllint及Release CTest。缺少目标平台实机只把独立目标回归移交R34，当前可确认和可执行的修复本轮解决。

#### 用户观察到的阵列渲染问题（复查必办）

- **VIS-ARRAY-BLUR-01 · 小窗口阵列画面模糊，大画面较清晰，原因未查明。** 用户在R19测试期间观察到：摆放大量专辑的页面在一个较小的测试程序窗口中明显模糊，后续较大渲染画面的程序看起来较清晰。尚不能确认两者是否同一产品/探针入口、相机状态、画质、后端、真实DPR或渲染分辨率，也不能直接判为源码有意景深、DPR问题或复制了旧渲染缺陷。R19A先从已有本地输出和调用链识别入口/输入并登记缺失条件；R19D主责调查和修复，R19B核对相机/投影，R19A核对封面资源，R19C核对材质透射/LOD，R19G核对总览取景与合成。
- BLUR-01调查须在相同匿名库、相机/运动状态和画质下分别控制窗口大小、实际DPR及render scale，记录逻辑尺寸、实际物理目标/附件尺寸、像素保护是否生效和最终显示缩放；核对封面解码/图集/高清租约、mipmap/LOD/过滤、Qt合成层尺寸与采样，以及景深焦平面、深度与CoC单位是否按源码正确换算。先区分不同程序/场景和采集路径，不仅比较窗口大小；不把环境变量当真实DPR，不拿放大的图片或离屏目标尺寸冒充实际窗口清晰度。保留有源码依据的景深/磨砂/背景模糊，修复额外缩放或采样等造成的非预期模糊，不以锐化、关闭效果或无依据提高全局分辨率掩盖原因。
- **VIS-ARRAY-FLICKER-01 · 大量专辑闪烁，半透明重叠区域尤其明显，属于待修渲染缺陷。** 用户在同一类大量专辑页面中观察到明显闪烁，当前未确认具体入口、后端及触发状态。不得将其当作复刻目标、正常设计或测试噪声；即使旧项目也有相同表现，仍须消除缺陷。R19C主责透明材质/排序/合成修复，R19D协查深度、采样、后处理与资源时序，R19B协查几何/相机；R19A先登记已有输入与入口，不能用此前功能或资源检查通过否定用户观察。
- FLICKER-01须分别观察静止/活动波浪、浏览移动、总览转换与不同重叠关系，定位最早出现异常的渲染通道。沿生产路径核查透明排序与稳定ID/深度键、前后表面及draw顺序、depth test/write和深度精度/共面冲突、捕获隔离与读写反馈、MSAA/resolve、跨帧目标/图集就绪与资源复用；检查shader非有限值或不连续状态。以上是待验证方向，不是已确认根因。用源码、通道状态和实际连续运行证明原因，不以旧实机图逐像素匹配或相似度打分定位；不得靠移除透明度、减少专辑、冻结正常动效或整体模糊隐藏闪烁。
- **两项均为R19A–R19H复查的明确退出约束。** 分别记录复现条件、源码依据、已确认根因、修复/保留设计的理由及受影响回归。BLUR-01须说明哪些模糊为源码设计，并消除非预期模糊；FLICKER-01须在代表性静止/运动/重叠场景下真实连续验证无异常闪烁。按风险选择小/大窗口、实际DPR及相关后端，复用有效结果，优先隐藏检查；确需原生窗口时遵守当轮授权与显式选择规则，不重启无关完整窗口矩阵。当前环境可可靠复现/修复的部分必须在复查阶段收口，不以“源码审计通过”“零warnings”代替实际质量，也不默认推给R34；只有独立缺环境的目标平台回归可按动态规则移交。R19H在进入R20前逐项核对，不得遗漏或无结论关闭。

### R19D · 阴影、后处理、抗锯齿与渲染集成源码复查

- 依赖R19C2C。逐轮覆盖R09、R09B、R10、R10C、R10D及后续性能/Metal/OpenGL适配，含历史可见窗口补验提交`e98c798`的图形改动。
- 审查阴影/偏置/滤波、SSAO/depth/normal、Bokeh、SMAA、MSAA/resolve、曝光/一次ACES与sRGB、render scale/DPR与目标尺寸、RHI通道顺序。追踪为图像匹配增加的核/偏移/模糊/精度或边缘处理，修复无来源补偿；API能力探测、等价renderbuffer与精度/采样保障不得机械撤销。
- 补齐已确认的生产雾路径缺口：冻结`scene.ts`创建Fog并在实际rendered camera distance基础上设置`near=distance+lerp(5,-1,detail)`、`far=distance+lerp(25,12,detail)`，纸面/玻璃材质fog=true；当前SceneFrame/RhiItem与shader没有fog输入/合成。按锁定Three的meshlambert/meshphysical、WebGLMaterials及fog_fragment实际调用顺序移植主题fogColor、viewDepth与smoothstep，核对其在toneMapped=false纸面、透射捕获、壳体、后处理/最终输出的有效色彩域与次序，不以增加全局白雾或截图调参替代。当前B1的近距独立通道fixture不触发源雾，不将其通过当作生产远阵列雾已实现；新增源公式和近/远/详情/主题真实状态测试，明确区分源码有意雾化与BLUR-01非预期模糊。该修复是R20前主线任务，不移交R34。
- 接收R19C2C本机隐藏OpenGL真实失败：独立8个光学覆盖状态通过，但完整生产SceneItem在首个capture读回R16F rawAO时抛Nonfinite scalar AO pixel，不能写成环境不可用或移到R34。输入为rhi-flicker-audit匿名9×64库、640×480、DPR1、原画质、4×、warmday/静止；入口`QSG_RHI_BACKEND=opengl QT_SCALE_FACTOR=1 build/release/tools/rhi-flicker-audit verification/r19d/opengl`（Review ON），原失败log为忽略verification/r19c2c-opengl.log。沿ssao shader/均匀表面和零向量、R16F attachment/clear/readback格式、QRhi GL能力与别名逐项定位首个非有限值，修复并重新运行完整960帧及普通生产输出；optics-only通过只限壳体，不能替代AO修复。该任务在R20前主线处理，原有限值门禁不放宽。
- 主责收口VIS-ARRAY-BLUR-01的小/大画面清晰度差异，证明景深设计与分辨率/DPR/缩放/采样的实际关系并修复非预期模糊；协同收口FLICKER-01中属于深度、resolve、后处理或跨帧资源的闪烁，不把用户观察归为未验证的正常动效。
- 分离旧GPU oracle/浏览器witness/像素分数与真实格式/资源/上传/功能验证，解除错误门禁并保留必要检查。复审原绘制路径选择理由，依据源码等价、产品质量、资源与成本重新确认，不能仅因旧分数选择或更换技术栈。
- 退出：上述逐轮矩阵和修复完成，实际后端无漏画/异常模糊/错误合成且轮廓平滑，正常设计效果保留，资源/生命周期/局部成本回归通过；完整120秒产品性能与尚缺目标环境按原R28/R34任务执行。

### R19E · 浏览控件、滑尺和数字字轮源码复查

- 依赖R19D。逐轮覆盖R14、R15及后续相关补丁，检查页眉/品牌容器、按钮/运输控件、浏览信息/脚注、滑尺/数字字轮、Canvas Painter与Qt Quick合成。
- 核对CSS结构、尺寸/留白/边界/字体/字距、主题、响应式、路径/描边、动画/退出保宽与输入路由。排查源截图反推的基线、像素取整、额外层或采样，区分字体度量/Qt布局/无障碍/缓存所需适配；组件比较只保留源码/功能/质量/成本依据。
- 退出：对应逐轮矩阵和修复完成，九尺寸/双主题/代表DPR的结构、运动、真实质量、键鼠/焦点、隐藏恢复与资源检查通过；播放歌名和按钮布局保持源码行为。

### R19F · 所有标题字轮及字体/采样补修源码复查

- 依赖R19E。逐项覆盖R16、R16B1、R16B2及B2V已实施部分、R16M1、R16M2、R16M2P、R16M3和后续字体/页眉补丁；核对B2V-P/B2W/B3历史是否存在已实施代码并纳入，不能因其旧任务未完成或被移除而遗漏。
- 高优先级追踪混合字体基线、字体框顶部/半行距、分数字号、整行shaping/字形关联、emoji字体面、CurveRendering/NativeRendering、倍DPR槽层、过滤/裁切/取整与隐藏缓存通知。分别找到原字体/CSS/生产调用、Qt语义及引入原因；数学/字体依据充分的修正保留，只有图像追分依据的改动移除或改为有原理的实现。
- 解除所有字轮/页眉工具的SSIM/RGB/边缘带完成门禁，保留真实内容、字体身份、布局、动画、退出保宽、缓存/隐藏/恢复/生命周期和成本检查；不恢复旧像素补修任务。
- 退出：每个子轮次及可疑补偿均有明确结论，普通文字/连字/组合字/彩色字体清晰且语义正确，当前可靠后端/DPR/主题和功能/资源检查通过，保留实现有可复核的必要性而非相似度依据。

### R19G · 详情、总览及本轮新增实现源码复查

- 依赖R19F。逐轮覆盖R17、R18、R19及其后续修正：展示结构/排版/虚拟化、抽取/归位/旋转/副本材质、总览相机/真实列居中/边缘折叠/标签投影与两步进入。
- 核对源代码状态/时序、CSS/文字度量、相机/几何/材质揭示、标签滞回/碰撞/动画和有意背景模糊；追踪本轮及历史所有按截图得出的调整。必要的Qt焦点、渲染同步、模型/资源所有权适配须有独立理由，不能将平台/测试缺陷当设计复刻。
- 退出：对应逐轮矩阵和修复完成，源算法数值、真实交互/焦点/打断/播放隔离、窄屏、减少动态/改速、隐藏与十次重建通过；无仅因旧图相似度保留的调整。

### R19H · 全历史覆盖与共享回归收口

- 依赖R19A–R19G，作为进入R20前的最后复查轮。对照Git历史逐个原轮次/子轮次及其后续提交核对覆盖，不以“源码看起来一致”或既有测试通过代替变更来源判断；补齐遗漏审查和当前可确认的修复。
- 汇总保留的必要适配、已移除/修复的无依据补偿和具体技术理由，确认不存在继续驱动实现/验收的原图逐像素工具或门禁，更新相关契约的现行要求。历史失败事实保留为历史，不冒充通过或重新变为待追分任务。
- 在当前可靠环境执行受影响的真实产品序列、源数学/结构、实际清晰度与平滑轮廓、双主题/尺寸/DPR、资源和Release共享回归；独立缺环境事项按非像素要求完整移交R34。
- 专项核对VIS-ARRAY-BLUR-01与VIS-ARRAY-FLICKER-01的原因、修复及实际质量结论；本机可靠验证仍有异常模糊或重叠闪烁时不得进入R20，不能以旧测试通过或与旧渲染相似关闭问题。
- 退出：R01–R19及全部实际实施子轮次覆盖无缺口、无来源/必要性未查明的外观补偿，所有本机可靠检查通过，必要跨栈差异有依据，后续R20才可开始。不得宣称未经目标实机验证的全平台理想渲染已经完成。

### R20 · 二维序章、三维开场和交互交接

- 依赖R03B、R08、R12、R14、R17。交付OpeningController、Opening.qml、原创品牌轮廓/圆环路径动画。
- 二维序章的品牌轮廓/圆环路径采用 R14 绘制契约验证的 Canvas Painter，按该组件的状态归属与实际成本选择 C++ 或 Canvas2D。复用原创 SVG 主素材的中心线/弧长与稳定分组，缓存不变路径，只更新揭示/描边/透明度等必要状态；与适用的 Qt Quick Shape/ShaderEffect 路径同输入比较，保留准确轮廓、抗锯齿、合成层级和既定时序，不通过改写三维管线换取二维绘制收益。
- 按musicOpeningFrame逐参数采样，不重写近似时间表：轮廓0.24–1.64s、标语2.72–3.58s进入/3.58–5.70s静读/5.70–6.32s退出，标语“一张一张，慢慢听”。
- 二维底纸6.32s开始消退，三维6.40s从源时间21.92开始，11.60s落稳；主页错峰720ms完成后解锁。主品牌节点复用，不通过两份品牌交叉淡入。
- 揭示 R03B 提供的新 Logo 中心线和分组，曲率/分段映射按品牌规范；原时间窗、位置、目标占位与交接节奏保持，旧品牌几何不进入生产资源。品牌区域另验收新 SVG，其余画面还原原算法/代码的理论效果，实机图仅作大致参考。
- View3D准备完成后才起片头；0–6.32s完全遮住时不提交三维帧，必要时显式暂停View3D/RHI更新而非只设opacity=0。
- 性能设计与验证：资源准备/首帧/固定片头/可交互分段，事件就绪谓词启动；取消/跳过不创建第二套场景。核对完全遮挡时实际三维提交为零，不只凭opacity判断节省工作。
- 退出：跳过/重播/后台恢复/中途减少动态有明确终态、正确焦点与一次交接；20–120fps时序golden通过；Canvas Painter 已走实际二维序章路径，品牌描边/揭示、双主题/DPR、遮罩交接和重复播放资源回落通过源码/结构/状态及真实质量检查，静止/退出后无持续重绘。

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
- 性能设计与验证：先收口已有PERF-01–07和确定缺陷，不能到本轮才首次审计架构。交付测量入口、各阶段预算依据、累计资源清单及当前可靠环境的实际基线；历史Windows R10D的18.35/20.98ms波动须在同Kit/设备/fixture下解释，当前缺少该环境则将独立正常VSYNC/吞吐复现及并发GPU/噪声记录完整移交R34，不把历史记录项视为达标。本机不可靠的呈现指标同样后置；所有真实固定场景按正常VSYNC/未限速分开串行至少三次采样，保留失败/原始样本和临界补修任务。
- 退出：PERF-01–07及当前可可靠执行的性能/生命周期检查通过，10次重建/30次换片可验证资源回落；初始化、离屏局部成本与真实呈现样本分别记录，未可靠测得的实机性能/资源峰值及原目标完整移交R34。

### R29 · 全GUI源码重放与渲染质量修复

- 依赖R28。按冻结生产调用链覆盖九尺寸、双主题与两阵列模式，交付源码参数/结构、动画数值及实际交互和渲染质量结论。
- 执行全部验收序列，核对头部、文字、滑尺、盒体、光带、背景、总览和片头的算法、布局、材质/效果、状态与时序；实际运行检查清晰度、平滑轮廓、裁切、合成和交互。原实机图只作大致参考，禁止逐像素对比、SSIM/RGB/ROI追分或复制其锯齿/模糊。
- 修复顺序为几何、布局/相机、字体、颜色、光场、材质、后处理、时序/打断；每次只回归受影响场景和相关行为。不把扫描元数据、格式支持或音频简化当作GUI缺陷，也不以截图差异代替源码错误的证据。
- 视觉修复涉及采样、通道、资源或调度时复核PERF-01–07及局部成本，不能引入额外overdraw、无效重建或输入停顿。复杂补修按明确组件/算法/事件边界拆分，不以图像ROI或像素分数拆任务。
- 退出：源码定义、结构/运动、实际质量、功能、资源和性能要求通过；未解决的真实产品问题写明原因、修复对象和复现入口，不宣称已达到无法证明的理想效果。

### R30 · Windows部署实现与动态阶段验收

- 依赖R29。交付Windows部署脚本、根普通启动器及Qt使用说明；真实Release目录/干净环境运行按能力判断，当前macOS无法执行的Windows构建、windeployqt与实机部分移交R34，不作为R31的前置。源码/脚本可确定缺陷必须本轮修复。
- `tools/deploy/` 使用 windeployqt，`--qmldir` 指向根 `qml/`，按生产导入与链接闭包部署 Qt Quick、Multimedia 及 Qt 随附媒体依赖，生产不打包 Quick3D 研究场景/插件；打包 qrc 资产与 MSVC 运行库要求到 `out/windows/`，不复制整个 Qt 安装目录。
- 纳入实际 Canvas Painter 生产闭包：部署 Qt6::CanvasPainter 运行库及 Canvas2D 被采用时的 QtCanvas2D QML 模块/插件与其真实依赖；即便主界面使用 C++ 注册绘制项，也要核对打包后资源/shader 与 RHI 初始化。脱离开发 Kit 运行滑尺及二维序章，检查无缺失模块/插件、正确透明合成和重建行为，不把 Review 探针或未采用的比较实现一起打包。
- 接入 R03B 原创 ICO 到 Windows 可执行资源，Qt 窗口图标及关于页使用同源 SVG/PNG；显示名 `yalahOO!!!`、输出文件 `yalahoo.exe`。实机检查 Explorer、窗口/任务栏及大/小尺寸图标；macOS ICNS/bundle 和 Linux desktop 图标由R31/R32按同一品牌清单接入，并按各自实际可执行能力验收。
- 用无Qt开发PATH、无Node/FFmpeg路径的临时环境运行；验证离线启动、demo、unicode音乐路径、设备列表、可支持样本和字体加载。
- 普通启动器只找合法安装/构建产物并启动Qt程序，不后台启动Web服务；演示/光照启动器继续在tools/launchers/。
- 性能设计与验证：部署Release通过正常启动路径复核主要性能/输入场景、冷启动和资源峰值，区分开发Review/产品开销，记录实际尺寸/DPR/刷新/VSYNC，不转抄旧benchmark。
- 退出：Windows部署/入口/资源/数据路径实现和严格审计通过；有可靠Windows环境时执行并通过真实Release干净环境、GUI/音频/设备/尺寸/DPR检查，否则把完整要求和已知问题逐项移交R34。实际运行记录OS/GPU/Kit/后端，包不含私人库/缓存/调试基线；静态交接不得写为实机通过。

### R31 · macOS完整实现、部署与动态阶段验收

- 依赖R30的部署实现与阶段交接，不依赖后置Windows实机检查。交付同一 C++23/Qt 源码的 macOS CMake 分支、`tools/build/` 与 `tools/deploy/` macOS 脚本、根普通 `.command`、`verification/macos-static/` 审计产物及 `docs/platforms/macos.md`。使用该开发机安装的最新原生 Qt Kit，不固定版本；当前macOS主力环境可可靠执行的configure/build/test、macdeployqt与bundle脱离Kit检查必须真实执行，其他项目逐项移交R34。
- 完整实现 bundle/Info.plist、资源路径、macdeployqt QML/媒体插件部署、Retina 逻辑像素、Command 菜单/快捷键、关闭与后台语义；产物放 `out/macos/`，不引用 Swift 桥接。根普通 `.command` 新建为 Qt 启动器；原脚本继续在 `legacy-web/` 保留，Qt 演示/研究入口在根 `tools/launchers/`。
- 按 COMMON-01–05 与 MAC-01 逐项展开实际源码/构建/部署链，审核 Metal/字体/qsb 资源与媒体设备逻辑；共享功能在当前可用平台真实回归，其他平台回归移交R34。修复所有可确定缺陷，禁止空分支、直接禁用功能或用 Windows 截图作为 macOS 图像证据。
- 补齐 Canvas Painter 的 Metal/Retina、绘制项与三维 SceneItem 合成次序、资源/shader 和实际 Canvas2D 导入（若采用）的 bundle/macdeployqt 闭包；审计同步/线程所有权、透明与裁切、DPR/cache 失效和重建释放。用户实机清单覆盖实际滑尺/序章及采用的文字路径，不能仅凭模块导入成功认定绘制通过。
- 用户验证清单列出 Qt Kit 设置、configure/build/test、macdeployqt、bundle 启动，以及代表性浏览/详情/总览/片头/玻璃 ROI、本地播放/暂停/seek/设备变化的命令或步骤及预期结果；真实执行结果按当轮可靠能力记录，未执行项明确移交R34。
- 性能设计与验证：补齐PERF-01–07在Metal/Retina上的尺寸/峰值、同步、缓存失效和生命周期推演；用户清单包括正常VSYNC/吞吐、输入和准备时延，不用Windows成绩填写目标结果。
- 退出：macOS实现、脚本、部署闭包及严格审计通过，当前可靠构建/测试/离屏/部署检查均真实通过；不可靠的窗口/呈现/媒体设备任务完整移交R34后完成阶段交接。结果分别标注实际运行范围和“静态审计通过，实机待验证（R34）”，不得将已有macOS实测写成只有静态审计。

### R32 · Linux完整实现、部署与动态阶段验收

- 依赖R31的阶段交接及R30部署实现，保持主线顺序。交付 Linux CMake 分支、`tools/build/` 与 `tools/deploy/` Linux 脚本、根普通 `.sh`、`verification/linux-static/` 审计产物及 `docs/platforms/linux.md`。当前缺少Linux目标环境的构建/部署/运行项目严格审计并移交R34；以后环境可用时按动态规则执行。
- 完整实现 C++23、该开发机安装的最新原生 Qt Kit 与 CMake/Ninja 构建及 `out/linux/` 部署，不固定 Qt 版本；明确 QML/平台/媒体插件、共享库搜索位置与 Qt 所需系统依赖。优先 OpenGL，可选 Vulkan 经探测可回退，X11/xcb 与 Wayland 所需路径和配置均实现。普通 `.sh` 从自身路径解析产物，保留参数，设置正确 Git 可执行位。
- 按 COMMON-01–05 与 LNX-01 检查大小写/Unicode/XDG/权限路径、字体与 shader、触摸板 pixelDelta、默认设备及错误处理；展开所有非 Windows 分支而非只审阅入口文件，修复每项静态可确定缺陷。系统包要求按实际 Qt 插件写明，脚本不静默修改系统音频/驱动配置。
- 补齐 Canvas Painter 的 OpenGL/所启用 Vulkan、X11/Wayland 合成、资源/shader 和实际 Canvas2D 导入（若采用）的库/插件闭包；审计同步/线程所有权、透明与裁切、DPR/cache 失效和重建释放。用户实机清单覆盖实际滑尺/序章及采用的文字路径，Windows OpenGL 结果不代替 Linux 实机绘制。
- 用户验证清单列出 configure/build/test/部署/启动命令、X11 与 Wayland、DPR/窗口尺寸、浏览/详情/总览/片头、默认设备与播放/seek/设备变化检查及预期结果；真实执行按当轮可靠环境记录，未执行项目完整移交R34。
- 性能设计与验证：补齐PERF-01–07在GL/可选Vulkan及X11/Wayland上的格式/尺寸/峰值、同步与节流推演；用户清单含正常VSYNC/吞吐与输入/准备时延，不从D3D11推断目标GPU效率。
- 退出：Linux实现/部署闭包及严格审计通过；有可靠目标环境的适用检查实际通过，否则保留完整构建、X11/Wayland、图形/音频/设备/部署任务到R34后完成阶段交接。纯静态项标注“静态审计通过，实机待验证（R34）”，不宣称未运行的目标通过。

### R33 · 原生主入口收口与最终文档

- 依赖R30–R32的实现与阶段交接、当前平台所有可执行视觉补修；不依赖R34内尚无环境的任务。交付根启动器、README/开发说明、源模块映射和最终离线功能清单。
- 默认产品入口指向Qt；全部 Web 源码/基线/研究资产继续隔离在 `legacy-web/`，Qt 源码、QML、资源与测试遵循根目录结构，普通用户流程不再要求Node、浏览器、Swift或外部FFmpeg。
- 文档明确普通播放、Qt支持格式边界、联网接口暂未启用、导入旧数据方式、Qt构建/平台部署命令和仍可复现的视觉残差；不恢复本计划已排除的能力。
- 核对各平台源码/构建/资源/GUI/离线Provider/普通播放/部署的实际运行覆盖与严格审计，按能力逐项区分通过、失败和暂缓；将所有缺少可靠运行条件的实机要求、已知平台/后端补修与回归完整汇入R34。R33只收口主线实现和文档，不表示全平台实机验收完成。
- 性能设计与验证：收口核对性能设计审计、资源清单、逐场景多次原始样本、预算依据/测量范围与残余风险；不能以平均FPS、CPU提交或Review报告承诺所有设备完整体验。
- 退出：主线实现与当前可靠检查完成，文档准确描述范围、残差与限制，R34完整保留所有尚未执行的专项任务；删除已完成主线轮次但不得清空R34或宣称未运行平台可用。最终实机结论在R34完成后更新。

### R34 · 后置平台/后端补修与最终实机验证（特殊轮次）

- 依赖R33的主线实现/阶段交接和各轮移交清单，作为最后一个专项阶段。按真实环境能力安排目标，不固定平台的验证方式；修改共享路径后回归当前主力平台受影响功能。
- **本专项不保留任何逐像素比较任务或完成门禁。** 删除浏览器/后端图像趋同、SSIM/RGB/ROI分数、文字边缘带、GPU导出PMREM与浏览器逐字节相等的补验收。历史未达图像目标的记录不改写为通过，也不再构成待修任务；只有源码审计或真实产品运行确认的缺陷才进入修复清单。本节引用的旧契约/脚本若仍耦合这些比较，先拆开相应检查，不以旧像素结果阻止功能/质量验收。
- 保留源码定义、结构/投影、文字内容与字体身份、运动/时序、实际渲染质量、交互、资源、窗口、媒体、部署和性能验证。图像可用于定位漏画、错位、裁切、错误合成、可见锯齿或异常模糊，不进行逐像素匹配，不复制基线渲染缺陷；源码明确要求的景深和背景模糊分别核对。
- 按后端/DPR、窗口、媒体、部署或性能边界拆为明确执行单元，每次只完成一个边界。各轮移交记录目标环境、能力缺口、匿名输入/源码版本、修复对象、准确入口/预期结果、非像素门槛、依赖和共享回归；已知产品失败与尚未运行分别记录，不提前生成假入口或假证据。

#### R19A-P · 资产来源保护与缺图绘制的目标平台回归

- 依赖R33及可靠Windows/Linux最新原生Kit。输入为冻结来源57f4f845c20cce646a15dbf4008c2a162aaaf4e2（按history-rewrite映射）、assets/branding清单、匿名本地/qrc封面与现有库fixture；实现与历史归属见`docs/contracts/source-fidelity-review.md`。当前macOS没有目标编译器/Qt原生后端，未执行新Windows/Linux运行，也没有新的目标实机失败；不能把历史通过当成当前源码通过。
- 严格静态核对`PrepareAssets::verifyFrozenInputs`、WIN32 DirectWrite/非Windows SFNT复用、QImageReader插件/ICC、CoverCache脚注字重及generation/lease/serial、QtKit架构/ABI、qrc/QtSvg与隐藏窗口分支；本机macOS Release资产/缓存/模型及Metal隐藏回归不外推目标平台。
- 目标Release构建后运行`prepare-assets <仓库> <同一最新Kit> --verify-inputs`及`ctest --preset qt-release -R '^(assets|branding|cover-cache|rhi-atlas-queue|library-data|library-qml|kit-selection|module-link-and-plugins)$' --output-on-failure`。在临时隔离源码根篡改一个输入，确认普通生成入口在任何写入前失败；不得修改真实归档/产品资产完成负向测试。
- 退出：冻结blob/归档/清单一致、完整几何/字体物理表/功能SVG及ICO/ICNS容器有效、无声媒体仅加载不播放、ICC/横竖方/alpha/缺图脚注符合源码、队列/租约/关闭与PERF-01–07通过；不运行旧图比较。共享修正回归当前主力平台的assets、cover-cache、library-data/library-qml与完整隐藏Release；目标GPU、部署和媒体设备仍由本节对应专项验证。VIS-ARRAY-BLUR-01/FLICKER-01保持R19C/D主责，不随本项后置。

#### R19B-P · 运动、相机与阵列的目标平台回归

- 依赖 R33 交接及可靠 Windows/D3D11、Linux/OpenGL 原生 Kit/窗口管理器。输入为冻结来源 57f4f845c20cce646a15dbf4008c2a162aaaf4e2（经映射）、motion/scene/archive-pool/real-columns 数学 fixture 和匿名库。当前 macOS 没有目标编译器/Qt 后端/窗口管理器，未运行这些目标；本机 Metal 隐藏结果、历史 Windows 结果均不代替当前目标运行。
- 严格静态核对唯一帧源/速度、double x/v/a、canonical GL 投影→QRhi 修正、GLB/XYZ 矩阵、最多 432+4 实例、惰性列时间/减少动态 epoch、稳定物理 ID、lease/generation/关闭及所有平台条件编译；审查结论与实现符号/hash 入口见 `docs/contracts/source-fidelity-review.md` 的 R19B。没有新目标实机失败数据；保留本机历史策略失败与局部计数误用诊断，不将其改写为目标失败或通过。
- 目标 Release 构建后运行 `check-web-baseline.mjs` 及 motion/scene/archive-pool/real-columns 源码导出器；用当前系统 `verification/{windows,linux}/real-columns/full-trajectories.json` 设置 `YALAHOO_REAL_COLUMN_GOLDEN` 运行 RealColumnsTest。普通快速 CTest 覆盖 motion、scene、real-columns、archive-pool、cover-cache、rhi-atlas-queue、navigation、detail-motion、overview、verification-window-policy；两组官方 qmllint/check-format 必须通过。
- `YALAHOO_REVIEW=ON` 后通过 `ctest --preset qt-release-visible -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure` 运行目标实际后端；入口现无任何图像匹配，不要求旧 PNG。按当轮授权选择默认置底/拒绝焦点或显式前台；保留源 x/v/a≤1e-6、Qt float 矩阵按既有测试容差、投影≤2逻辑px、边界事件消费不改变稳定 ID、两模式/记忆/库替换、90,000 库容量、物理 ID/cover 区域 rebase 不变、稳态零上传、≤2MiB 突发及 cover/renderer 生命周期要求。源 x/v/a≤1e-6，不能放宽为图像相似度。
- macOS 的实际 GPU 隐藏重建已执行；三平台物理输入、原生窗口/表面与正常 VSYNC 按本节已有窗口/输入专项继续，不用 offscreen invalidation 替代。目标共享修正后回归当前平台 motion、scene、archive-pool、real-columns、navigation、detail-motion、overview、封面与隐藏 Release；BLUR-01/FLICKER-01 仍须 R19C/D/H 在 R20 前收口，不能随此项目后置。

#### R19C1-P · 材质与光场参数的目标平台回归

- 依赖R33及可靠Windows/D3D11、Linux/OpenGL原生Kit/编译器/窗口管理器。输入为冻结57f4f845c20cce646a15dbf4008c2a162aaaf4e2（经映射）、R19C1提交中的45组materials/parameters、210组render-backend参数、lighting/golden及匿名阵列；实现符号与hash入口为check-web-baseline及audit-material-parameters。当前macOS缺少目标原生工具链/后端，目标未执行，没有新的目标实机失败；当前受限服务no-screens失败保留本机日志，不冒充目标失败或通过。
- 静态展开ShellMaterial/SelectionLighting/RowLightController→SceneItem→Renderer uniformTemplate/逐实体lid/上传、QML有效属性、WIN32 D3D适配隔离、Linux/Apple真实Qt分支、std140和qsb各目标。矩阵入口`python3 tools/audit/audit-material-parameters.py`；当前同Kit跨目标qsb只证明静态翻译。
- 目标最新Kit配置Release，运行`node tools/review/export-material-parameters.mjs`、check-web-baseline、`ctest --preset qt-release -R '^(rhi-material-parameters|lighting|scene|cover-cache|rhi-atlas-queue|verification-window-policy)$' --output-on-failure`。Review ON后按窗口契约运行`ctest --preset qt-release-visible -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure`，不要求浏览器原图；能可靠隐藏时优先用真实隐藏入口，不能写假offscreen结果。
- 退出：45+210源参数、924轨迹/392场核和真正上传的transport/coat/tint/attenuation逐实体通过（既有2e-6/3e-5、轨迹/主题1e-6、位置1e-4容差不放宽），无ROI系数重入，双主题/详情/归位、资源重建和默认窗口策略通过；两组官方qmllint/check-format与完整隐藏Release回归。光学shader/连续质量与BLUR-01/FLICKER-01仍由R19C2/D/H在R20前处理，不随本项后置；共享修正回归当前主力平台材质/灯光/阵列/封面与完整隐藏集。

#### R19C2A-P · 材质与灯光Review目标平台回归

- 依赖R33及可靠Windows/D3D11、Linux/OpenGL最新原生Kit/编译器/窗口环境。输入为R19C2A实现提交（起点48a9495）、冻结57f4f845c20cce646a15dbf4008c2a162aaaf4e2（按映射）、materials/single-case.json、lighting/visual.json及均匀paper-probe.png。当前macOS没有目标原生工具链/后端，目标实机未执行、没有新目标实机失败；本机Metal隐藏通过不代替目标结果。PowerShell入口本机仅静态核对，目标需执行真实解析/格式及进程等待。
- 静态展开MaterialReview→独立GeometryScene→OffscreenSurface/原生窗口、QML root/engine/控制器所有权、原子invalidation计数、qrc/GuiPrivate/QtQuick3D、Windows MSVC入口及真实Python进程退出；`python3 tools/audit/audit-material-review.py`生成绑定符号/hash的COMMON-01–05/PERF-01–07及平台矩阵。
- 目标Release Review ON后运行`ctest --preset qt-release-visible -R '^(runtime-materials|runtime-lighting)$' --output-on-failure`（可靠隐藏能力改变时先重新分类）；默认置底/拒绝焦点，只有当轮授权才可前台。Windows先执行`tools/format/FormatTools.ps1 -Check`并运行verify-r07.ps1/verify-r08.ps1，Linux可在配置工具链后运行`python3 tools/build/verify-material-review.py materials|lighting`的对应单个入口。入口不需要浏览器或原图，禁止恢复旧ROI/SSIM门禁。
- 退出：99材质/纸面与48灯光状态、3表面参数≤1e-6、源码/CPU/Quick3D投影≤2逻辑px、纸面源公式误差≤3码值、实际DPR/目标与有效输出、QML warnings为空及窗口策略通过；材质十次/灯光一次完整root重建均真实失效并重放有效输出。原生窗口的同对象hide/restore、正常VSYNC仍按本节窗口专项执行，不以完整root重建替代。两组官方qmllint、格式和目标完整适用回归；共享修正回归主力平台完整隐藏Release。RHI光学及BLUR-01/FLICKER-01仍由R19C2B/C、R19D/H在R20前处理，不随本项后置。

#### R19C2B1-P · 不透明RHI目标平台回归

- 依赖R33及可靠Windows/D3D11、Linux/OpenGL原生Kit/编译器/窗口管理器。输入为R19C2B1实现提交（起点75b5543）、冻结57f4f845c20cce646a15dbf4008c2a162aaaf4e2（经映射）、匿名三封面、已知GLB平面/源纸面光照/阴影公式及有界RhiCaptureHarness；本机macOS缺少目标原生环境，目标未执行，无新目标实机失败。
- 静态矩阵入口`python3 tools/audit/audit-rhi-opaque.py`逐COMMON-01–05/PERF-01–07/平台绑定符号/hash，核对D3D/GL clip/资源/线程、QPointer值交接、最多2批/唯一纹理读回、实际格式与imageOf端序/Y翻转；不以Metal或同Kit跨目标qsb代替目标运行。PowerShell薄入口和退役脚本本机只静态核对，目标另跑真实解析/FormatTools -Check。
- 目标最新Kit Release Review ON后，执行paper/shared shader检查、`ctest --preset qt-release-visible -R '^rhi-opaque-gpu$' --output-on-failure`，按当轮窗口契约；可靠隐藏能力变化时先重新评估。预期36状态、16源墨色探针、12有序尺寸、30逐帧resize、同request封面替换、6阴影状态及十次真实graph重建通过；保持深度<2e-6、法向/纸面≤1/255、四角≤2实际DPR1物理px和原shadow强度范围.675–.70，不降低门槛。alias字段共享storage、无额外readback，真实driver bytes未知为null。
- 两组官方qmllint、check-format及适用Release回归通过；共享修正回归主力平台完整隐藏Release（阵列/真实列/封面/材质/详情/总览）。隐藏graph重建不证明原生窗口恢复、正常VSYNC或产品性能，按本节原专项执行；BRDF/透明捕获、fog与用户BLUR-01/FLICKER-01仍在R19C2B2/C/D/H主线，不随此项后置。

#### R16B2W · D3D11/DPR2文字、成本与生命周期

- 依赖R33阶段交接和可靠Windows/D3D11目标环境；原R16B2V-P逐像素补修任务已移除，不再构成本任务依赖。执行前重新评估规模，覆盖真实DPR1/1.5/2中尚欠的字体、布局与生命周期事实，不为栅格相似度改动正确绘制路径。
- 输入为冻结字体/生产字轮源码、`tests/fixtures/text/titles.json`。在`tools/review/TitleAudit.cpp`、`tools/build/verify-title.mjs`及对应PowerShell入口中分离结构/运动/成本/资源结果与像素比较；独立检查不要求浏览器PNG，不能用`passed=false`的像素报告冒充功能结果。
- 检查20项双主题文字的内容、字体身份、连字/组合字/彩色字体、断行/省略/裁切与18项页眉的结构、按钮位置、改速/减少动态及定位不改变播放；验证十次恢复和最终释放。三路径各三次独立120帧采样，分别报告GUI、同步/CPU提交、Canvas绘制、缓存与资源，隐藏候选无文字绘制、静止无重排，GPU不可测为null。
- 退出：结构≤1逻辑px、源码运动≤1e-6、窗口/资源/生命周期、Release/source-format/官方qmllint/qsb与PERF-01–07检查通过；只回归实际改动影响的DPR/后端。原像素失败作为历史记录保留，不作为补修或选型要求。

#### R16B3 · OpenGL功能、质量与文字路径选型

- 依赖R16B2W及R16M3阶段选型。在可靠Windows/Linux目标OpenGL环境运行相同匿名文字/页眉、DPR/主题、运动、生命周期及三次成本采样，验证实际可见呈现；macOS已有隐藏OpenGL4.1/CoreProfile事实不外推到其他系统，也不无故重复未受影响矩阵。
- 修复实际后端的shader/pipeline、文字缺失、裁切、输入、缓存或资源错误；按源码核对布局/字形与运动，检查可见锯齿和异常模糊。保留正常画质，不为贴合另一后端的栅格改字号、基线或采样。
- 依据源码语义、实际功能/质量、成本、缓存失效与生命周期选择生产路径，写明连字/组合字、彩色字体及维护差异。执行COMMON/PERF和平台分支审计；原默认GLSL120故障的修正及失败记录仍见`docs/contracts/title-reels.md`。
- 退出：上述非像素要求通过，更新简明结论，并回归R17–R33受影响路径；不以跨后端像素趋同作为选型要求。

#### R16M3-S · 共享隐藏完成通知的目标平台回归

- 依赖R33交接及可靠Windows/D3D11、Linux/OpenGL环境。当前macOS缺少对应原生编译器/窗口管理器/GPU入口；已有macOS运行事实和静态审计不代替目标运行。
- 输入为冻结字轮/字体、匿名titles fixture及TitleReel自然宽度通知、active=false有界queued finish/恢复取消、TransportTitle退出运动。运行目标Release的`title-reels`与完整CTest，确认十次hide合并、恢复取消、真实宽度单次通知；使用上节分离后的结构/运动/缓存/资源入口检查20项文字、18项页眉、九组活动/缓存、隐藏零文字工作、十次恢复/释放及零绑定环/shader错误。
- 此项为尚未执行的共享回归。结构≤1逻辑px、源码运动≤1e-6及窗口/资源门槛保留；修改共享通知链后回归当前平台受影响路径。真实可见hide/restore/minimize与正常VSYNC另行验证，离屏释放不代替呈现事实。

#### R17-P · 详情展示的目标后端与可见输入/焦点

- 依赖R33交接、R18同步运动ports及可靠目标Kit/窗口管理器。保留Windows/D3D11、Linux/OpenGL/X11/Wayland的组件验收和三平台后续产品运动/焦点集成；macOS已经执行的九尺寸/双主题/DPR组件、10,000曲目虚拟化、介绍切换与恢复不重复列为欠验。
- 输入为匿名PresentationTest、PresentationAudit曲目/介绍/制作人员与冻结music-presentation/ui-transitions/music-title/music.css调用链。运行目标Release configure/build、官方两组qmllint和完整CTest；`ui-audit --presentation 输出目录`检查18项结构、非零且<100的委托、零QML warnings、十次恢复/释放与windowPolicy。审计入口`tools/audit/audit-presentation.py`，细节见`docs/contracts/presentation.md`，其中旧像素门槛不适用。
- 实际产品验证browse退场后相机进入、layoutRevision/场景就绪、退字连选/取消只提交最后目标、四种archiveRoute、快速open/back、改速/减少动态、介绍/歌单滚轮及键鼠/触摸。检查退出期禁用、有效Item焦点恢复、hide/restore/minimize及surface/flags重建；R23/R24后补真实定位不改变播放。
- 退出：结构≤1逻辑px、时序≤一个60Hz帧、源码状态/revision与PERF-01–07通过；按授权选择前台或默认策略，遮挡无效样本不算通过。共享变更后回归presentation、presentation-qml、navigation、title-reels和完整Release CTest。

#### R18-P · 详情运动的目标后端、物理输入与产品集成

- 依赖R33及可靠目标Kit/窗口管理器/设备。输入为DetailMotion/ReturningCasePool/PointerDragController/CaseClarity、匿名detail-golden/qrc封面和冻结music-camera/scene/music-app；当前尚缺Windows/D3D11及Linux/X11/Wayland原生环境，没有新的目标运行失败数据。
- 目标Release构建、官方两组qmllint和完整CTest后，运行`python3 tools/build/verify-detail.py --backend d3d11|opengl --dpr 1|1.5|2 --runs 1 --output 全新目录`；当轮获授权才加`--foreground`，夜间用`--night`；三十次换片/十次重建仅在相关边界以`--lifecycle`另执行一次，不随每个DPR或功能重复运行。检查退browse后同步抽取/相机、三次换片/反向、满池最后目标、旋转副本对齐/下降与新盒等待、途中返回/搜索、拖动取消、改速/减少动态、三十次切片及十次图形上下文重建。
- 分开记录Qt提交/ready延迟、上传突发、四份CPU封面租约/分块、GPU图集/附件估算；运动不重建不变目标/材质，实际driver显存不可测为null。轨迹各≤1e-6、投影≤2逻辑px、时序≤一个60Hz帧及PERF-01–07保留。
- 三平台补真实触摸/系统抓取与焦点丢失、有效Item焦点恢复及R23/R24实际定位/播放隔离。自注入Qt事件不冒充物理设备或input-to-photon；macOS已完成回放不重复列欠验。三维源码/质量、完整GUI及120秒性能分别由相关任务验证。共享修正回归detail-motion、scene、archive-pool、real-columns、presentation、presentation-qml、navigation、title-reels及实际后端；入口见`docs/contracts/detail-motion.md`和`tools/audit/audit-detail.py`。

#### R19-P · 总览的目标后端、物理输入及平台回归

- 依赖R33、R19源码交付及R19A–R19H复查结论；输入为冻结来源57f4f845c20cce646a15dbf4008c2a162aaaf4e2、`tests/fixtures/overview/source-golden.json`和匿名内置库。当前macOS没有Windows/D3D11/OpenGL或Linux/xcb/Wayland原生Kit、窗口管理器与GPU入口；对应目标运行尚未执行，没有新的目标实机失败数据。静态审计见`tools/audit/audit-overview.py`，不替代实机。
- 在目标最新原生Kit的Release构建后运行官方两组qmllint、完整CTest及`python3 tools/build/verify-overview.py --backend d3d11|opengl --dpr 1|1.5|2 --runs 1 --output 全新目录`；夜间用`--night`，当轮明确授权才加`--foreground`。窗口压力序列以`--lifecycle`在相关所有权/上下文边界单独执行一次，不在每个DPR或功能重复运行。另以`--visual --runs 1`在真实可用隐藏入口覆盖九尺寸/双主题/收起与展开36状态、十次实际根/资源重建；目标无可靠离屏入口时使用遵守窗口策略的真实渲染并记录范围，不能伪造隐藏运行。
- 检查V/Esc/点专辑同一返回、第一击只展开/第二击引导进入、真实列/填充共用progress、±4.5折叠与extent≤.12不可拾取、标签滞回/碰撞/窄屏、播放歌名与按钮固定、减少动态/改速/反转、库替换、十次隐藏/surface重建和资源回落。三维目标/postprocess不随总览转换重建，图集稳态突发≤2MiB；UI局部模糊层另行记账，未知driver分配写null。
- 三平台补物理键鼠/触摸板、焦点丢失/恢复与真实窗口状态；R23/R24后补真实定位不改变播放。当前macOS自注入Qt事件、窗口读回/隐藏UI均不证明物理设备、屏幕扫描或完整120秒性能；这类独立事实仍需实际目标验证。
- 退出：数学轨迹≤1e-6、源代码投影≤2逻辑px，实际结构/交互/清晰度/平滑轮廓/资源和平台要求通过，无QML/shader/窗口策略错误；禁止原实机图逐像素比较或据图调参。共享变更回归overview、overview-qml、detail-motion、scene、real-columns、archive-pool、presentation、navigation、title-reels及受影响实际后端；准确范围见`docs/contracts/overview.md`。

#### MAC-VISIBLE-3D · 三维源码、渲染质量与正式采样

- 输入为冻结来源57f4f845c20cce646a15dbf4008c2a162aaaf4e2（经来源映射读取）、生产资源与render-backend/rhi-shell/postprocess/occlusion参数。按实际生产调用链核对几何/取景、颜色空间、光场/材质、透明深度/排序、阴影、环境贴图方向/精度和后处理，不要求浏览器GPU导出的PMREM字节或PNG与原生相等。
- 检查Metal/threaded、macOS OpenGL/basic的真实输出、可见锯齿、异常模糊、漏画/裁切/错误合成与资源生命周期。MSAA renderbuffer适配保留原采样数、颜色格式和resolve语义。旧rhi-opaque/rhi-postprocess等Review入口须先解除浏览器witness/图像比较与功能检查的耦合；没有截图参照不构成产品失败，已确认源码或渲染缺陷仍须修复。
- 用`yalahoo --verify-integration 新目录 --integration-seconds 120`在同Kit/设备/匿名输入下串行至少三次正常VSYNC，未限速吞吐另列；检查实际覆盖范围，不以已有12秒单盒探针代替完整产品、p95/p99/长帧、GPU/显示延迟或实测显存。完整产品准备、十次重建/30次换片与预算依R28/R33；历史Windows18.35/20.98ms按原环境复现，Metal不消除原记录。
- 按源码/单后端质量/MSAA与正式采样分执行单元，保留真实失败与测量限制；不运行逐像素匹配，不用截图瑕疵定义产品效果。运行产物保持忽略，只保存必要结论。

#### 各平台最终实机矩阵与专项退出

- 输入为同源匿名fixture、冻结生产源码、当前Release源码/二进制及具体产品失败/暂缓记录。先核对系统/Kit/后端/设备/入口/窗口策略，未具可靠环境的项目继续保留，不能把“已安排”写成“已执行”。
- 窗口：Windows/Linux验证首次show、hide/restore、minimize、flags/surface重建与退出；按当轮授权选择前台或默认策略。macOS已验收前台窗口边界不重复列欠验，默认隐藏策略持续回归；未授权模式遮挡/节流样本不可用，不提升Z顺序。
- 图形/输入：对尚欠D3D11、Metal/Retina、OpenGL及实际Vulkan，覆盖九尺寸/双主题/DPR1/1.5/2、浏览/详情/总览/片头、文字/彩色字体/滑尺、玻璃/纸面/透明合成、键鼠/触摸板/快捷键与焦点。核对源码效果与真实产品质量，不做逐像素、SSIM/RGB/ROI或边缘带匹配；发现锯齿、异常模糊等问题时定位实际原因并修复，保留缓存/资源差异。
- 媒体/部署：用匿名或用户指定本地样本真实播放、暂停/seek/停止/换曲，检查独立歌曲/BGM/音效、后台、默认/切换/消失设备与格式错误。部署包脱离开发Kit、Node/外部FFmpeg运行，检查Unicode/空格路径、资源/插件/字体/图标/可写目录，不改用户音乐文件。
- 性能/资源：同Kit/设备/fixture的正常VSYNC与未限速吞吐各至少三次串行，记录呈现事件、输入到Qt提交、冷启动/准备、资源峰值和重建/换片回落；保留120秒/1920×1080及p95/p99/长帧目标，解释历史Windows波动的适用范围和噪声。不可测显示/GPU完成写null或估算，不以离屏/CPU或其他设备成绩替代。
- 退出：上述源码、功能、实际质量、窗口、媒体、部署、性能与资源项目在可靠目标环境通过，受影响主线回归及文档完成后删除专项。缺环境、真实产品失败或未完成非像素检查保留具体任务；R33阶段完成不豁免本专项。

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

#### R19C2B2-P · 光学RHI目标平台回归

- 依赖R33及可靠Windows/D3D11、Linux/OpenGL最新原生Kit/编译器/窗口环境。输入为R19C2B2实现（起点3c90d2d）、冻结57f4f845c20cce646a15dbf4008c2a162aaaf4e2映射、rhi-shell/visual.json、源GLB/Room/DFG/LTC及Review OpticsProbe。当前macOS无上述目标环境，目标实机未执行且无新目标失败；qsb五目标编译不能替代目标GPU。
- 静态入口`python3 tools/audit/audit-rhi-optics.py`，COMMON-01–05/PERF-01–07/平台逐符号及hash；核对D3D/GL clip、float readback、SRB、共享depth、resolve/mips、有限奇点及Review生产隔离。目标真实执行格式/PowerShell解析与shader生成，退役verify-r10b不得恢复旧图像流。
- Release Review ON后执行`ctest --preset qt-release-visible -R '^rhi-shell-gpu$' --output-on-failure`，先按实际能力重评隐藏路径，遵守当轮窗口策略。预期61源状态、6排序/2隔离、重复ID拒绝、法向/深度和投影≤2px、1×/4×alpha中心≤1/255且黑边≤原一整数码值、透明洞不写depth、4×有部分覆盖、十次真实SG重建及窗口策略通过。10数学样本：LTC解析误差≤.001，其他max(1e-6,abs(expected)×.001)，含正视/零光/GGX/DFG/shadow及view-roll0/45的roughness导数；不放宽门槛。
- 两组qmllint/check-format和目标适用Release回归；共享修正回归主力平台完整隐藏集。正常呈现/VSYNC和原生窗口恢复按R34原专项，连续FLICKER-01与BLUR-01仍按R19C2C/D/H在R20前收口，不随此项后置。

#### R19C2C-P · 连续透明阵列目标平台回归

- 依赖R33和可靠Windows/D3D11、Linux/OpenGL目标原生Kit/编译器/窗口环境。输入为R19C2C实现（起点ebfb320）、冻结57f4f845c20cce646a15dbf4008c2a162aaaf4e2映射、原GLB/光学资产、匿名9×64三封面库及rhi-shell/visual.json。本机无目标环境，目标未执行且无新目标失败；本机Metal已修复不代替目标结果。
- 严格静态入口`python3 tools/audit/audit-rhi-flicker.py`逐COMMON-01–05/PERF-01–07/平台符号/hash；展开invariant跨目标qsb、相同world→nativeVP计算、稳定ID/depth顺序、capture隔离、Review probe不入产品和有界异步封面/读回。MSL2.1仅两个geometry顶点，其他shader仍保持原目标；任何Kit更新重新核对实际生成限定符与运行。
- 目标Release Review ON执行`ctest --preset qt-release-visible -R '^rhi-flicker-gpu$' --output-on-failure`，先按当前可靠隐藏能力调整实际入口，遵守窗口契约。1/7/8/9实体×1×/4×、640×480/1280×720双主题静止/波浪/浏览/总览进退16×60帧；480诊断样本missing/nonfinite必须精确0、shade>100，480正常光学保留代表输出实际检查，不恢复旧截图比较。实际DPR/目标/窗口策略、模式、资源generation与上传预算通过；十次SG重建、resize/封面及完整目标适用回归、两组qmllint/check-format通过。
- 共享修正回归主力平台完整隐藏集。原生窗口、真实呈现/VSYNC和不同设备须按本节原专项，不能将缺环境写通过；BLUR-01仍R19D/H主线，R19H核对本机已修闪烁与各目标欠验边界，不把本轮未运行目标销项。
