# Linux · 用户实机验证清单

R02 结论：**静态审计通过，实机待用户验证**。矩阵见 `verification/linux-static/audit-matrix.json`。以下均未在 Linux 执行，真实结果由用户填写；完整部署和系统依赖闭包在 R32 接入。

安装本机最新原生 Qt Desktop Kit（包含 Core/Gui/Qml/Quick/QuickControls2/QuickDialogs2/Quick3D/Multimedia/Concurrent/Svg/ShaderTools/Test/QuickTest）、支持 C++23 的原生编译器、CMake 与 Ninja。Qt 官方安装布局默认枚举 `$HOME/Qt`、`/opt/Qt`、`/usr/local/Qt`；自定义安装根使用 `YALAHOO_QT_ROOTS`。Kit 的 qmake/mkspec/CMake 配置须同属完整前缀；不从 PATH 拼凑不同安装的 Qt。

```sh
sh tools/build/build.sh qt-release
sh tools/build/build.sh qt-release
```

预期：两配置使用同一最高版本，C++23 探针通过，全部 CTest 通过，qsb 包存在 GLSL 150 变体。升级后可追加 `--fresh`。

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/release/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=xcb ./build/release/bin/yalahoo
# 在真实 Wayland 会话中另行验证：
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=wayland ./build/release/bin/yalahoo
```

预期：标题 `yalahOO!!!`、中文正确、按钮响应、原 GLB 三壳体导入几何显示，resize/关闭正常；Qt 使用 OpenGL，XDG 数据目录由 QStandardPaths 正确解析。Qt Kit 的平台插件和系统图形依赖须可用，中文统一使用 qrc MiSans。当前使用开发 Kit 的媒体插件且不打开来源，不应发声、启动 Node/浏览器/HTTP 服务或发送请求。

运行证据在 `build/release/verification/runtime.json` / `runtime.png`。如果驱动、QPA 或媒体插件失败，保留 CTest 输出诊断。R02 未验证实际音频播放、设备切换、Vulkan、生产场景或独立部署包。

实机结果：待用户验证。

## R03 资源验证

结论：**静态审计通过，实机待用户验证**。新增矩阵见 `verification/linux-static/assets/audit-matrix.json`；同源字体与资产路径见 `docs/contracts/assets.md`。没有在 Linux 实际运行。

```sh
ctest --preset qt-release -R assets --output-on-failure
sh tools/assets/prepare-assets.sh qt-release
sh tools/build/build.sh qt-release
```

预期：47 项 qrc hash、三壳体/1,000 三角形/纸面间隙通过；四个 MiSans 字重的 CFF 表与实际字体选择一致，中文/拉丁覆盖与整行宽度检查通过；八个功能 SVG 双主题有效；四个完整音频资源进入 LoadedMedia，duration 有效，不创建 QAudioOutput、不播放。POSIX 准备脚本只调用该 Kit 工具，验证后复用可跨平台 SFNT，不链接 Dwrite。实际报告和字体/图标预览在 `build/release/verification/`。

在 X11/xcb 与真实 Wayland 会话中分别从带中文/空格的任意目录启动绝对程序路径，确认 qrc 资源、字体和三壳体均正常。媒体加载不代表歌曲播放/设备验收，R24/R25 再实现相应检查。

## R03B 品牌资产实机复核（结果待用户填写）

本轮静态审计通过，实机待用户验证。配置本机最新 Qt Kit 后执行：

```sh
sh tools/assets/export-branding.sh qt-release --window-preview
ctest --preset qt-release -R branding --output-on-failure
```

预期：带空格/Unicode 的仓库路径可用；13 SVG 加载成功，243 个尺寸/DPR/主题样本和品牌清单可复现；16px 外框/双环/内孔分离，反白单色在深背景可见；原生预览窗口可打开且 branding 测试通过。SVG 和 PNG 路径区分大小写，生成目录不可写时工具明确失败。

在系统图像查看器复核 `resources/icons/yalahoo/yalahoo.svg` 及九档 PNG，记录显示缩放和桌面环境。Qt 预览文字标签可以回退系统字体，生产品牌不依赖字体。desktop 文件/平台插件部署和主界面品牌接入属于后续轮次；本清单不宣称已有 Linux 桌面集成。

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

预期：两个 694 帧冻结数值对照及运动边界检查通过，x/v/a 误差分别不超过 1e-6。`runtime-motion` 运行真实 OpenGL/QML 窗口，自动隐藏/最小化并恢复，两次后台期间帧数和视觉时间不增长，恢复首帧 dt=0，减少动态完成最后 revision，开场独立时间到 11.6，媒体来源为空；报告在 `build/release/verification/runtime-motion.json`，配套截图为同目录 `runtime-motion.png`。在实际 X11/Wayland 桌面环境复核隐藏、最小化和恢复；如果平台发出 ApplicationHidden/Suspended，也应停止唯一帧源。仅切换焦点为 Inactive 时允许继续视觉帧。

若已有构建树新增 QObject 后保留旧 moc 列表，执行 `cmake --build --preset qt-release --clean-first` 再运行测试。以上没有在 Linux 执行；实际 GL 帧率、窗口管理器生命周期事件和产品场景仍待对应轮次/用户验证。

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

本轮静态审计见 `verification/linux-static/materials/`；结论为“静态审计通过，实机待用户验证”。它不表示本平台 shader、窗口或图像已经运行通过。

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

静态审计见 `verification/linux-static/occlusion/`；结论为“静态审计通过，实机待用户验证”。使用本机最新原生 Qt Kit，执行 Release：

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

新增严格静态审计在 `verification/linux-static/postprocess/`，结论为“静态审计通过，实机待用户验证”。使用本机最新原生 Kit，分别在 X11/xcb 与 Wayland 下执行 Release Review：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'render-quality|runtime-postprocess|postprocess-gpu-audit' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

核对 `build/release/verification/postprocess/` 三个 DPR 报告共 105 状态的实际 DPR 与 OpenGL 后端，缩放无法按请求取得时重采同输入参照。检查九尺寸双主题的 NoAA/MSAA 几何边缘覆盖和 SSAA 诊断图、纸面留白/字体清晰度、原始 MSAA 与高/极高 SMAA 的实际作用范围；Bokeh 前/后景开关应改变像素，焦距随相机/实体位置变化。反复切四预设以及关闭 DOF 再启用 SMAA，不能崩溃/黑帧/重置导航。检查 RGB24 全分辨率深度方向、FRAMEBUFFER_Y_UP 帧缓冲变换与数值 lookup 行序，final/raw 背景差异和一次 ACES/sRGB；旧 R09 channel=5 不能用作真实线性图。残差逐项按 `docs/contracts/postprocess.md` 和限制清单检查，不把 Windows 图片当 Linux 图。

关闭 Review 后重建、执行普通 CTest，并用 `--verify-runtime <report.json> --verify-shading` 检查生产图；普通程序应拒绝 `--verify-materials`。本平台编译、OpenGL、X11/Wayland、DPR、图像、交互实测结果：待用户填写。
