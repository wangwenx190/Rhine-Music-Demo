# RHI生产接线、同步与遥测

本文负责SceneItem生产输入/失效/生命周期与采集接线；通道实现分别在rhi-opaque/rhi-shell/rhi-postprocess契约，测量指标和采样规则只在[performance.md](performance.md)定义。

## 生产与同步

- `App.qml → MusicStage.qml → SceneItem → SceneAdapter::frame/SelectionLighting/RenderQuality → RhiItem::submit → synchronize/renderFrame`。SceneAdapter独占相机/实体/投影/拾取/focalDepth；QML不维护后端专属状态。生产实体由填充/真实列和有限选中/归位池提供，封面消费者见archive-pool.md。
- 普通构建不链接Quick3D/FixedTextureData，不打包GeometryScene/材质/后处理研究资产；Review才注册ResearchApp及隔离材质入口。几何验证使用生产提交矩阵。
- SceneItem填满实际逻辑viewport，RenderQuality接`effectiveDevicePixelRatio`；尺寸/scale/保护规则归occlusion.md。窗口DPR变化合并polish，换窗口断开旧screen/DPR连接。最终Qt item纹理按其逻辑rect合成，源画质缩放或像素保护导致的比例须显式记录。
- 相机/布局/实体实际变化才更新几何revision，输入revision独立。polish合并同帧相机/灯光/画质/封面；相同submission不重复复制。封面owned immutable像素共享，外部Review输入隔离一次，cacheKey变化才交接。
- SceneItem在实际可绘制的polish中按已确定的pool/单盒模式请求对应封面。
- PreparedAssets在首个RhiItem的GUI构造/show前一次预载固定qrc GLB/qsb/PMREM half/lookup，进程内有界不可变；render/同步不读盘或解码。
- 目标兼容键含尺寸/阴影/MSAA/布局，封面有独立revision；后处理键含AO尺寸/核/DOF/SMAA/实际附件身份。相机、焦点、主题数值和实体数量不得重建无关目标/PSO/SRB。buffer容量/上传边界只在rhi-postprocess.md维护。
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
- `yalahoo --verify-integration 新目录 --integration-seconds 12..120`按真实事件推进，FrameAnimation仍唯一帧源，无processEvents/msleep轮询。功能边界包括九尺寸/四画质/双主题，独立压力包括十次完整QML根窗口重建；重建后重新绑定输入/遥测并以epoch拒绝旧回调，核对新renderer首批封面上传与资源回落。计时、功能与压力的实际分支拆分归计划R34-T1，不能在每次计时中重复完整矩阵；短单盒输入不能冒充完整产品。
- `rhi-postprocess-audit --profile`是独立分通道CPU提交诊断，不能混入正常120秒性能样本；范围见rhi-postprocess.md。record-only模式只改性能退出门槛，functionalPassed/samplingComplete/gateEnforced/performanceAcceptance分开，用户历史例外与18.35/20.98ms待复现由performance契约和R34维护。

## 目标尺度与生产阵列检查

`ctest --preset qt-release-render -R '^rhi-scale-gpu$'`在macOS使用真正隐藏RenderControl；其他平台按test-gates选择native入口。输入为`postprocess/visual*.json`九尺寸和与连续阵列入口共用的匿名9×64库，原始画质与双主题，另含50/125/200 scale、半分辨率AO及高画质SMAA。显式RenderControl DPR为1/1.5/2，核对`effectiveDevicePixelRatio`和真实外层target；外部RenderControl target变更DPR不产生原生screen事件，入口在target设置后、polish前投递标准`DevicePixelRatioChange`窗口事件，再由生产信号合并更新；环境变量不能代替该证明。native入口仅报告实际DPR，其余目标矩阵按计划R34/R35分工。

九尺寸布局、主题和DPR边界按test-gates.md分开选择；实现入口的范围选择器归计划R34-T3，不把九尺寸×双主题×三DPR固定为必跑GPU全组合。奇数尺寸/格式/MSAA、半AO/full DOF/CoC等真实耦合边界仍须独立覆盖。

独立源尺寸公式核对SceneItem/fixed buffer、所有几何/resolve/MSAA/Qt item附件、AO/full DOF及最终输出图尺寸，记录全部资源清单、limited和最终缩放。核对实际线性/clamp封面无mip、独立透射mip、后处理color线性/depth nearest及真正上传的focus/aperture/aspect/.011 UV CoC；256缩略图与1024独立租约须就绪。同一640×480窗口DPR 1→2→1.5→1时无resize，重新polish并更新实际目标，图集generation与稳态零上传保持。普通产品不增加读回或诊断遍历。

`python3 tools/audit/audit-rhi-scale.py`输出冻结生产调用/最终CSS、D1–D4历史及COMMON/PERF/三平台符号hash矩阵。尺度检查与`rhi-flicker-gpu`的连续静止/波浪/浏览/总览进退/详情输出、组合资源及十次SG重建共同覆盖生产闭包。连续入口在读取当次输入前执行生产item的ensurePolished并核对SceneFrame revision，避免native queued frameSwapped对应旧帧。自身输出另检查实际轮廓/清晰度/合成，不运行图像分数。CPU提交/driver显存/物理屏幕及完整产品性能的边界见performance与迁移计划。

## 整场GUI重放

`python3 tools/build/verify-gui.py --backend metal|d3d11|d3d12|opengl|vulkan --output 全新目录`运行Review的`ui-audit --gui`。macOS当轮获可见授权时添加`--visible --foreground`运行原生窗口；默认保持隐藏。它复用StartupAudit的生产MusicStage驱动、唯一FrameClock、实际SceneItem与匿名576专辑库，仅使用qrc横/竖/方封面。后端执行分工与默认窗口策略由test-gates.md和迁移计划管理。

源状态集合为九尺寸×双主题×填充/真实列共36状态、180份自身输出；隐藏目标按尺寸轮换代表DPR1/1.5/2，native只记录实际屏幕DPR。执行矩阵按test-gates.md选择；范围选择及几何/完整交互分离归计划R34-T5。关键输出覆盖浏览/总览/详情/搜索，填充模式另含二维/三维片头；代表状态以自身Qt键鼠事件核对正反导航、总览进退、详情打开/返回、连选最后意图、搜索模态及浏览播放隔离。详情须等入场透明度达到1，完全遮挡期检查clear提交语义，不能要求重新上传被遮挡的三维快照。运动数值、CSS结构、连续渲染质量分别验收。

报告保留实际viewport/DPR、有限矩阵、实体/清晰封面容量、renderer代次、资源描述符与旧新重叠估算、独立GUI/同步/提交成本和最终释放；显式读回发生在帧完成后的检查点，不计入帧成本。manifest保存源码/资产/二进制/图像hash和失败退出状态，不覆盖已有目录。此入口没有原生呈现、物理输入或显存实测，不替代performance.md的正式产品矩阵。

`--narrow`只重放≤700逻辑px的尺寸，另核对长列名界限与单专辑恢复单行，用于小屏布局改动后的定向回归；原完整矩阵中的未受影响状态按test-gates.md复用。`python3 tools/audit/audit-gui.py --report 完整目录/matrix/report.json --report 后续定向目录/matrix/report.json`按输入顺序覆盖同状态/阶段记录，要求合并后完整矩阵齐全且各报告自身通过，然后复核各组件唯一来源审计，并输出本地COMMON/PERF/三平台调用链矩阵。正常输出由实际查看检查清晰度、轮廓、裁切和合成，不生成图像评分；源码数学和结构仍使用各组件既有导出/比较入口。
