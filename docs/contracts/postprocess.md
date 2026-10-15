# 景深、抗锯齿与最终输出

冻结基线为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。算法和有效参数从 `legacy-web/src/scene.ts` 的初始化、`setQuality`、逐帧 focalPoint 更新，以及锁定 Three 依赖的 Bokeh/SMAA/Output shader 解析；来源与 lookup 字节 hash 在 `resources/manifests/postprocess.json`。截图只用于定位差异。

## 合成与色彩

生产顺序是线性不透明捕获、线性磨砂壳体合成、SSAO、Bokeh、按设置 SMAA、最终 ACES exposure、精确 sRGB。`SceneEnvironment.TonemapModeNone` 禁止 Qt 再次色调映射。`GlassMaterial.compositorOutput` 在生产通道中输出未 tone-map 的 radiance；孤立 R07/R08 直接材质审阅仍保留直接渲染的 ACES 路径。材质的曝光不在生产壳体中提前应用。背景保持源码颜色的线性值，不能用主题改色补偿输出差异。

Three 的 RenderPass 写入线性浮点目标时，材质级 tone mapping 不执行；`toneMapped=false` 纸面限制仍在纸面入射量/印刷处理处保留，整个 composer 的 OutputPass 对最终图像执行一次 ACES 和一次 sRGB，包括纸面与背景。Qt 对应 `material-output.frag`，raw AO/normal/depth/linearColor 审阅通道绕过全部输出转换。QML 文字和阅读遮罩在 View3D 之后叠加。

raw linearColor 诊断显式追加 identity ShaderPass，把 RenderPass 留在 composer 目标中。只有一个 RenderPass 时，EffectComposer 会把它变为 renderToScreen，导致材质 ACES/sRGB 已执行；R09 的 channel=5 图不能当成真实线性输入。本轮保存新的明确线性诊断，不以旧采集标签覆盖源码执行路径。

## 景深

初始化值为 focus=25、aperture=.0018、maxblur=.011；有效帧值为 focus=相机 view 空间中 `entityOrigin+(0,2,0)` 的正深度、aperture=`(.0003+.0005*detail)*depthOfField/100`。`SceneAdapter::focalDepth` 提供 C++ 数值核对；QML 使用相同相机位置/aim 的点积并直接依赖通知。只读取已计算的实体和相机状态，不建立新的时钟或阻尼。

`bokeh.frag` 保留冻结 BokehShader 的 41 次采样、三组 .9/.7/.4 环、aspect 修正、正负模糊量和 ±.011 限幅。独立全分辨率 `OcclusionPrepass` 用不透明壳体覆盖材质写入 RGB24 线性深度，覆盖实际玻璃几何，而不误用只含不透明主通道的 DEPTH_TEXTURE。深度提供者在后处理启用期间保持身份；隐藏且未被启用 Pass 采样时不提交该深度视图。SSAO 的分辨率和深度通道独立。

## SMAA 与固定通道布局

高/极高使用锁定 Three 的 SMAA 1x Medium：颜色阈值 .1、最多 8 步搜索、160×560 area 与 66×33 search lookup、原 neighborhood blending 的 gamma 2.2 规则。lookup 从已有 SMAAPass 内嵌 PNG 导出；运行时由 `FixedTextureData` 加载 qrc，按源码 `flipY=false` 保持 PNG 解码行序的 RGBA8，不做 sRGB 转换、不生成 mip。framebuffer UV 与 canonical WebGL UV 的转换仅作用于帧缓冲采样，lookup 不随图形后端翻转。

`PostprocessEffect.qml` 提供仅景深、仅 SMAA、景深→SMAA 三套固定实例。组合在同一个 Effect 内经具名 RGBA16F 景深中间目标、与源码相同的 RGBA16F edges/weights，再输出 HDR；最后一个 Pass 必须输出到 Effect 最终目标，不能把具名中间 Buffer 当作最终结果。预设只选择固定实例，不改变正在使用的 Pass.output/BufferInput。此公开 API 组织方式避免 Windows 实机发现的多 Effect 串联/动态 Pass 重定向崩溃，不依赖 Qt 私有类型。图像算法、顺序和设置含义保持一致。

## 几何抗锯齿与预算

原始画质使用 View3D offscreen MSAA High，实际作用由 NoAA/MSAA 成对 GPU 图和斜边覆盖样本验证，不能只读属性声明。原始设置 `antialias=off` 表示不开后期 SMAA，不关闭原生几何抗锯齿。SSAA 仅作为 Review 的捕获作用范围诊断，产品默认仍为 MSAA；SSAA 额外内部尺寸不作为用户预设，也不能用于绕过 8.3MP 目标预算。所有生产后处理目标共享 `RenderQuality.targetSize`，用户设置与设备限制分开。

纸面留白始终使用原 contain、1/64 内缩、采样钳制和 alphaTest；盖内局部 LOD 不变。075 使用原 R07 的精确相机、主题、clarity 和直接材质路径，避免混用生产灯光/composer 条件。公开屏幕透射捕获的边界残差和非边缘 ROI 超目标项必须保留完整指标与局部放大图，作为 R10 的具体输入，不能以整体模糊或修改几何/封面/光照掩盖。

## 历史重建与验收资产（旧图像门禁停用）

Windows：`./tools/build/verify-r09b.ps1 -Python <可用的 Python 可执行文件>`。已有同 hash 的冻结图时可加 `-SkipWebCapture`。脚本只构建/运行 `qt-release`，自动发现最高原生 Kit，Review 与 production 都执行 CTest/qmllint；production 拒绝 Review CLI。Node/浏览器只用于隔离基线采集，Qt 产品运行不依赖它们。

105 个状态包含九尺寸×双主题的 NoAA/MSAA/SSAA、四预设×三种封面、前后景深开关、SMAA 开关、final/raw 输出、精确 075 和真实 DPR 1/1.5/2。Qt 窗口与源码相机共用 fixture，固定四个提交帧后抓取；C++ 比较器保留五个等权 ROI 的 RGB MAE/11×11 Gaussian SSIM、全图差分、投影、焦距与资源尺寸。geometry AA、Bokeh、SMAA 均有实际像素开关检查。业务状态保持由 RenderQualityTest 检查。

Windows 实机证据在 `verification/windows/postprocess/`，全图、差分与局部放大图在 `docs/media/postprocess/`。macOS/Linux 的新增调用链、资源、shader 与错误路径审计在各平台 `postprocess/`：**静态审计通过，实机待用户验证**。完整部署与平台运行仍按后续 R30–R32 执行。

## R19D4A 当前 Bokeh 门禁

Bokeh生产算法未发现需要补偿的源码差异：focus取真实view矩阵中的`entityOrigin+(0,2,0)`，aperture按detail与强度更新，RGB24深度线性恢复near/far；41点、正负CoC/±.011、aspect、线性颜色采样及alpha=1均保持。`export-bokeh-kernel.py --check`核对锁定Three文件的既有manifest hash、公共/生产shader数学体及最小41点测试输入；普通构建和测试只读提交的JSON，不依赖Node或node_modules。

Release Review ON下执行`ctest --preset qt-release-render -R '^rhi-bokeh-gpu$'`。入口直接创建真实QRhi offscreen frame，运行生产Postprocess与同一qsb，不创建swapchain/可见窗口；Windows/D3D11、macOS/Metal、OpenGL由本机能力选择，GL fallback surface保持到QRhi销毁之后。36组包含129×73、257×193、193×257、focus20/25/30、aperture0/.0003/.0008/.0012，另十次真正Postprocess reset/recreate以near=.1/far=1000验证恢复。解析HDR色/不连续RGB24深度以独立double双线性及锁定源41点求和核对全部输出，包含焦平面、前后景、限幅及clamp-to-edge。颜色公式采用既有3/255绝对门槛，不从实机图评分或调参；最终alpha必须精确1且值有限。自身预览是线性HDR裁到显示范围的诊断图，不是最终tone-map画面。

Bokeh-only实际资源为一个全尺寸RGBA16F ping目标及一个uniform，常态API前缀上传80 bytes；焦点/aperture变化不得重建，关闭后目标全部释放。`Postprocess::submittedBokeh`仅Review保存真正交给updateDynamicBuffer的前缀，显式request才序列化；生产不增加采集、缓存或逐帧JSON。`rhi-flicker-gpu`的1200帧真实生产回放进一步对上传focus/aperture/maxblur/near/far/aspect及绑定的完整DOF附件尺寸核对，覆盖详情/相机变化与原光学连续门禁。

本节不恢复历史postprocess/occlusion/RenderBackend/RhiPostprocess原图入口；AA与旧门禁覆盖映射属R19D4B，最终窗口/DPR/render scale/BLUR-01属R19D4C。Postprocess资源重建不等于Qt scene-graph或原生窗口恢复。正式性能/正常VSYNC及目标设备按原R28/R34任务；当前实际运行结论见source-fidelity-review.md。
