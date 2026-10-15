# 单盒材质契约

冻结基线为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。主依据为 `music-model.ts` 的 configure/theme/clarity/print 函数及 `appearance.ts`、`scene.ts` 的最终有效覆盖，来源 hash 在 [materials.json](materials.json)。单盒验收独立于全场动态灯光、阴影、景深、阵列、片头与播放。

## 材质与空间

`scene/CaseMaterials.qml` 持有三个共享壳体材质、一个漫反射纸面及共享 RoomEnvironment/DFG 纹理；`GlassMaterial.qml` 以 surface=0/1/2 区分主盖/侧脊/后托。导入的三批次 mesh 没有改动。纸面是 3.06×3.06、中心 (0.17,1.85,0.027) 的独立平面，随同一个盒体位移，后托前面 -0.032、盖背面 .052、盖前面 .070 不共面。

| 表面 | 基础→暖昼透射 | 粗糙度 | 厚度 | 衰减距离 | clearcoat / 粗糙度 |
|---|---|---|---|---|---|
| 主盖 | .96→.94 | .34→.40 | .018→.022 | 5→3.8 | .08 / .38 |
| 侧脊 | .88→.84 | .25→.30 | .06→.08 | 4.5→3.2 | .16 / .20 |
| 后托 | .80→.70 | .38→.46 | .038→.05 | 4.5→2 | .08 / .38 |

IOR 固定 1.46，金属度为零，透明度为一，单面透射；侧脊独立计算密度与高光。详情只使主盖粗糙度趋向基础 .27 / 暖昼 .34，侧脊和后托不跟随。数值 warmth/clarity 都是连续输入，实际主题动画按 [灯光契约](lighting.md) 接入同一个 FrameClock。参数直接进入 QML 与生产 RHI uniform。R19C1 移除了 R07 的 ROI 搜索系数（环境/粗糙度 1.01、厚度 .91809、衰减距离 1.0201）；它们没有单位或算法依据，不再参与实现。IOR、颜色、封面与几何仍按冻结源码。

暖昼表面色为 #fffdf8 / #e7e1d8 / #ece6dc；夜间为 #f6fbff / #f6fbff / #c6d6e5。暖昼衰减色 #f3e9db，夜间主盖/侧脊 #dceafd；后托保留原衰减色。QML color 作为线性 shader uniform，颜色插值发生在线性 shader 中。生产调用下，Three 在材质 envMap 为空时把场景 environmentIntensity 覆盖到 uniform；因此有效强度为暖昼 .48 / 深夜 .68，而不是再乘 configureMusicGlass 的 .65。

主盖局部 `d=max(abs(localXY-(.17,1.85))-(1.53,1.53))`，`p=1-smoothstep(.015,.15,d)`，纸面 LOD 因子为 `mix(1,.16,p)`。局部位置由同一 MODEL_MATRIX 关联实体；SCREEN_MIP_TEXTURE 的粗糙 LOD 乘 .92 的 IOR 修正和局部因子，再按实际捕获 textureSize 推导出的完整 mip 链钳制，bicubic floor/ceil 两次访问均不超过末级。没有噪声、屏幕固定模糊框或虚假的二维玻璃。

## 线性捕获与输出

玻璃使用公开 CustomMaterial.Unshaded，显式移植 Three 的 cubeUV/DFG、多次散射能量补偿、折射投影、Beer 衰减、clearcoat 与 ACES；不使用 Qt 私有渲染类。shader 片段保留 MIT 来源，许可证为 `resources/licenses/three-LICENSE`。固定 RoomEnvironment 从冻结源码和已锁定 Three 的实际 PMREM 路径生成；RGBA32F 为 768×1024 的原 cubeUV 排列。DFG 为原 16×16 半精度积分表转成 float，`node tools/assets/export-material-data.mjs` 从既有锁定依赖确定性导出并刷新材料 manifest；`FixedTextureData` 使用公开 TextureData API 读固定 qrc 字节并处理端序，两张纹理各只持有一份。

纸面走 Shaded/POST_PROCESS，线性采样后以入射漫反射 `paperIrradiance` 乘原图颜色，再限制到原图颜色；不加 emission/specular，不对纸面执行 ACES。R07 的固定白色半球测试输入为 (1,1,1)，R08 接入实际世界光场时替换这一入射量。过强照明不能洗白图像，无光必须输出黑色印刷区域。三种封面共用 contain、1/64 透明留白、alphaTest=.025 与半 texel clamp。正式异步封面、图集和选中纹理的共同绘制路径由 R11 接入。

不透明背景平面写入精确线性的双主题背景，保证同一捕获中背景/纸面都处于线性空间；它是当前单盒固定环境的背景输入，后续场景集成需要保留同一颜色空间约束。磨砂壳体从公开 SCREEN_MIP_TEXTURE 采样，捕获不含这些玻璃，不能递归透过第二层玻璃。SceneEnvironment 的 TonemapNone 禁止重复映射；最后 `material-output.frag` 只做精确分段 sRGB 编码。玻璃先做一次源 ACES，纸面保留 toneMapped=false；不把 Unshaded 输出误认为 Qt 会自动编码 sRGB。

## 验证与重建

当前有效入口：`node tools/review/export-material-parameters.mjs` 直接执行映射后的冻结 `configureMusicGlass`、`CardAppearance.setTheme`、`ThemeTransition` 和 `setMusicGlassClarity`，生成45组主题/清晰度参数输入；不需要浏览器或GPU读回。`rhi-material-parameters` 同时检查 C++ 和真正加载的 `GlassMaterial.qml`，包括生产环境 .25/.16 与隔离材质 .48/.68 的区分。`ArchivePoolAudit` 在真实隐藏Metal场景读取实际上传缓冲区中的壳体参数，检查每个可见实体，覆盖阵列、详情/归位与两主题；只在采集时生成参数JSON，不增加生产逐帧工作。

```sh
node tools/review/export-material-parameters.mjs
node tools/review/check-web-baseline.mjs
cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release -R '^(rhi-material-parameters|lighting)$' --output-on-failure
ctest --preset qt-release-render -R '^(archive-pool-gpu|real-columns-gpu)$' --output-on-failure
```

`calibrate-materials.py` 已退役，执行即报错且不写文件；MaterialReview拒绝calibration/factors输入，参数核对不再将expected乘同一拟合系数。MaterialReview及`verify-r07.ps1`/`verify-r08.ps1`已在R19C2A移除原图/ROI/标定依赖。两个PowerShell入口转交跨平台`tools/build/verify-material-review.py materials|lighting`，只验证当前源参数/投影、自己的输出及资源；Windows入口仍先进入MSVC环境，进程退出由Python真实等待。RhiShellAudit/RhiAudit的原图/环境witness仍待R19C2B拆耦，不能执行。R07曾使用90个双主题/视角/封面/分层状态及9项纸面探针；历史采集与像素分数仅保留原事实，不再执行或据此调参。

参数通过不证明光学捕获、磨砂LOD、连续闪烁或最终缩放已正确；这些检查分别由R19C2/R19D执行，用户问题保持开放。Windows/D3D11与Linux/OpenGL目标回归按迁移计划R34移交，本机Metal结果不外推。完整逐项历史归属、PERF与验证范围见 [source-fidelity-review.md](source-fidelity-review.md)。

公开接口语义：[CustomMaterial](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)、[SceneEnvironment 色调输出](https://doc.qt.io/qt-6/qml-qtquick3d-sceneenvironment.html)、[TextureData](https://doc.qt.io/qt-6/qquick3dtexturedata.html)。音乐生产路径环境强度为暖昼 .25 / 深夜 .16，纸面接入真实漫反射环境与世界光场；上文 .48 / .68 和固定白色入射只用于 R07 隔离材质参照。

## 材质/灯光隐藏 Review 入口

`--verify-materials`在创建ApplicationWindow前分派，直接在自己的QQuickWindow中加载同一`GeometryScene`。macOS默认使用`OffscreenSurface`/QQuickRenderControl真实Metal，其他系统暂保留原生置底/拒绝焦点路径；`YALAHOO_VERIFY_VISIBLE=1`才在macOS选择窗口，前台授权规则不变。既有90个材质状态和48个灯光状态均来自已提交源参数fixture，保留三表面参数、独立源投影和CPU/Quick3D投影的2逻辑px容差。每状态由真实帧事件驱动四个准备帧，然后仅采集一次自己的texture；不使用轮询或原图。

纸面九项数值探针使用`tests/fixtures/materials/paper-probe.png`的均匀RGBA墨色(96,160,208,255)，这是测试输入，只在Review链接。期望值由源公式`sRGBEncode(min(sRGBDecode(ink)*irradiance, sRGBDecode(ink)))`推导，覆盖0/.25/2入射，保留原3码值容差；本机误差为0。三种正式封面形状/contain仍由原90状态覆盖，不拿均匀墨色证明封面细节正确。

材质额外执行十次完整QML root/scene graph重建，灯光执行一次并重新应用相同源状态；每次检查真实invalidation和有效输出。报告显式`qmlRootRecreated=true`，不宣称同一View3D对象恢复或可见窗口恢复通过。首次尝试只invalidate/initialize、补releaseResources及重新挂接同一View3D后均为背景；完整root重建恢复有效输出，内部Quick3D原因尚未确认。该流程属于独立Review生命周期，生产SceneItem/RHI不走此路径。

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
cmake --build --preset qt-release
ctest --preset qt-release-render -R '^(runtime-materials|runtime-lighting)$' --output-on-failure
python3 tools/audit/audit-material-review.py
# 其他系统当前使用qt-release-visible选择同名测试，遵守窗口契约。
# 一体复建入口（Windows需MSVC shell）：
python3 tools/build/verify-material-review.py materials
```

一体入口最后恢复Review OFF并构建/运行普通Release快速回归；日常Review ON的完整隐藏preset包含这两个测试，快速preset不包含。报告`validation=source-state-and-own-output`、`imageComparison=false`，保存warnings、实际后端/DPR、窗口策略及Offscreen附件估算；未知driver分配保持null。检查只证明有限Review场景源状态、纸面公式、有效输出与完整root重建，不能证明RHI光学等价、连续透明闪烁、正常VSYNC或完整性能。原SSIM/RGB/ROI历史数据不再重算，R19C2B/C、R19D继续处理光学与用户问题。
