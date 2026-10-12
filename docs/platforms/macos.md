# macOS · 用户实机验证清单

R02 结论：**静态审计通过，实机待用户验证**。矩阵见 `verification/macos-static/audit-matrix.json`。以下均未在 macOS 执行，真实结果由用户填写；完整产品和 macdeployqt 部署在 R31 接入。

安装本机最新原生 Qt Desktop Kit（包含 Core/Gui/Qml/Quick/QuickControls2/QuickDialogs2/Quick3D/Multimedia/Concurrent/Svg/ShaderTools/Test/QuickTest）、Xcode C++23 工具链、CMake 与 Ninja。默认枚举 `$HOME/Qt`、`/opt/Qt`、`/usr/local/Qt`；自定义安装根使用 `YALAHOO_QT_ROOTS`。仅使用当前主机的单一原生架构。

```sh
sh tools/build/build.sh qt-release
sh tools/build/build.sh qt-release
```

预期：两配置自动选同一最高版本，C++23 探针通过；CTest 的身份、Kit、模块、qsb、实际窗口测试全部通过。qsb 检查存在 MSL 变体。首次使用编译器或升级 Kit 后，可追加 `--fresh`。

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/release/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" ./build/release/bin/yalahoo.app/Contents/MacOS/yalahoo
```

预期：标题与 bundle 名为 `yalahOO!!!`，中文完整、Basic 按钮点击后计数变化、中央几何有立体面；窗口在 Retina 下以逻辑像素布局，可正常 resize/关闭。Qt 使用 Metal；AppDataLocation 使用原生 Qt/macOS 位置且技术名稳定。仅创建媒体对象，不应发声、打开浏览器或联网。

构建期 `build/release/verification/runtime.json` / `runtime.png` 记录实际结果。当前使用 qrc 中同源 MiSans，默认 Regular，标题 Bold，中央显示原 GLB 三壳体的导入几何。当前开发 bundle 依赖选中 Kit，不是已完成的独立安装包。没有验证实际播放、格式兼容、设备切换、产品菜单或最终模型/光效。

## R03 资源验证

结论：**静态审计通过，实机待用户验证**。新增矩阵见 `verification/macos-static/assets/audit-matrix.json`；资源路径及字体映射见 `docs/contracts/assets.md`。没有在 macOS 实际运行。

```sh
ctest --preset qt-release -R assets --output-on-failure
sh tools/assets/prepare-assets.sh qt-release
sh tools/build/build.sh qt-release
```

预期：47 项 qrc hash、三个原壳体、1,000 三角形及纸面间隙通过；四个 MiSans 字重选中正确 CFF 表，中文/拉丁 glyph 与整行宽度检查通过；八个功能 SVG 的双主题/viewBox 有效；四个完整音频 LoadedMedia、duration 有效，全程无 QAudioOutput、无播放。准备脚本从同一最新 Kit 导入，复用已入库 SFNT 时先验证原 WOFF2/输出 hash；不需要 Dwrite。字体、图标预览和报告在 `build/release/verification/`。

从带空格/中文的任意目录启动上述绝对 bundle 路径，确认所有资源仍显示；Metal 下确认三壳体没有缺失，四字重预览没有回退成系统字形。音频资源加载通过不代表实际播放验收，后者留给 R24/R25。

实机结果：待用户验证。

## R03B 品牌资产实机复核（结果待用户填写）

本轮静态审计通过，实机待用户验证。配置本机最新 Qt Kit 后执行：

```sh
sh tools/assets/export-branding.sh qt-release --window-preview
ctest --preset qt-release -R branding --output-on-failure
```

预期：工具能读取带空格/Unicode 的仓库路径，13 个 SVG 加载成功，16px 双环内孔和边框分离；浅/深/透明背景及反白稿清晰；导出资源 hash 可复现，branding 测试通过。检查 `docs/media/branding/` 的实尺寸和揭示进度图。预览标签可使用系统字体，品牌本身完全不依赖字体。

macOS 额外使用系统 `iconutil -c icns resources/icons/yalahoo/yalahoo.iconset -o build/yalahoo-iconutil.icns`，复核 iconset 可接受及 Finder 预览；生成文件保留在 build。Windows 生成的 `resources/icons/yalahoo/yalahoo.icns` 已通过块结构/PNG验证，但本机 Finder 结果尚未执行。应用 bundle、页眉与开场品牌接入在后续轮次，本清单不假定已经接入。

## 数据与离线资料用户实机验证（R04）

常规验证只使用 Release；只有明确需要获取堆栈时才编译和运行 Debug。以下步骤尚未在本平台执行，请用户记录实际结果。

```sh
sh tools/build/build.sh qt-release
ctest --preset qt-release -R 'library-data|library-qml|runtime-library' --output-on-failure
```

预期：所有冻结匿名库列、216 个冻结搜索用例、排序/分组、JSON 未知字段与 Unicode 本地 QUrl、模型通知、独立导航/播放 ID、异步缓存/Deferred/取消/过期结果/销毁通过；同一 QML 组件读到稳定专辑/曲目角色，离线和测试 Provider 的 Matched/Uncertain/Error 均可读取。实际 runtime-library 窗口显示 21 张匿名专辑，Typography 得到 28 条结果，资料缓存返回一次，鼠标交互正常，歌曲来源保持空；报告在 build/release/verification/runtime-library.json。没有扫描或播放用户音乐，零网络请求的源码审计见对应 library-data/audit-matrix.json。

实机结果：待用户填写。此清单不代表本平台已经编译、运行或验证媒体播放。

## 运动内核与生命周期实机验证（R05）

静态审计通过，实机待用户验证。使用本机最新 Qt Kit 执行：

```sh
sh tools/build/build.sh qt-release
ctest --preset qt-release -R 'motion|runtime-motion' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

预期：两个 694 帧冻结数值对照及运动边界检查通过，x/v/a 误差分别不超过 1e-6。`runtime-motion` 运行真实 Metal/QML 窗口，自动隐藏/最小化并恢复，两次后台期间帧数和视觉时间不增长，恢复首帧 dt=0，减少动态完成最后 revision，开场独立时间到 11.6，媒体来源为空；报告在 `build/release/verification/runtime-motion.json`，配套截图为同目录 `runtime-motion.png`。另手动将应用进入系统隐藏/恢复状态，确认帧源停用/恢复且无跳变；仅切换焦点为 Inactive 时允许继续视觉帧。

若已有构建树新增 QObject 后保留旧 moc 列表，执行 `cmake --build --preset qt-release --clean-first` 再运行测试。以上没有在 macOS 执行；实际 Metal 帧率、平台生命周期事件和产品场景仍待对应轮次/用户验证。

## 几何、相机与布局实机验证（R06）

静态审计通过，实机待用户验证。使用本机最新原生 Qt Kit，仅 Release：

```sh
sh tools/build/build.sh qt-release -DYALAHOO_REVIEW=ON
ctest --preset qt-release -R '^scene$|runtime-scene' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

预期：48 个冻结相机/布局目标、528 顶点/锚点及 694 个 20/30/60/120fps 运动样本通过；双精度轨迹误差≤1e-6。真实 View3D 的逻辑像素投影误差≤2px；九内容尺寸、横竖屏比值 1.05 的两侧与等值、移动中 resize/反向/改速/减少动态同步更新布局和相机。三批次几何保留原物理尺寸，仅使用不透明无光调试材质，当前未验收玻璃/灯光/纸面。

查看 `build/release/verification/scene-dpr-*/runtime.json` 及同目录截图，核对 actual dpr、client width/height、PNG imageWidth/imageHeight、case bindings、逐帧 projections 和 passed。`QT_SCALE_FACTOR` 会与平台原生 DPR 相乘，测试专用 `QT_ENABLE_HIGHDPI_SCALING=0` 在某些原生平台不能关闭 Retina/原生缩放；必须报告实际有效 DPR，不能将测试目录名当成实际 DPR。窗口管理器若限制超大 frameless 客户区，记录具体尺寸/实际值及失败项，不记为已通过。

手动调整窗口经过 945×900 和 944×900，检查布局类别同时切换、相机沿原速度继续移动，无 Z 翻转或重复缩放。普通探针按钮可反向切换浏览/详情目标。运行 `runtime-motion` 时减少动态应同时让共用运动与相机落稳；隐藏/最小化暂停唯一生产帧源。

关闭 Review 后继续普通构建：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=OFF
cmake --build --preset qt-release
```

预期：普通构建不识别 `--verify-scene`/`--scene-report`。本平台编译、GPU/窗口系统、DPR 及交互实际结果：待用户填写；Windows 证据不替代上述执行结果。

## R07 单盒材质用户验证（未执行）

本轮静态审计见 `verification/macos-static/materials/`；结论为“静态审计通过，实机待用户验证”。它不表示本平台 shader、窗口或图像已经运行通过。

使用本机最新原生 Kit，执行 Release configure/build/test 与 qmllint，开启 `-DYALAHOO_REVIEW=ON` 后运行 `runtime-materials`。输入为 `tests/fixtures/materials/single-case.json`，源相机/匿名封面与 Windows 对照一致。确认三个壳体分别可见、纸面真实位于盖后，无 Z 翻转、UV 倒置、发光或重复 gamma；夜间留白不能多出蓝色高光，主盖详情柔化不能改变侧脊。

该固定图像 fixture 要求实际 DPR1；先读取本机原生 DPR，必要时只为测试进程设置 QT_SCALE_FACTOR 的补偿倍率，使报告的 actual dpr 真正为1。Retina/Wayland 原生缩放可能不响应 QT_ENABLE_HIGHDPI_SCALING=0，不能仅根据环境变量宣称 DPR 满足。若无法获得相同 DPR/真实客户区，应另采该平台同尺寸同 DPR 的 Web 对照，按材质契约核对，记录这一差异；产品不设置上述测试环境变量。

检查 `build/release/verification/materials/runtime.json`：90 同输入逐层状态、9 漫反射探针、非空前景、sourceParameterMatch、Web投影≤2px；所有 PNG 的真实像素尺寸应与报告DPR一致。分别检查正视/低角度、双主题和方/横/竖封面，人工确认 SCREEN_MIP_TEXTURE 的纸面区域/留白采样与折射 UV 方向，MSL/GLSL 生成成功不能替代这一实机步骤。结束后 `cmake --preset qt-release -DYALAHOO_REVIEW=OFF` 并重新构建，普通产品应拒绝 --verify-materials，按钮改变 detail 时只有主盖柔化；隐藏/恢复与旧运动回归继续通过。

本平台实际构建、shader/GPU、图像、DPR及窗口交互结果：待用户填写。完整平台部署、真实音频和全场灯光/后处理属于后续轮次。

## R08 灯光用户验证（未执行）

静态审计见对应 `verification/*-static/lighting/`，结论为“静态审计通过，实机待用户验证”。使用本机最新原生Qt Kit，在根目录执行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'lighting|runtime-lighting' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

输入为 `tests/fixtures/lighting/golden.json` 和 `visual.json`；读取 `build/release/verification/lighting-errors.json` 与 `lighting/runtime.json`。预期924轨迹/主题样本和392光场样本通过、48双主题/四模式场景非空、五ROI MAE≤6/255/有效SSIM≥.97、Web投影≤2px。确认实际有效DPR=1，原生Retina/Wayland缩放如不能关闭，按前述R07步骤为测试进程补偿或重采同DPR对照，不把环境变量值当作实际DPR。

手动运行普通探针，反复点击暖昼/深夜按钮，再切浏览/详情目标；主题应从当前显示状态连续反向，650ms源时窗按动效倍率缩放一次，印刷只暗化不闪白，玻璃与纸面光带位置一致。隐藏/最小化暂停视觉，恢复不补跑整段主题。预期Metal/OpenGL无需修改灯光代码；实际GPU纹理方向/绑定必须检查，MSL/GLSL生成不替代实机。

验证后关闭Review并重建：`cmake --preset qt-release -DYALAHOO_REVIEW=OFF`、`cmake --build --preset qt-release`、`ctest --preset qt-release --output-on-failure`；普通产物拒绝 --verify-materials。本平台构建、GPU、图像、DPR、按钮交互实测结果：待用户填写。

## R09 阴影与 SSAO 用户验证（未执行）

静态审计见 `verification/macos-static/occlusion/`；结论为“静态审计通过，实机待用户验证”。使用本机最新原生 Qt Kit，执行 Release：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'render-quality|runtime-occlusion|occlusion-gpu-audit' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

输入 `tests/fixtures/occlusion/visual.json`：80个生产状态及8个单独诊断；核方向来自同一seed=417资源。按R07步骤取得真实DPR1，检查报告88状态、真实960×720 PNG、画质目标尺寸及8.3MP上限。检查raw depth/normal的行方向、纸面/壳体完整几何覆盖，尤其RGB24线性深度链接解码；080–085的AO应非全白，086/087的阴影开关必须有真实像素变化。逐项比较raw AO、blur、linearColor及邻盒，按 `docs/contracts/occlusion.md` 和限制清单区分后续输入；图像审计按本平台的Metal/OpenGL枚举检查，Windows的实际通过结论不替代本平台结果。

公开Qt阴影投影/偏置/滤波和Unshaded壳体接收仍交R10；背景/linearColor 输出契约及真实线性诊断见 R09B 清单。应在本平台另外记录实际后端、DPR、目标格式/方向、差异和诊断，MSL/GLSL生成不能替代GPU验证。关闭Review并重建、运行普通回归后，用 `--verify-runtime <report.json> --verify-shading` 检查同一产品材质/AO链路；普通产品应拒绝 `--verify-materials`。性能/原始/高/极高切换不得改变相机或业务状态。

本平台真实构建、GPU、图像、DPR、窗口交互结果：待用户填写。

## R09B 景深、抗锯齿与色彩用户验证（未执行）

新增严格静态审计在 `verification/macos-static/postprocess/`，结论为“静态审计通过，实机待用户验证”。使用本机最新原生 Kit，Release Review 执行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'render-quality|runtime-postprocess|postprocess-gpu-audit' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

按前述 Retina 步骤核对报告的真实 DPR，必要时重采同 DPR Web 参照；不把环境变量值冒充实际比例。检查 `build/release/verification/postprocess/` 三个 DPR 报告共 105 状态与汇总；后端应为 Metal。检查九尺寸双主题盒体外斜边、盖内纸面留白和封面文字，比较 NoAA/MSAA/SSAA 局部图（SSAA 仅诊断）；确认原始 MSAA 实际产生边缘覆盖，高/极高启用 SMAA、性能关闭 DOF，Bokeh 前/后景模糊开关产生真实像素变化，焦距随相机/抽取位置变化。检查原始/高/极高/性能往返以及先关景深再启用 SMAA，无崩溃/黑帧/相机和导航重置。查验背景 final/raw ROI、RGB24 深度方向/精度、area/search 数值纹理行序和单次 ACES/sRGB，不拿旧 R09 channel=5 当真实线性图。具体未达图像目标项按 `docs/contracts/postprocess.md` 与限制清单逐项对照，Windows 图不能替代 Metal 实测。

关闭 Review：`cmake --preset qt-release -DYALAHOO_REVIEW=OFF`，重建并执行普通 CTest；运行 `--verify-runtime <report.json> --verify-shading` 检查生产图，普通程序应拒绝 `--verify-materials`。本平台构建、Metal、DPR、图像和交互实测结果：待用户填写。

## R10 渲染选择与探测用户验证（未执行）

新增审计在 verification/macos-static/render-backend/，结论为“静态审计通过，实机待用户验证”。R10 选择 RHI，当前程序仍为 Quick3D bootstrap；RHI 实现按 R10A–D 执行。目标机使用最新原生 Kit 的 Release Review：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'image-comparison|runtime-render-backend|render-backend-gpu-audit' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

先核对实际 DPR1 和 fixture 的 960×720/320×720 内容尺寸，必要时重采同 DPR Web 参照。报告的 graphicsApi 应为 Metal (6)；核对全部 70 状态、源参数、投影、三档阴影 on/off、邻盒 pre/post Bokeh 与独立深度/法线，检查 9 实例、图集变色/稳定/还原的实际像素。audit passed 代表探测有效，parityPassed=false 保留 Quick3D 的渲染残差；不得将此视为 RHI 或全应用图像通过。实际 GPU 上传字节仍为 null；R10C 的子区域/预算验证须实测，不从 CPU payload 推断。完成后关闭 Review、重建并执行普通 CTest；产品拒绝 --verify-materials。本平台构建、Metal、图像及性能执行结果：待用户填写。

## R10A RHI 不透明 GPU 用户验证（未执行）

新增审计在本平台 `verification/*-static/rhi-opaque/`，结论为“静态审计通过，实机待用户验证”。本轮实现位于 `src/render/rhi/`，产品替换仍由 R10D 执行。在目标机最新原生 Kit 的 Release Review 环境中运行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R '^rhi-opaque-gpu$' --output-on-failure
```

此测试使用同机当前 Quick3D 和 RHI 真实窗口/读回，核对报告中的实际后端、36 组双主题/三种封面/前低视角纸面和法线/深度、已知平面深度误差、四角投影 <=2px，以及九尺寸/恢复/30 次连续 resize。报告输出 `build/release/verification/rhi/runtime.json` 与成对 PNG。测试指定 DPR1；若桌面仍得到其他实际 DPR，按真实比例调整 oracle 和目标大小后重新采集，不冒充 DPR1 已通过。阴影六对对应冻结 R10 的 1024/2048/4096 双主题 receiver；查看 `.32` 阴影强度与目标图，不能将白色 receiver 掩码用于全壳体材质验收。MSAA capture/resolve、壳体和后处理分别由 R10B/C/D 的对应检查完成。结束后关闭 Review、重新构建并执行普通 Release CTest。目标平台的实际编译、GPU、图像、尺寸/DPR 检查结果：待用户填写。
