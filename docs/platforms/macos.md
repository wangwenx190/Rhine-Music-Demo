# macOS · 开发、动态验证与实机清单

当前主力开发与验证环境为macOS。以下历史“静态审计通过，实机待用户验证”只说明对应轮次当时的覆盖；后续逐项按实际系统/Kit/后端/设备/采集能力判断，可靠构建/字体/Metal离屏与可用部署检查必须真实执行。不可执行或不可靠的检查严格审计并完整移交最后R34，不能将整个macOS统称为只能静态，也不删除实机要求。

## Apple Silicon 换机接续入口

源码、QML、shader、原创品牌、四字重 MiSans OTF/原 WOFF2、资源来源和许可、匿名测试输入、构建工具与冻结 Git 来源映射均已入库。完整克隆 `my` 分支即可接续；不复制 Windows 的 `build/`、Kit、CMakeCache、CMakeUserPresets、node_modules 或私人音乐/数据。`verification/`、`docs/media/`、`docs/contracts/web-scenes/` 是可重建本地输出，未入库；Windows 当前五项文字失败的最小数值、案例和复现方法保存在 [title-reels.md](../contracts/title-reels.md)，其原图仅保留在原机器。若需研究某张原图，另行定点搬运，不全量提交。

```sh
git clone --single-branch --branch my https://github.com/wangwenx190/Rhine-Music-Demo.git
cd Rhine-Music-Demo
git cat-file -e df251a2eb0406b1ccde11a0e9f8a5017c0293185^{commit}
uname -m
```

不得使用浅克隆；冻结工具需要祖先提交，原始来源 SHA 经 `docs/contracts/history-rewrite.json` 解析。Apple Silicon 终端的 `uname -m` 应为 `arm64`，关闭终端的 Rosetta 模式。准备支持项目 C++23 探针的 Xcode/Command Line Tools、CMake（满足根最低版本）、Ninja、能读取根 `.clang-format` 的现代 clang-format，以及本机最新完整 Qt Desktop Kit（含 CanvasPainter 和下文全部模块、private headers）。Qt 的 Universal Kit 可用，其 `QT_ARCHS` 的 arm64 切片参与发现；应用仍构建单一原生架构。若 C++23、最新 Kit 模块或 formatter 检查失败，补齐本机工具，不能降级 Qt/标准或关闭门禁。自定义 Qt 安装根用 `YALAHOO_QT_ROOTS`，formatter 不在 PATH 时可向 configure 传 `-DYALAHOO_CLANG_FORMAT=/本机路径/clang-format`，不写入公共 presets。

当前用户开发机定位提示：Qt 安装在 `/Users/zhaoyuhang/Qt/6.12.0/macos`，默认 `$HOME/Qt` 枚举可找到它；此处记录不锁定版本。Homebrew LLVM 的 formatter 可用 `/opt/homebrew/opt/llvm/bin/clang-format`，首次 configure 可追加 `-DYALAHOO_CLANG_FORMAT=/opt/homebrew/opt/llvm/bin/clang-format`。后续 session 先重新查询 Kit 和工具实际版本，不复用别台机器的 cache。

```sh
sh tools/build/build.sh qt-release --fresh -DYALAHOO_REVIEW=OFF
cmake --build --preset qt-release --target check-format
cmake --build --preset qt-release --target Yalahoo_qmllint ui-audit_qmllint
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/release/CMakeCache.txt)
if [ -x "$kit/bin/qsb" ]; then
    qsb="$kit/bin/qsb"
else
    qsb="$kit/libexec/qsb"
fi
"$qsb" --dump build/release/src/app/.qsb/reel-slot.frag.qsb
```

检查 qsb 输出含 MSL 变体。初次构建用 Review=OFF；这仍构建 `ui-audit` 和已启用基础 CTest，不要求携带旧 GPU 采集产物。记录实际 Kit、编译器、arm64、CTest 和 Metal 结果；现有 Windows 27 项通过只证明 Windows。当前开发 bundle 可启动，但独立部署、根 `.command`、完整平台菜单与媒体验收仍属 R31/R24 等后续轮次，不把本入口当作完成的安装包。

用户主动查看应用时：

```sh
QT_PLUGIN_PATH="$kit/plugins" build/release/bin/yalahoo.app/Contents/MacOS/yalahoo \
    --data-dir "$PWD/verification/macos/manual-data"
```

同机标题诊断另需符合 `legacy-web/package.json` engines 的原生 Node 和已安装 Chrome/Edge；它们仅供开发采集，播放器不依赖它们。以下只在冻结 Web 根安装已有 lockfile 的开发依赖，不改其源码；浏览器全程 headless，仅访问本机匿名测试服务，不下载浏览器。可用 `REVIEW_BROWSER` 指定已有浏览器可执行文件。

```sh
(cd legacy-web && npm ci --no-audit --no-fund)
node tools/review/export-title.mjs --dpr 2 --output verification/macos/title-reels/web
node tools/build/verify-title.mjs --backend metal --dpr 2 \
    --source verification/macos/title-reels/web --diagnostics
```

采集器记录本机来源平台/架构；原生入口支持 `ui-audit --titles 输出目录 源参照目录`，报告真实 `metal` 后端。包装工具自动查询 native screen DPR、按目标/原生 DPR 校准 scale，检查20项文字结构、18项页眉、每路径三组120帧、缓存/隐藏工作、十次恢复及最终释放，不放宽像素门槛。`--diagnostics` 只接受实际像素退出6且其余门禁通过，仍写 `passed=false` 和待修案例；正常像素验收须去掉该参数。每次使用新输出目录保留失败样本，原始生成物保持忽略。Qt 验证窗口使用既有置底/拒绝焦点契约，不采集用户桌面。

本机 Apple Silicon 接续已实际执行：macOS 27.0.1、arm64、Qt 6.12.0、AppleClang 21.0.0、C++23 Release；C++23/QRhi/字形私有接口探针、构建、格式、两个qmllint及29份qsb的GLSL/HLSL/MSL变体检查通过。已安装Homebrew Node 26.11.1/npm并使用现有Edge 155.0.4283.45生成同机20项DPR2冻结参照；没有增加产品运行依赖。字体资产测试按原OS/2表和实际物理字形身份验证，品牌资产hash不变，仅适配跨平台一个RGB量化单位的栅格差异。

按用户最新要求，macOS优先真正离屏验证，验证方式逐项按可靠能力决定；当前尚无可靠入口的可见窗口、呈现/VSYNC、交互项目先严格审计并集中到最后R34，已有可靠本机入口及后续可可靠执行的部署检查须真实运行。默认`YALAHOO_MACOS_VISIBLE_TESTS=OFF`，当前四项runtime CTest明确disabled/deferred；不计为通过。以下历史清单中当前不可靠的手工可见/呈现项目属于R34输入；可可靠执行的当前主线检查按轮次顺序执行。普通产品由用户主动启动的行为不变。

Release CTest的23项已启用测试全部通过，4项可见runtime入口明确暂缓，未算作通过。R16M2用Qt公开emoji字体策略对齐同机浏览器的`Apple Color Emoji / AppleColorEmoji`，修复原case10/11的16.078125逻辑px累积误差；现在20项生产结构/文本、18项页眉、三路径各三次120帧有界诊断、隐藏候选零文字绘制、十次离屏恢复/最终释放及隐藏窗口策略通过。case10/11的固定1物理px带外像素也通过；完整诊断`diagnosticsPassed=true`，像素整体`passed=false`，剩余case6/7、14/15、16/17保留在R16M2P。字体证据、最小失败数值和复现入口见[title-reels.md](../contracts/title-reels.md)，静态源码/性能入口为`node tools/audit/audit-title.mjs 完整诊断目录`。离屏frame完成不是present，GUI线程sync/render成本不能当作正常VSYNC或产品render-thread性能；静态矩阵与macOS实际运行覆盖分别记录，尚不可靠的可见实机项目待R34。D3D11/其他不可执行的后端补修已移入R34，R17依赖当前主线R16M2P/M3阶段验收。

本次换机入口在 Windows Release 实测：Universal/单架构 Kit 元数据回归通过；自定义源目录与DPR校准后的D3D11/DPR1标题全通过，DPR1.5诊断仍准确保留case6/7、14/15、17的像素失败。普通CTest曾在Canvas读回阶段失败，原生权限或仅指定threaded重试仍失败；完整使用迁移验收的 `QSG_RENDER_LOOP=threaded` 与 `QT_SCALE_FACTOR=1` 后通过，已写入 ui-drawing 的 CTest 环境。factor1保留屏幕实际DPR1.5，不强制DPR1；Qt内部为何依赖显式factor尚未确认，未放宽图像/读回门槛。浏览器受限环境页面连接失败，原生 headless 环境成功生成20项；这两种失败原因不能混为一谈。最终27项Release CTest、check-format、两组qmllint及qsb的GLSL/HLSL/MSL变体检查通过。此记录不代表Metal实机通过。

## 真实列、导航与偏好（R13 实机待验证）

本阶段结论：**静态审计通过，实机待用户验证**。逐项调用链与内容 hash 在 `verification/macos-static/real-columns/`；实现契约见 [real-columns.md](../contracts/real-columns.md)。目标机使用最新原生 Kit 执行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R '^(real-columns|archive-pool|lighting|motion|source-format)$' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

GPU 源码对照须先用已冻结 Web 开发依赖执行 `node tools/review/export-real-columns-golden.mjs`（仅生成匿名测试输入，应用运行不需要 Node），再执行 `QSG_RHI_BACKEND=metal ctest --preset qt-release -R '^real-columns-gpu$' --output-on-failure`。Review 工具保持置底/不接受焦点，PNG 仅来自自身 GPU 读回。核对真实 Metal 后端、sourceMatrixError≤1e-6、sourceSsim≥0.97、sourceMae≤6、稳定零上传、双主题资源复用与隐藏/重建；不能用 Windows 或 Windows OpenGL 报告填入目标机结果。

完成后以 `cmake --preset qt-release -DYALAHOO_REVIEW=OFF` 恢复生产 Release，重建并执行全部 CTest。用户主动启动 `build/release/bin/yalahoo.app/Contents/MacOS/yalahoo --data-dir "$PWD/verification/manual-navigation"`：验证演示库真实/填充切换、首尾键盘/滚轮边界、独立列居中与记忆开关，关闭后重开只恢复开关，不恢复列位。若有触摸设备，再测逻辑像素阈值与首尾；检查本机 Unicode/空格路径和损坏 preferences.json 保留/报错。完整长列的 49/106 张输入由 Review 入口验证。实际编译、窗口管理器、DPR、GPU、偏好写入与触摸结果：待用户填写。

R02 结论：**静态审计通过，实机待用户验证**。矩阵见 `verification/macos-static/audit-matrix.json`。以下均未在 macOS 执行，真实结果由用户填写；完整产品和 macdeployqt 部署在 R31 接入。

安装本机最新原生 Qt Desktop Kit（包含 Core/Gui/Qml/Quick/QuickControls2/QuickDialogs2/Quick3D/Multimedia/Concurrent/Svg/ShaderTools/CanvasPainter/Test/QuickTest）、Xcode C++23 工具链、CMake 与 Ninja。默认枚举 `$HOME/Qt`、`/opt/Qt`、`/usr/local/Qt`；自定义安装根使用 `YALAHOO_QT_ROOTS`。仅使用当前主机的单一原生架构。

```sh
sh tools/build/build.sh qt-release
```

预期：Release 自动选最高完整原生版本，C++23 探针通过；CTest 的身份、Kit、模块、qsb、实际窗口测试全部通过。qsb 检查存在 MSL 变体。首次使用编译器或升级 Kit 后，可追加 `--fresh`。

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

## R10B RHI 材质 GPU 用户验证（未执行）

新增审计在 `verification/macos-static/rhi-shell/`，结论为“静态审计通过，实机待用户验证”。目标机最新原生 Kit 的 Release 环境运行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'rhi-material-parameters|runtime-rhi-shell-quick3d|rhi-shell-gpu|rhi-opaque-gpu' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

核对实际 Metal、DPR1、61 个 sourceIndex/同封面同相机的五 ROI、6 对壳体阴影、2 个邻盒法线/深度、倒序/同深度 ID、捕获隔离与 alpha-1x/4x 边缘。报告和 PNG 在 `build/release/verification/rhi-shell/`，材质残差与未接入的后处理按 [rhi-shell.md](../contracts/rhi-shell.md) 区分；passed 不表示全部 ROI 已达 .97/6，也不表示产品后端已替换。检查 4× float MRT 的真实创建/resolve/mips，缺能力应明确失败，不降级冒充通过。Retina 若需调整 DPR，必须重采同输入源图并调整实际像素目标，不能拿 Windows 图当 macOS 图像证据。结束后关闭 Review 重建普通 Release，运行全部 CTest。目标机真实构建、Metal、图像和部署结果：待用户填写。

## R10C 图集与后处理用户验证（未执行）

本平台静态审计结论见 verification/macos-static/rhi-postprocess/，必须标为“静态审计通过，实机待用户验证”。使用最新原生 Kit：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release
ctest --preset qt-release -R 'rhi-atlas-queue|rhi-material-parameters|rhi-opaque-gpu|runtime-rhi-shell-quick3d|rhi-shell-gpu|rhi-postprocess-gpu' --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

检查真实 Metal、DPR1、same-hash source fixture、全部 61 pre-stage 与 50 final sourceIndex/five ROI、raw AO/blur/normal，以及 432 实体初始化/变色/稳定/还原/倒序/resize/generation。目标格式、4× MSAA resolve、阴影 comparison、独立几何深度和 pass 次序见 [rhi-postprocess.md](../contracts/rhi-postprocess.md)。缺能力必须给出创建错误，不隐降画质。D3D11 原生命令字节在本平台不可用，报告必须保留 available=false，不把请求统计当作本平台原生传输测量；CPU 队列限额与真实整页 GPU 像素证明仍应执行。

性能报告分别标注未限速吞吐、系统呈现节奏、CPU 提交和 timestamp（不可用时不能写零）。真实桌面 compositor/DPR/设备性能需本机验证，Windows 图像及帧时间不作为本平台结果。完成后关闭 Review，重建普通 Release 并执行 CTest。实际构建、GPU、图像、性能与部署结果：待用户填写。

## R10D 生产 RHI 集成用户验证（未执行）

本平台对应矩阵在 `verification/macos-static/rhi-integration/`：**静态审计通过，实机待用户验证**。在目标机最新原生 Kit 执行 Release：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=ON
cmake --build --preset qt-release --target format
cmake --build --preset qt-release
cmake --build --preset qt-release --target check-format Yalahoo_qmllint
ctest --preset qt-release --output-on-failure
QSG_RHI_BACKEND=metal build/release/bin/yalahoo.app/Contents/MacOS/yalahoo --verify-integration build/release/verification/rhi-integration/native-run --integration-seconds 120
```

核对 `runtime.json` 的实际后端、client 尺寸、DPR、四个输入 revision 对应提交、独立 frameSwapped 时间序列、九尺寸/四画质/十次真实场景图释放恢复。窗口全程置底、不接受焦点，PNG 只来自自身渲染；目标窗口管理器不支持策略时真实报告失败，不自动激活。资源清单是描述符估算，UnknownFormat 深度附件为明确上界，不冒充实测显存；正常 VSYNC 单盒数据不代表完整阵列/搜索/扫描产品或 input-to-photon。qsb 格式支持、颜色/深度/resolve 和驱动节奏均需实机检查。正式采样按三个独立目录串行执行并保留全部结果。

结束后 `cmake --preset qt-release -DYALAHOO_REVIEW=OFF`，重建、运行全部 Release CTest 和 qmllint。普通产品不得包含 ResearchApp/GeometryScene/FixedTextureData 或 Quick3D 链接依赖；共享资源文件保留在仓库。完整平台部署仍使用后续平台轮次入口。本清单的目标机运行结果：待用户填写。

## 封面服务与图集实机验证

本阶段结论：静态审计通过，实机待用户验证。配置 `cmake --preset qt-release -DYALAHOO_REVIEW=ON`，构建 `cmake --build --preset qt-release` 后，执行 `ctest --preset qt-release -R '^(cover-cache|cover-gpu)$' --output-on-failure`。检查本地 `build/release/verification/covers/runtime.json`：windowPolicy 通过、实际后端正确、未上传/旧租约不显示、稳定帧零上传、scene graph 恢复成功；原生 D3D11 command introspection 在本平台不可用，不能伪报 destinationBytes 实测。用 Unicode/空格本地路径及当前 Qt 支持的 PNG/JPEG 验证 reader、ICC、缺图和展开保护，确认不存在其他专辑封面串用。详见 `docs/contracts/covers.md`。

验证后恢复 `cmake --preset qt-release -DYALAHOO_REVIEW=OFF`，重建并执行全部 Release CTest；不以 Windows 成绩代替目标机 codec、GPU、窗口管理器或性能验证。

## R14 二维控件与 Canvas Painter 用户验证（未执行）

本平台结论：**静态审计通过，实机待用户验证**。共同源码、插件闭包、渲染线程所有权和 PERF-01–07 矩阵在 `verification/macos-static/browse-ui/`；绘制范围与已知差异见 [二维绘制契约](../contracts/canvas-painter.md)。目标机自动发现最新原生 Kit 后执行：

```sh
cmake --preset qt-release -DYALAHOO_REVIEW=OFF
cmake --build --preset qt-release
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
ctest --preset qt-release --output-on-failure
QSG_RHI_BACKEND=metal build/release/tools/ui-audit build/release/verification/browse-ui/native
QSG_RHI_BACKEND=metal build/release/bin/yalahoo.app/Contents/MacOS/yalahoo --verify-browse build/release/verification/browse-ui/production
```

对 DPR 1/1.5/2 应先查询 `ui-audit --native-dpr`，将 QT_SCALE_FACTOR 设为目标 DPR / 查询值，并核对报告的实际 DPR。检查九尺寸双主题、短屏滚动、SVG 与字体/emoji 回退、主题按钮输入、无焦点窗口策略，以及双 API 静止、hide/restore/resize/十次重建结果。PNG 只采集自身渲染或提交的 swapchain，禁止桌面截屏或激活窗口。窗口管理器不支持置底/拒绝焦点时须报告实际失败；目标平台的颜色、GPU 时间及呈现不能由 Windows 成绩代替。此处实际结果：待用户填写。
