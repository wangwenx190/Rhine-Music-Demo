# macOS · R02 用户实机验证清单

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

构建期 `build/debug/verification/runtime.json` / `runtime.png` 记录实际结果。该平台系统字体暂须含中文；R03 再接入统一生产字体。当前开发 bundle 依赖选中 Kit，不是已完成的独立安装包。没有验证实际播放、格式兼容、设备切换、产品菜单或最终模型/光效。

实机结果：待用户验证。
