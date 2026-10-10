# Linux · R02 用户实机验证清单

R02 结论：**静态审计通过，实机待用户验证**。矩阵见 `verification/linux-static/audit-matrix.json`。以下均未在 Linux 执行，真实结果由用户填写；完整部署和系统依赖闭包在 R32 接入。

安装本机最新原生 Qt Desktop Kit（包含 Core/Gui/Qml/Quick/QuickControls2/QuickDialogs2/Quick3D/Multimedia/Concurrent/Svg/ShaderTools/Test/QuickTest）、支持 C++23 的原生编译器、CMake 与 Ninja。Qt 官方安装布局默认枚举 `$HOME/Qt`、`/opt/Qt`、`/usr/local/Qt`；自定义安装根使用 `YALAHOO_QT_ROOTS`。Kit 的 qmake/mkspec/CMake 配置须同属完整前缀；不从 PATH 拼凑不同安装的 Qt。

```sh
sh tools/build/build.sh qt-debug
sh tools/build/build.sh qt-release
```

预期：两配置使用同一最高版本，C++23 探针通过，全部 CTest 通过，qsb 包存在 GLSL 150 变体。升级后可追加 `--fresh`。

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/debug/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=xcb ./build/debug/bin/yalahoo
# 在真实 Wayland 会话中另行验证：
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=wayland ./build/debug/bin/yalahoo
```

预期：标题 `yalahOO!!!`、中文正确、按钮响应、立体几何显示，resize/关闭正常；Qt 使用 OpenGL，XDG 数据目录由 QStandardPaths 正确解析。Qt Kit 的平台插件和系统图形依赖须可用，CJK 系统字体暂须安装。当前使用开发 Kit 的媒体插件且不打开来源，不应发声、启动 Node/浏览器/HTTP 服务或发送请求。

运行证据在 `build/debug/verification/runtime.json` / `runtime.png`。如果驱动、QPA 或媒体插件失败，保留 CTest 输出诊断。R02 未验证实际音频播放、设备切换、Vulkan、生产场景或独立部署包。

实机结果：待用户验证。
