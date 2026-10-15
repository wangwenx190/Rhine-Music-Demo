# 阴影、SSAO 与画质合同

冻结提交为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。参数来源、Three 算法及资源 hash 在 `resources/manifests/occlusion.json`；生产调用链为 `scene.ts` 构造 → `enableSelectionLighting` → `softenMusicContactShadows` → `setQuality`（换 kernel 后再次 soften）→ `resize` → `update/composer.render`。构造值 0.38/0.44、0.09 不覆盖音乐入口的最终值。

- 生产 SSAO：kernelRadius=0.18、minDistance=0.001、maxDistance=0.035；距离判定沿 Three 的 orthographic depth，不能把它们当作世界单位。原始/高为 32 个样本，极高 64，性能为 0。kernel/noise 用 seed=417 的 LCG，调用锁定的 SSAOPass/SimplexNoise 生成；16/32/64 分别重置 seed，资源 RGBA32F little-endian。
- 对所有可见盒体壳体和完整纸面做不透明几何覆盖，等价于 MeshNormalMaterial 的输入语义；纸面的透明 contain 留白在此覆盖中仍是完整平面，与源 override 一致。复用实际模型、同一实体 origin/邻盒位置和 SceneAdapter 相机，不用主通道的仅不透明 depth 代替玻璃几何。阵列池由后续 R12 接入。
- `OcclusionPrepass` 的公开 `DEPTH_TEXTURE` 为硬件 [0,1] 深度；`NORMAL_ROUGHNESS_TEXTURE` 的 RGBA16F xyz 是有符号世界法线，先转到 view space 再 pack RGB。不能重复做 `*2-1`。两个独立几何预通道分别输出 view normal RGB 和 RGB24 编码的线性距离，alpha 固定1；Effect 输出请求 RGBA32F，但公开 View3D texture provider 的最终颜色链接可能量化到 RGBA8，不能依赖该请求或 Quick layer 格式保留深度。RGB24 将线性距离转为三个整数字节，nearest 采样后解码，消费侧按相机 near/far 还原 viewZ；raw depth 探针再还原 hardware depth。float32在最大24bit整数处的进位须在取整后再clamp；当前白色远平面按存储RGB24精确哨兵排除，旧1-ulp容差由R19D1移除。Windows raw depth/normal 对照独立验证该链路。Qt 管理的原始 depth 具体格式没有公开查询接口，不将请求的格式冒称为实测底层格式；R10 如需精确控制由显式目标接管。
- 透视重建使用源矩阵的标量等价式：`viewZ=n*f/((f-n)*depth-f)`，`xy=ndcXY*tan(fov/2)*(aspect,1)*(-viewZ)`。FOV/宽高来自同一 View3D/SceneAdapter；仅用 FRAMEBUFFER_Y_UP 修正 framebuffer 行方向，不给硬件 [0,1] 深度再套 GL clip-space 修正。只支持当前合同的标准透视相机。
- SSAO 按原半球 kernel、标量 noise 的 xyz 复制、TBN、严格 `minDistance < delta < maxDistance`、平均采样比例执行；模糊为对称 5×5 box；乘数 `mix(1,blur,0.22)`，下限 0.78。没有景深或 SMAA。
- 主 View3D 渲染目标使用 RenderQuality 的有效尺寸；AO/模糊为 RGBA16F、aoResolution 倍率，法线与深度几何覆盖均使用同一 AO 尺寸。BufferInput 与 TextureInput 属性由 Qt 生成 binding，禁止手工指定运行时 binding。性能档卸载预通道，0 samples 不执行采样；用户参数不因限流被改写。
- `RenderQuality` 移植 `render-quality.ts` 的四预设、归一化与 renderDimensions：scale=50–200、步长5；DPR cap=1/1.5/2/3；shadows=0/1024/2048/4096；AO=0/16/32/64，倍率 .5/.75/1；其余已保留的 DOF/AA/透射/各向异性字段由后续对应轮次消费。全分辨率 ≤8,294,400 像素。当前公开场景采用保守 4096 单边上限，并公开 limited；R10 后端应供应实际设备上限。stageScale 沿源窗口尺寸公式，Review 同输入场景显式使用1。质量变更不改变相机、导航/播放或业务对象。
- 关键灯方向来自 (-6,14,-5) 指向原点；仅 Optical_Diffuser 投射阴影，壳体/纸面接收。公开 DirectionalLight 配置为 shadowFactor=32、按预设 map size、far=45、PCF16、pcfFactor=4、32bit map。其亮度不叠加到已有自定义灯光；纸面 DIRECTIONAL_LIGHT 只取得 SHADOW_CONTRIB，POST_PROCESS 仅减去遮挡的 key diffuse。Unshaded 透射壳体无法直接消费这个公开 hook；投影/偏置/滤波和壳体接收限制按下述文件交 R10。

通道：0 合成、1 raw AO、2 blur、3 hardware depth、4 view normal、5 linearColor。所有 raw 通道跳过最终 sRGB，便于诊断数值。生产输出继续使用 R07 的输出约定；背景 OutputPass 的 ACES 差异由 R09B 收口，不能在本轮用调色补偿。所有诊断相机/半径/接收平面只通过 Review fixture 注入；普通产品拒绝 `--verify-materials`。

重建：`tools/build/verify-r09.ps1`（已保存 Web 同输入时可加 `-SkipWebCapture`）。只执行 Release，自动选最新 Kit。证据为 `verification/windows/occlusion/`，成对图 `docs/media/occlusion/{web,qt}/000–087.png`；实际尺寸/DPR/后端/帧数/ROI/source参数与 fixture hash 在 release-runtime.json。80 个生产参数状态覆盖双主题、两角度、单盒/三盒、四预设；080–085 是明确标记的 .1/100、radius=.38 算法压力探针；086–087 是单独阴影乘数平面开关探针。这些额外诊断不改变生产 5/300、radius=.18。

当前后续输入在 `docs/contracts/occlusion-limitations.json`；不将该输入当作已经选择 RHI 或已经完成 R10/R09B。macOS/Linux 新增链路审计见各自 `verification/*-static/occlusion/`，结论仅为“静态审计通过，实机待用户验证”。

图像门槛：raw depth/normal 与 raw AO/blur 的整图 MAE 均小于0.05/255，源白色远平面保持白色；单盒四个材质ROI MAE≤6/255、有效SSIM≥.97。背景与邻盒保留完整五ROI指标，由限制清单指定后续验收；本轮通过不代表完整画面或阴影滤波已达到最终门槛。


## 当前SSAO源数学与读回门禁

R19D1复审结论见[source-fidelity-review.md](source-fidelity-review.md)。当前生产RHI raw/blur仍为R16F，源normal只解码、不再额外归一化，背景按RGB24精确white判断。固定417核、完整16/32/64样本、严格min/max、5×5 box和.22乘数保持；无GPU字节/旧图匹配。上文R09图像分数与Windows路径为历史事实，不是当前执行门槛；完整旧GPU工具由R19D4拆解。

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release-render -R '^rhi-ao-gpu$' --output-on-failure
python3 tools/audit/audit-rhi-ao.py
python3 tools/audit/bake-occlusion-shaders.py
```

入口使用27个奇偶宽/三倍率/三核背景状态、243个接触数值点及5×5/.22公式、真实格式/缓存复用与十次SG重建；macOS默认真正隐藏，其他系统按实际窗口入口及契约执行。OpenGL R16F奇数宽读回按实际pack对齐同格式复制到缓存宽纹理后解码，产品目标无额外padding；这修复诊断读回，不能把原坏字节当作产品AO非有限。生产未分配无用kernel texture，常量核只上传一次，80-byte动态前缀保留1104-byte块布局；三个AO pass的常态API更新请求240字节，后端实际传输未知。所有正常效果、画质/业务状态不变。
