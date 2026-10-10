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
