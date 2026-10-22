# 单盒材质契约

冻结基线为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。主依据为 `music-model.ts` 的 configure/theme/clarity/print 函数及 `appearance.ts`、`scene.ts` 的最终有效覆盖，来源 hash 在 [materials.json](materials.json)。单盒验收独立于全场动态灯光、阴影、景深、阵列、片头与播放。

## 材质与空间

Quick3D Review的`scene/CaseMaterials.qml`持有三个共享壳体材质、一个漫反射纸面及共享 RoomEnvironment/DFG 纹理；`GlassMaterial.qml` 以 surface=0/1/2 区分主盖/侧脊/后托。导入的三批次 mesh 没有改动。纸面是 3.06×3.06、中心 (0.17,1.85,0.027) 的独立平面，随同一个盒体位移，后托前面 -0.032、盖背面 .052、盖前面 .070 不共面。

| 表面 | 基础→暖昼透射 | 粗糙度 | 厚度 | 衰减距离 | clearcoat / 粗糙度 |
|---|---|---|---|---|---|
| 主盖 | .96→.94 | .34→.40 | .018→.022 | 5→3.8 | .08 / .38 |
| 侧脊 | .88→.84 | .25→.30 | .06→.08 | 4.5→3.2 | .16 / .20 |
| 后托 | .80→.70 | .38→.46 | .038→.05 | 4.5→2 | .08 / .38 |

IOR 固定 1.46，金属度为零，透明度为一，单面透射；侧脊独立计算密度与高光。详情只使主盖粗糙度趋向基础 .27 / 暖昼 .34，侧脊和后托不跟随。数值 warmth/clarity 都是连续输入，实际主题动画按 [灯光契约](lighting.md) 接入同一个 FrameClock。参数唯一由`src/render/OpticalParameters.h`的连续模板计算维护；`SourceOptics`缓存double结果供三个Quick3D材质读取，生产RHI以float实例化同一公式并上传uniform。R19C1 移除了 R07 的 ROI 搜索系数（环境/粗糙度 1.01、厚度 .91809、衰减距离 1.0201）；它们没有单位或算法依据，不再参与实现。IOR、颜色、封面与几何仍按冻结源码。

暖昼表面色为 #fffdf8 / #e7e1d8 / #ece6dc；夜间为 #f6fbff / #f6fbff / #c6d6e5。暖昼衰减色 #f3e9db，夜间主盖/侧脊 #dceafd；后托保留原衰减色。生产RHI按原float精度一次解码固定颜色端点，跨帧复用；连续颜色插值保留原计算顺序。QML color 作为线性 shader uniform，颜色插值发生在线性 shader 中。隔离材质Review的环境强度暖昼.48/深夜.68来自场景environmentIntensity覆盖，不再乘configure的.65；生产音乐环境强度由lighting.md定义。

主盖局部 `d=max(abs(localXY-(.17,1.85))-(1.53,1.53))`，`p=1-smoothstep(.015,.15,d)`，纸面 LOD 因子为 `mix(1,.16,p)`。局部位置由同一 MODEL_MATRIX 关联实体；SCREEN_MIP_TEXTURE 的粗糙 LOD 乘 .92 的 IOR 修正和局部因子，再按实际捕获 textureSize 推导出的完整 mip 链钳制，bicubic floor/ceil 两次访问均不超过末级。没有噪声、屏幕固定模糊框或虚假的二维玻璃。

## 线性捕获与输出

隔离Quick3D Review玻璃使用公开CustomMaterial.Unshaded，显式移植 Three 的 cubeUV/DFG、多次散射能量补偿、折射投影、Beer 衰减、clearcoat 与 ACES；不使用 Qt 私有渲染类。shader 片段保留 MIT 来源，许可证为 `resources/licenses/three-LICENSE`。固定 RoomEnvironment 从冻结源码和已锁定 Three 的实际 PMREM 路径生成；RGBA32F 为 768×1024 的原 cubeUV 排列。DFG 为原 16×16 半精度积分表转成 float，`node tools/assets/export-material-data.mjs` 从既有锁定依赖确定性导出并刷新材料 manifest；`FixedTextureData` 使用公开 TextureData API 读固定 qrc 字节并处理端序，两张纹理各只持有一份。

纸面走 Shaded/POST_PROCESS，线性采样后以入射漫反射 `paperIrradiance` 乘原图颜色，再限制到原图颜色；不加 emission/specular，不对纸面执行 ACES。R07 的固定白色半球测试输入为 (1,1,1)，R08 接入实际世界光场时替换这一入射量。过强照明不能洗白图像，无光必须输出黑色印刷区域。三种封面共用 contain、1/64 透明留白、alphaTest=.025 与半 texel clamp。正式异步封面与图集见covers.md。

不透明背景平面写入精确线性的双主题背景，保证同一捕获中背景/纸面都处于线性空间；它是当前单盒固定环境的背景输入，后续场景集成需要保留同一颜色空间约束。磨砂壳体从公开 SCREEN_MIP_TEXTURE 采样，捕获不含这些玻璃，不能递归透过第二层玻璃。SceneEnvironment 的 TonemapNone 禁止重复映射；最后 `material-output.frag` 只做精确分段 sRGB 编码。玻璃先做一次源 ACES，纸面保留 toneMapped=false；不把 Unshaded 输出误认为 Qt 会自动编码 sRGB。
生产RHI输出与隔离Review的颜色路径分开：生产统一后处理/ACES/sRGB归postprocess.md；RHI捕获/壳体适配归rhi-shell.md。本节直接材质ACES不进入生产逐物体输出。

## 参数与独立Review检查

- `node tools/migration/export-material-parameters.mjs`执行冻结configure/CardAppearance/theme/clarity，生成45连续输入；`rhi-material-parameters`核对真正加载的QML/C++及210端点，生产显式采集检查每实体实际uniform，不能仅测helper。
- `runtime-materials`在独立QQuickWindow加载GeometryScene，macOS默认RenderControl，其他平台按test-gates选择实际入口。源90状态与9纸面探针共99项，三表面参数≤1e-6、独立CPU/源/Quick3D投影≤2逻辑px。
- 纸面输入`tests/fixtures/materials/paper-probe.png`为RGBA(96,160,208,255)，0/.25/2入射按`sRGBEncode(min(sRGBDecode(ink)*irradiance,sRGBDecode(ink)))`核对，≤3码值；正式三形状封面另验contain/细节。
- 每状态真实事件驱动四个准备帧后采一次自身texture，核对request/warnings/有效输出。材质与灯光各一次完整QML root+资源重建；报告区分root重建与同一View3D恢复。只invalidate/initialize后漏画时按计划对应经验重建拥有场景的root。
- 已准备Release Review产物后，按test-gates.md选择`ctest --preset qt-release-render -R '^runtime-(materials|lighting)$'`中的对应组件；状态/公式通过不能替代生产RHI光学/连续质量/最终尺度检查。

## Quick3D Review 的 OpenGL MSAA 适配

`src/render/rhi/ReviewGlCapabilities.h`只由Review材料入口使用，私有API由`YALAHOO_REVIEW_MSAA_API`按本机最新Kit实际编译探测。Cocoa OpenGL缺少`glTexStorage2DMultisample`时，Qt的纹理MSAA能力标志与该后端实际分配入口不一致；在`sceneGraphInitialized`且上下文有效时，仅校正该标志，令Quick3D采用其既有renderbuffer＋resolve分支。该分支依据[Qt Quick3D源实现](https://github.com/qt/qtquick3d/blob/dev/src/quick3d/qquick3dscenerenderer.cpp)；不修改Kit文件、全局GL函数或产品渲染器。

实际render线程在`afterRendering`读取Quick3D主pass的采样数与真实颜色附件/resolve描述符，仅以加锁值快照交给GUI报告。适配启用且请求MSAA时，必须核对4×采样、renderbuffer、独立resolve及颜色格式一致；每次真实SG重建重新探测。无相关缺口的平台不修改能力，不能关AA、降采样或改颜色精度规避失败。

Review的原生“重建”销毁完整QQuickWindow及QML根；RenderControl仍以完整QML根＋control失效重建。两者都要求实际sceneGraphInvalidated递增并恢复正常输出。新窗口重新应用窗口策略/能力探测，旧帧按window epoch拒绝；同对象surface恢复由窗口/产品专项另行检查。
