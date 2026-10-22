# RHI生产接线、同步与遥测

本文负责SceneItem生产输入/失效/生命周期与采集接线；通道实现分别在rhi-opaque/rhi-shell/rhi-postprocess契约，测量指标和采样规则只在[performance.md](performance.md)定义。

## 生产与同步

- `App.qml → MusicStage.qml → SceneItem → SceneAdapter::frame/SelectionLighting/RenderQuality → RhiItem::submit → synchronize/renderFrame`。SceneAdapter独占相机/实体/投影/拾取/focalDepth；QML不维护后端专属状态。生产实体由填充/真实列和有限选中/归位池提供，封面消费者见archive-pool.md。
- 普通构建不链接Quick3D/FixedTextureData，不打包GeometryScene/材质/后处理研究资产；Review才注册ResearchApp及隔离材质入口。几何验证使用生产提交矩阵。
- SceneItem填满实际逻辑viewport，RenderQuality接`effectiveDevicePixelRatio`；原生尺寸与失败语义归occlusion.md。窗口DPR变化合并polish，换窗口断开旧screen/DPR连接。最终Qt item纹理按其逻辑rect合成，各屏幕空间附件与最终输出保持相同物理尺寸。
- 相机/布局/实体实际变化才更新几何revision，输入revision独立。polish合并同帧相机/灯光/画质/封面；相同submission不重复复制。实体ID序列与上个已接受的非空壳体输入相同时复用其唯一性验证；身份变化、空实体或非壳体输入后重新验证。封面owned immutable像素共享，外部Review输入隔离一次，cacheKey变化才交接。
- SceneItem在实际可绘制的polish中按已确定的pool/单盒模式请求对应封面。
- PreparedAssets在首个RhiItem的GUI构造/show前一次预载固定qrc GLB/qsb/PMREM half/lookup，进程内有界不可变；render/同步不读盘或解码。
- 屏幕目标兼容键含尺寸/MSAA/布局；阴影按自身尺寸独立兼容与标脏，封面有独立revision；后处理键含AO尺寸/核/DOF/SMAA/实际附件身份。相机、焦点、主题数值和实体数量不得重建无关目标/PSO/SRB。buffer容量/上传边界只在rhi-postprocess.md维护。
- MusicStage.visualActive统一门控帧源、启动快照、SceneItem.renderingActive及封面队列；隐藏停止新解码准入/polish/提交，有限在途结果只入有界CPU缓存/租约。恢复合并ensurePolished/update并提交最新快照。GPU资源由render释放，错误跨renderer虚函数前捕获并显示errorText；已录命令引用的资源等待正常SG释放，不在错误回调提前销毁。QObject销毁取消回调，旧窗口结果核对epoch。

## 资源统计接线

- resourcePlan按纯值资源键变化计算分配前请求，核对真实QRhi尺寸/格式/采样；ResourceInventory开启遥测后从实际描述符复核，共享附件按身份去重。
- 纹理计完整mip/sample，UnknownFormat明确为0并按D32FS8的8bytes保守上界。记录estimatedCurrentBytes和resizeOldPlusNewUpperBoundBytes；后者仅资源替换对的估算，不是driver延迟回收后的驻留峰值。
- 计入Qt item输出；窗口swapchain/compositor、driver padding/PSO/SRB/buffer轮换与字体图集不可完全观测时逐项注明，measuredVramBytes=null。封面/atlas分阶段计数归covers/archive-pool/rhi-postprocess，不重复相加。

## 输入与采集接线

- SceneAdapter::recordInput记录单调时间/revision，SceneFrame携带真正已提交导航的关联revision；render报告submittedNs。input→状态→Qt命令提交与屏幕显示严格区分。
- cpuSubmitMs只含renderFrame到最后输出命令，synchronizationMs单列最近快照同步；静态复用不解释为每帧重复同步。遥测组装自身开销不混入提交数字，GPU timestamp不可用null。
- 遥测与诊断在隐藏/恢复时重起分段，GUI邮箱回调按各自epoch拒绝旧结果；callbackRevision参与changed-only提交，静止恢复同样刷新render持有的回调。
- 遥测默认关闭；开启时mailbox最多128样本、一个排队GUI回调，溢出丢最旧并计coalescedSamples，丢样不能算统计完整。Review最多40000提交/呈现事件，溢出失败；frameSwapped按实际render事件时间独立记录。
- Review显式capture最多两批在途，纹理身份去重/共享值交付；request保留到对应提交。静态异步读回按MaxAsyncReadbackFrames+1有界续帧，普通产品无回调不增加工作。
- `yalahoo --verify-integration 新目录 --integration-seconds 12..120`按真实事件推进，FrameAnimation仍唯一帧源，无processEvents/msleep轮询。功能边界包括九尺寸/景深变化/双主题，独立压力包括十次完整QML根窗口重建；重建后重新绑定输入/遥测并以epoch拒绝旧回调，核对新renderer首批封面上传与资源回落。当前入口仍捆绑上述工作，仅具体未决问题确需运行时按test-gates.md解开最小分支，不默认执行矩阵/压力或为此单设工具改造任务；短单盒输入不能冒充完整产品。
- `rhi-postprocess-audit --profile`是独立分通道CPU提交诊断，不能混入正常120秒性能样本；范围见rhi-postprocess.md。record-only模式只改性能退出门槛，functionalPassed/samplingComplete/gateEnforced/performanceAcceptance分开，用户历史例外及数值的解释唯一由performance.md维护，不产生默认复现任务。Windows定向包装`verify-r10d.ps1 -Seconds 所需时长`只消费已构建产物，默认一次；显式`-Runs`才调整样本数，不附带其他组件/构建检查。

## 原生同窗口恢复

二维SVG与手写Shape装饰由`SceneGraphVisual`/`ArchiveVectorImage`持有。`SceneGraphLifecycle`将窗口SG失效/初始化排队交GUI，并以窗口epoch拒绝旧通知；只释放和重建绘制装饰，保留按钮、焦点、模型、媒体及原尺寸/CurveRenderer。隐藏无逐帧工作，装饰数量有界，不遍历业务视觉树或重建整个App。

`ui-audit --window-lifecycle 全新目录`只运行生产MusicStage的暂停/播放×隐藏/最小化四次转换；使用qrc封面和一首临时匿名WAV，不生成大封面、不换片、不重建QML根或采样性能。macOS设置`YALAHOO_VERIFY_VISIBLE=1`选择原生窗口；当前轮次授权前台时设置`YALAHOO_VERIFY_FOREGROUND=1`。后端通过`QSG_RHI_BACKEND`选择，Kit插件路径沿native-bootstrap.md。

核对首次曝光、隐藏停止视觉工作而歌曲继续或保持暂停、同一窗口/root/SceneItem恢复当前快照和自身输出、停止清空歌曲及最终资源释放。报告保留每次转换的实际SG失效、隐藏时保留的renderer和窗口策略。Qt的persistency设置为提示：basic循环可保留context/SG，surface销毁不冒充新renderer；删除场景后先在有效context交付清理帧，再隐藏并检查所有拥有的渲染资源归零。实际物理输入、听感、跨屏与默认置底无激活显示仍由计划的独立门槛维护。

## 目标尺度与生产阵列检查

`ctest --preset qt-release-render -R '^rhi-scale-gpu$'`在macOS使用真正隐藏RenderControl；其他平台按test-gates选择native入口。输入为`postprocess/visual*.json`九尺寸和与连续阵列入口共用的匿名9×64库，原生质量与双主题、64核全分辨率AO及SMAA。CTest使用`rhi-scale-audit 新目录 --representative`选择321×241与640×480，核对封面过滤及同窗口DPR转换，供尺寸规则改动后的定向检查。显式RenderControl DPR为1/1.5/2，核对`effectiveDevicePixelRatio`和真实外层target；外部RenderControl target变更DPR不产生原生screen事件，入口在target设置后、polish前投递标准`DevicePixelRatioChange`窗口事件，再由生产信号合并更新；环境变量不能代替该证明。native入口仅报告实际DPR，其余目标矩阵按计划R36分工。

九尺寸布局、主题和DPR边界按test-gates.md分开选择；必要入口调整按test-gates.md，不把九尺寸×双主题×三DPR固定为必跑GPU全组合。奇数尺寸/格式/MSAA、半AO/full DOF/CoC等真实耦合边界仍须独立覆盖。

独立源尺寸公式核对SceneItem/fixed buffer、所有几何/resolve/MSAA/Qt item附件、AO/full DOF及最终输出图尺寸，记录全部资源清单与相等的最终物理尺寸。核对实际独立封面mip/足迹过滤、独立透射mip、后处理color线性/depth nearest及真正上传的focus/aperture/aspect/.011 UV CoC；缩略图与满足当前投影需求的独立租约须就绪。同一640×480窗口DPR 1→2→1.5→1时无resize，重新polish并更新实际目标，图集扩容时更新generation并重传已有不可变像素，尺寸不变时保留generation；就绪后维持零上传。普通产品不增加读回或诊断遍历。

`python3 tools/audit/audit-rhi-scale.py`输出冻结生产调用/最终CSS、D1–D4历史及COMMON/PERF/三平台符号hash矩阵。尺度检查与`rhi-flicker-gpu`所选生产状态的有效报告共用；只补未覆盖的实际附件/合成断言，重复SG重建由具体未决寿命问题决定。连续入口在读取当次输入前执行生产item的ensurePolished并核对SceneFrame revision，避免native queued frameSwapped对应旧帧。自身输出另检查实际轮廓/清晰度/合成，不运行图像分数。CPU提交/driver显存/物理屏幕及完整产品性能的边界见performance与迁移计划。

## 整场GUI重放

`python3 tools/build/verify-gui.py --backend metal|d3d11|d3d12|opengl|vulkan --output 全新目录`运行Review的`ui-audit --gui`。macOS当轮获可见授权时添加`--visible --foreground`运行原生窗口；默认保持隐藏。它复用StartupAudit的生产MusicStage驱动、唯一FrameClock、实际SceneItem与匿名576专辑库，仅使用qrc横/竖/方封面。后端执行分工与默认窗口策略由test-gates.md和迁移计划管理。

源状态集合为九尺寸×双主题×填充/真实列共36状态、180份自身输出；隐藏目标按尺寸轮换代表DPR1/1.5/2，native只记录实际屏幕DPR。执行矩阵按test-gates.md选择；必要的范围选择及几何/完整交互分离按test-gates.md在当前任务内处理。关键输出覆盖浏览/总览/详情/搜索，填充模式另含二维/三维片头；代表状态以自身Qt键鼠事件核对正反导航、总览进退、详情打开/返回、连选最后意图、搜索模态及浏览播放隔离。详情须等入场透明度达到1，完全遮挡期检查clear提交语义，不能要求重新上传被遮挡的三维快照。运动数值、CSS结构、连续渲染质量分别验收。

报告保留实际viewport/DPR、有限矩阵、实体/清晰封面容量、renderer代次、资源描述符与旧新重叠估算、独立GUI/同步/提交成本和最终释放；显式读回发生在帧完成后的检查点，不计入帧成本。manifest保存源码/资产/二进制/图像hash和失败退出状态，不覆盖已有目录。此入口没有原生呈现、物理输入或显存实测，不产生performance.md之外的性能任务。

`--representative`选择1280×720、930×900和390×844，保留双主题/两阵列及全部联动，输出60个检查点，用于组件布局结果仍有效时补后端产品接线。`--narrow`只重放≤700逻辑px的尺寸，另核对长列名界限与单专辑恢复单行，用于小屏布局改动后的定向回归；两种选择互斥，原完整矩阵中的未受影响状态按test-gates.md复用。`python3 tools/audit/audit-gui.py --report 完整目录/matrix/report.json --report 后续定向目录/matrix/report.json`按输入顺序覆盖同状态/阶段记录，要求合并后完整矩阵齐全且各报告自身通过，然后复核各组件唯一来源审计，并输出本地COMMON/PERF/三平台调用链矩阵。正常输出由实际查看检查清晰度、轮廓、裁切和合成，不生成图像评分；源码数学和结构仍使用各组件既有导出/比较入口。
