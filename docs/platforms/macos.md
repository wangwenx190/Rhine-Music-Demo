# macOS · 用户实机验证清单

R02 结论：**静态审计通过，实机待用户验证**。矩阵见 `verification/macos-static/audit-matrix.json`。以下均未在 macOS 执行，真实结果由用户填写；完整产品和 macdeployqt 部署在 R31 接入。

安装本机最新原生 Qt Desktop Kit（包含 Core/Gui/Qml/Quick/QuickControls2/QuickDialogs2/Quick3D/Multimedia/Concurrent/Svg/ShaderTools/Test/QuickTest）、Xcode C++23 工具链、CMake 与 Ninja。默认枚举 `$HOME/Qt`、`/opt/Qt`、`/usr/local/Qt`；自定义安装根使用 `YALAHOO_QT_ROOTS`。仅使用当前主机的单一原生架构。

```sh
sh tools/build/build.sh qt-debug
sh tools/build/build.sh qt-release
```

预期：两配置自动选同一最高版本，C++23 探针通过；CTest 的身份、Kit、模块、qsb、实际窗口测试全部通过。qsb 检查存在 MSL 变体。首次使用编译器或升级 Kit 后，可追加 `--fresh`。

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/debug/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" ./build/debug/bin/yalahoo.app/Contents/MacOS/yalahoo
```

预期：标题与 bundle 名为 `yalahOO!!!`，中文完整、Basic 按钮点击后计数变化、中央几何有立体面；窗口在 Retina 下以逻辑像素布局，可正常 resize/关闭。Qt 使用 Metal；AppDataLocation 使用原生 Qt/macOS 位置且技术名稳定。仅创建媒体对象，不应发声、打开浏览器或联网。

构建期 `build/debug/verification/runtime.json` / `runtime.png` 记录实际结果。当前使用 qrc 中同源 MiSans，默认 Regular，标题 Bold，中央显示原 GLB 三壳体的导入几何。当前开发 bundle 依赖选中 Kit，不是已完成的独立安装包。没有验证实际播放、格式兼容、设备切换、产品菜单或最终模型/光效。

## R03 资源验证

结论：**静态审计通过，实机待用户验证**。新增矩阵见 `verification/macos-static/assets/audit-matrix.json`；资源路径及字体映射见 `docs/contracts/assets.md`。没有在 macOS 实际运行。

```sh
ctest --preset qt-debug -R assets --output-on-failure
sh tools/assets/prepare-assets.sh qt-debug
sh tools/build/build.sh qt-debug
```

预期：47 项 qrc hash、三个原壳体、1,000 三角形及纸面间隙通过；四个 MiSans 字重选中正确 CFF 表，中文/拉丁 glyph 与整行宽度检查通过；八个功能 SVG 的双主题/viewBox 有效；四个完整音频 LoadedMedia、duration 有效，全程无 QAudioOutput、无播放。准备脚本从同一最新 Kit 导入，复用已入库 SFNT 时先验证原 WOFF2/输出 hash；不需要 Dwrite。字体、图标预览和报告在 `build/debug/verification/`。

从带空格/中文的任意目录启动上述绝对 bundle 路径，确认所有资源仍显示；Metal 下确认三壳体没有缺失，四字重预览没有回退成系统字形。音频资源加载通过不代表实际播放验收，后者留给 R24/R25。

实机结果：待用户验证。
