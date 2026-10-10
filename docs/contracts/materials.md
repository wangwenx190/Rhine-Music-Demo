# 单盒材质契约

冻结基线为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。主依据为 `music-model.ts` 的 configure/theme/clarity/print 函数及 `appearance.ts`、`scene.ts` 的最终有效覆盖，来源 hash 在 [materials.json](materials.json)。单盒验收独立于全场动态灯光、阴影、景深、阵列、片头与播放。

## 材质与空间

`scene/CaseMaterials.qml` 持有三个共享壳体材质、一个漫反射纸面及共享 RoomEnvironment/DFG 纹理；`GlassMaterial.qml` 以 surface=0/1/2 区分主盖/侧脊/后托。导入的三批次 mesh 没有改动。纸面是 3.06×3.06、中心 (0.17,1.85,0.027) 的独立平面，随同一个盒体位移，后托前面 -0.032、盖背面 .052、盖前面 .070 不共面。

| 表面 | 基础→暖昼透射 | 粗糙度 | 厚度 | 衰减距离 | clearcoat / 粗糙度 |
|---|---|---|---|---|---|
| 主盖 | .96→.94 | .34→.40 | .018→.022 | 5→3.8 | .08 / .38 |
| 侧脊 | .88→.84 | .25→.30 | .06→.08 | 4.5→3.2 | .16 / .20 |
| 后托 | .80→.70 | .38→.46 | .038→.05 | 4.5→2 | .08 / .38 |

IOR 固定 1.46，金属度为零，透明度为一，单面透射；侧脊独立计算密度与高光。详情只使主盖粗糙度趋向基础 .27 / 暖昼 .34，侧脊和后托不跟随。数值 warmth/clarity 都是连续输入，实际主题动画按 [灯光契约](lighting.md) 接入同一个 FrameClock。参数后乘共享标定系数，生产系数保存在 QML，扫描与原始参数分别保存于 `verification/windows/materials/calibration.json` 与 fixture；不调整 IOR、颜色、封面或几何。

暖昼表面色为 #fffdf8 / #e7e1d8 / #ece6dc；夜间为 #f6fbff / #f6fbff / #c6d6e5。暖昼衰减色 #f3e9db，夜间主盖/侧脊 #dceafd；后托保留原衰减色。QML color 作为线性 shader uniform，颜色插值发生在线性 shader 中。生产调用下，Three 在材质 envMap 为空时把场景 environmentIntensity 覆盖到 uniform；因此有效强度为暖昼 .48 / 深夜 .68，而不是再乘 configureMusicGlass 的 .65。

主盖局部 `d=max(abs(localXY-(.17,1.85))-(1.53,1.53))`，`p=1-smoothstep(.015,.15,d)`，纸面 LOD 因子为 `mix(1,.16,p)`。局部位置由同一 MODEL_MATRIX 关联实体；SCREEN_MIP_TEXTURE 的粗糙 LOD 乘 .92 的 IOR 修正和局部因子，再按实际捕获 textureSize 推导出的完整 mip 链钳制，bicubic floor/ceil 两次访问均不超过末级。没有噪声、屏幕固定模糊框或虚假的二维玻璃。

## 线性捕获与输出

玻璃使用公开 CustomMaterial.Unshaded，显式移植 Three 的 cubeUV/DFG、多次散射能量补偿、折射投影、Beer 衰减、clearcoat 与 ACES；不使用 Qt 私有渲染类。shader 片段保留 MIT 来源，许可证为 `resources/licenses/three-LICENSE`。固定 RoomEnvironment 从冻结源码和已锁定 Three 的实际 PMREM 路径生成；RGBA32F 为 768×1024 的原 cubeUV 排列。DFG 为原 16×16 半精度积分表转成 float，`node tools/assets/export-material-data.mjs` 从既有锁定依赖确定性导出并刷新材料 manifest；`FixedTextureData` 使用公开 TextureData API 读固定 qrc 字节并处理端序，两张纹理各只持有一份。

纸面走 Shaded/POST_PROCESS，线性采样后以入射漫反射 `paperIrradiance` 乘原图颜色，再限制到原图颜色；不加 emission/specular，不对纸面执行 ACES。R07 的固定白色半球测试输入为 (1,1,1)，R08 接入实际世界光场时替换这一入射量。过强照明不能洗白图像，无光必须输出黑色印刷区域。三种封面共用 contain、1/64 透明留白、alphaTest=.025 与半 texel clamp。正式异步封面、图集和选中纹理的共同绘制路径由 R11 接入。

不透明背景平面写入精确线性的双主题背景，保证同一捕获中背景/纸面都处于线性空间；它是当前单盒固定环境的背景输入，后续场景集成需要保留同一颜色空间约束。磨砂壳体从公开 SCREEN_MIP_TEXTURE 采样，捕获不含这些玻璃，不能递归透过第二层玻璃。SceneEnvironment 的 TonemapNone 禁止重复映射；最后 `material-output.frag` 只做精确分段 sRGB 编码。玻璃先做一次源 ACES，纸面保留 toneMapped=false；不把 Unshaded 输出误认为 Qt 会自动编码 sRGB。

## 验证与重建

Windows 使用 `./tools/build/verify-r07.ps1`，只执行 `qt-release`。它从新 Git archive 运行冻结模块、使用已安装浏览器和既有锁定 Web 开发依赖生成单盒 GPU 对照；不读取私人曲库，阻止外部 HTTP 请求。然后开启 YALAHOO_REVIEW，做确定性坐标搜索和所有 CTest、qmllint、六 shader 的跨目标 qsb 检查；结束时关掉 Review、构建/测试普通产物并验证 `--verify-materials` 被拒绝。`-SkipWebCapture`/`-SkipCalibration` 仅用于对 hash 未变的既有证据进行复用。

`single-case.json` 定义相同 960×720 内容、DPR1、32°相机，正视/低角度/斜视，双主题、方/横/竖封面以及 all/paper/主盖/侧脊/后托，合计 90 状态。低角度与斜视用于分开检查侧脊密度、主盖霜化与纸面空间。每状态等待四帧真实 GPU 渲染，核对独立 Three 投影≤2px、源参数与非空前景。追加九个纸面测试：零/.25/2 倍入射量分别应暗、渐暗、限制到原图颜色，中心通道误差≤3/255。几何 Review 的九尺寸、DPR1/1.5/2 和 120fps 相机重放仍在全量回归内。

五个 ROI 为盖内纸面、盖外留白、侧脊、外框、盒体后的背景，投影来自固定局部区域。Qt/C++ 计算 RGB MAE、signed RGB、11×11 σ1.5 Gaussian SSIM 和差分 PNG。窄外框不足 11px 时 SSIM 为 null，使用 MAE/独立结构门槛；不把空窗口记为 SSIM=1。标定使用全部 18 个组合的 all 视图、五区域等权损失，先完成颜色空间/固定环境核对，随后按曝光、环境、透射、粗糙度、厚度、衰减距离依次试当前/±10/5/2/1%，最多三遍，范围钳制；同损失取离源码最近的系数。R07 不引入 Qt 灯，因此没有 Qt 灯强单位需要搜索，动态灯光见 [灯光契约](lighting.md)：使用同一源单位的自定义 GLSL，未使用需要单位换算的 Qt 内置灯。

Windows 数值/诊断在 `verification/windows/materials/`，Web/Qt 同输入图像和差分在 `docs/media/materials/web/`、`qt/`，残差说明在 `verification/windows/materials/report.md`。它们只证明单盒静态材质及固定入射输入，不代表后续动态光场/阴影/景深/阵列/性能验收。`tools/audit/audit-r07.ps1` 展开 macOS/Linux 的本轮构建、资源、shader 与生命周期；结论为“静态审计通过，实机待用户验证”，用户清单在 docs/platforms。

公开接口语义：[CustomMaterial](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)、[SceneEnvironment 色调输出](https://doc.qt.io/qt-6/qml-qtquick3d-sceneenvironment.html)、[TextureData](https://doc.qt.io/qt-6/qquick3dtexturedata.html)。qsb wrapper 生成 MSL/GLSL 仅作为静态翻译检查，实际 Metal/OpenGL 的捕获/采样/Y 方向与绑定还需要目标机运行验证。

音乐生产路径覆盖环境强度为暖昼 .25 / 深夜 .16，纸面接入真实漫反射环境与世界光场；上文 .48 / .68 和固定白色入射只用于 R07 隔离材质参照。动态生产值、轨迹及对应证据见灯光契约。
