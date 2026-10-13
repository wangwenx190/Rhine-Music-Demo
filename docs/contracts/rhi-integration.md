# RHI 生产场景集成契约

生产调用链为 `App.qml -> SceneItem -> SceneAdapter::frame()/SelectionLighting/RenderQuality -> RhiItem::submit -> Renderer::synchronize/renderFrame`。`SceneItem` 是 QML 中唯一的三维场景入口；相机、导航、`projectWorldPoint`、`hitTest`、`focalDepth` 和不可变 `SceneFrame` 仍归 SceneAdapter 所有。QML 不识别 D3D11/Metal/OpenGL，也不根据后端维护另一份相机或实体状态。当前实体仍为既有匿名单盒，阵列、波浪、完整详情和封面服务分别由后续轮次接入。

生产 CMake 不链接 Quick3D，不注册 FixedTextureData，不打包 GeometryScene、Quick3D 材质/后处理 QML 或其 mesh/QML 资产。`YALAHOO_REVIEW=ON` 才加入 `ResearchApp.qml` 和历史 Quick3D oracle；仅 `--verify-materials` 加载它。`--verify-scene` 使用生产 SceneItem 的提交矩阵及真实 RHI 轮廓。旧研究源码和资产保留在原路径，历史报告不改写。

## 同步、失效与生命周期

SceneAdapter 只在相机、布局或实体位置实际变化时重建矩阵和发布几何 revision；输入 revision 单独记录。SceneItem 用 Qt polish 合并同帧的相机、灯光和画质通知，发布与已提交内容相同时不重新提交。身份和矩阵的双精度计算顺序保持原样；一次内容变化只产生一次不可变值交接。Renderer 对相同 submission 不重复复制快照；封面仍按 cacheKey 拥有一次像素副本，后续复用，图集保持既有 generation/lease/serial 队列。

`PreparedAssets` 在首个 RhiItem 的 GUI 构造阶段、首次 show 前预载固定 qrc 资产，解析 GLB、反序列化 qsb、转换确切的 PMREM half 数据和解码 SMAA lookup。每个进程仅有一份有界不可变缓存，场景图重建不重新读盘/解码；render/synchronize 只引用 Qt 隐式共享值。这里仅处理固定内部资产，不是 R11 的异步用户封面实现。

渲染资源由 render 线程独占。附件兼容性键为输出尺寸、阴影尺寸、封面页面尺寸/generation、MSAA、壳体/诊断布局；后处理另使用 AO 大小/核、景深/SMAA 和输入附件身份。相机、焦点、主题数值、实体数量不改变附件键，uniform 和实例容量固定为 `uniformStride*(432*3+6)` 与 `432*112`。阴影 caster/开关未变时复用；局部 upload 和有效前缀保持 R10C 契约。

GPU 不支持所请求格式、采样或尺寸时显式报错，SceneItem 的 `errorText` 在界面显示，设置不偷偷改为低画质。错误穿过 Qt renderer 虚函数前被捕获；已记录命令引用的资源不在错误回调中销毁，等待 Qt 正常场景图释放。隐藏时生产 FrameAnimation 停止；恢复继续遵守测试窗口标志，窗口/renderer 的销毁取消 QObject 回调。Review 在途读回最多两个，待采集目标保持最新 request，普通产品没有捕获回调。Review 读回只用于诊断；普通产品没有 readback 或 GPU finish 热路径。

## 累计资源清单

AO raw/blur 仅保存唯一 half 标量，消费者只读 `.r`，原重复 RGB 与恒定 alpha 在诊断读回重建；与 RGBA16F 对照逐 PNG 字节验证，采样数、滤波、精度、分辨率和效果数学均不变。1920×1080 的两份 AO 附件载荷减少 24883200 bytes；这是描述符载荷节省，不是 FPS 或显存实测。

分配前 `resourcePlan` 汇总实际请求的颜色附件、MSAA、resolve/mips、共享 depth、阴影、封面/图集、独立 AO/DOF 几何、后处理 ping/pong、SMAA、lookup、几何/uniform/实例 buffer 和 Qt RHI item 输出。纯值资源键变化时才查询设备能力和计算计划，稳定帧不拼装键字符串或重复查询驱动；尺寸/采样/格式检查以当前 QRhi 设备为准，不编造跨设备显存门槛。

开启遥测后 `ResourceInventory` 从实际已创建的描述符复核；同一共享深度附件只记一次，纹理按尺寸×格式字节×sample count 加完整 mip 链。QRhiRenderBuffer 的 `UnknownFormat` 明确记录为格式 0，并使用 D32FS8 的 8 bytes 保守上界。报告 `estimatedCurrentBytes` 和前后计划同时存在的 `resizeOldPlusNewUpperBoundBytes`。后者是资源描述符替换对的估算，不是驱动延迟回收后的驻留上界。资源对象的释放路径与十次真实场景图重建同时检查。

GPU descriptor 载荷不等于物理显存。driver padding、内部 PSO/SRB、buffer 后端轮换、Qt 窗口 swapchain 和系统 compositor 不在 item API 中完整可观测，报告逐项注明限制，`measuredVramBytes=null`。Qt item 输出已计入；窗口颜色存储可另按实际物理尺寸×4×平台 swapchain buffer 数计算，但未读取真实 buffer 数时不得编造累计实测。后续 R11/R28/R30 新增资源必须扩展这份清单。

## 输入与帧测量

`SceneAdapter::recordInput` 在 QML 动作入口记录单调时钟和递增 input revision；随后生产 SceneFrame 包含它。Renderer 报告对应 frame revision、input revision 和 `submittedNs`，测量范围是输入回调收到到 Qt RHI 命令提交完成，包含等待下一帧和 CPU 准备，不是 input-to-photon。`cpuSubmitMs` 从 renderFrame 开始至最后输出命令结束，`synchronizationMs` 单列最近一次快照同步耗时（静态复用帧可沿用该值，不解释为每帧都做了同样同步）；遥测/资源清单自身的组装不计入 CPU 提交数字。GPU timestamp 无法取得时为 null。`rhi-postprocess-audit --profile` 仅在独立 30 秒分通道诊断启用实际 GPU timestamps；该组数据不混入正常 120 秒验收样本。

遥测默认关闭。开启时采用最多 128 个样本的有界批量 mailbox和最多一个排队 GUI 回调，overflow 丢最旧样本并累计 `coalescedSamples`；采集存在丢样则不能判定统计完整。Review 原始序列最多 40000 个提交/呈现事件，溢出使检查失败。`frameSwapped` 用 render 线程的实际事件时间独立记录，报告分阶段 p50/p95/p99、一/两/三周期计数及连续长帧，不将它解释为屏幕扫描或 compositor 延迟。

`--verify-integration <directory> --integration-seconds <12..120>` 仅在 Review 构建启用。驱动通过 Qt 精确定时器发布一次 QML 点击；resize/画质/重建逐步等待对应实际提交的确认，再发布下一步，避免长采样末尾粗粒度定时器同时到期而乱序。FrameAnimation 仍是唯一生产帧源，采集不用轮询/processEvents/msleep。验证双主题的浏览/详情、九个实际 client 尺寸、四档画质及十次 hide/release/restore；窗口第一次 show 前使用 VerificationWindow 置底、不接受焦点、无边框，PNG 来自自身 grabWindow。

固定长样本由 `tools/build/verify-r10d.ps1` 串行执行三次，每次保留四个阶段的原始提交/呈现时间序列；另执行本机 OpenGL 实际探测。R06–R10 的 CPU、GPU、五 ROI/投影及 DPR 1/1.5/2 回归和 24 组 qsb 变体也保存在新目录。D3D11 为 Windows 默认、Metal 为 macOS 默认、OpenGL 为 Linux 默认；`QSG_RHI_BACKEND=opengl` 可显式选择 OpenGL，未知/不适用的覆盖明确失败。

证据在 `verification/windows/rhi-integration/`，图片在 `docs/media/rhi-integration/`，对应平台静态矩阵在 `verification/*-static/rhi-integration/`。历史 R10B/C 残差依原 sourceIndex/ROI 复验，不将 summary.passed 等同于所有 ROI 达到 .97/6。完整阵列/搜索/扫描/抽取产品未实现，其 120 秒混合交互和真实 input/显示预算、噪声验收仍由 R28 完成；单盒集成数据只能解释已实现路径。macOS/Linux 结论为“静态审计通过，实机待用户验证”。

## 本轮性能记录边界

按用户明确要求，R10D 性能实测为记录项，20ms 保留参考值，不阻止本轮完成和提交。结构/算法、Windows 功能/视觉、上传、资源与生命周期仍为门禁；正常 VSYNC 三次样本及 OpenGL 实机探测必须完整。已有受控吞吐 p95=18.35ms 和 20.98ms 分开保存，后者仍保留原 passed=false，不改写成数值达标，不继续为过线采样。复核后的其他 GPU 占用只作为环境记录，不能反推为已证明的超限原因。

复建脚本在 Review 配置显式设置 YALAHOO_PERFORMANCE_RECORD_ONLY=ON，CTest 使用 --record-performance；工具仍执行完整视觉/上传/资源检查并保留性能原值和原数值比较结果。functionalPassed、samplingComplete、gateEnforced 与汇总的 performanceAcceptance 分开说明范围，普通 Review 默认仍保留历史数值门槛。生产配置恢复该选项为 OFF。record-only-validation 仅验证新增记录模式的完整输出/退出语义，不用于挑选更好的性能样本。R28 用同设备/Kit/fixture 解释这些波动并建立完整产品预算和噪声依据。
