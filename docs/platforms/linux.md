# Linux · 用户实机验证清单

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

预期：标题 `yalahOO!!!`、中文正确、按钮响应、原 GLB 三壳体导入几何显示，resize/关闭正常；Qt 使用 OpenGL，XDG 数据目录由 QStandardPaths 正确解析。Qt Kit 的平台插件和系统图形依赖须可用，中文统一使用 qrc MiSans。当前使用开发 Kit 的媒体插件且不打开来源，不应发声、启动 Node/浏览器/HTTP 服务或发送请求。

运行证据在 `build/debug/verification/runtime.json` / `runtime.png`。如果驱动、QPA 或媒体插件失败，保留 CTest 输出诊断。R02 未验证实际音频播放、设备切换、Vulkan、生产场景或独立部署包。

实机结果：待用户验证。

## R03 资源验证

结论：**静态审计通过，实机待用户验证**。新增矩阵见 `verification/linux-static/assets/audit-matrix.json`；同源字体与资产路径见 `docs/contracts/assets.md`。没有在 Linux 实际运行。

```sh
ctest --preset qt-debug -R assets --output-on-failure
sh tools/assets/prepare-assets.sh qt-debug
sh tools/build/build.sh qt-debug
```

预期：47 项 qrc hash、三壳体/1,000 三角形/纸面间隙通过；四个 MiSans 字重的 CFF 表与实际字体选择一致，中文/拉丁覆盖与整行宽度检查通过；八个功能 SVG 双主题有效；四个完整音频资源进入 LoadedMedia，duration 有效，不创建 QAudioOutput、不播放。POSIX 准备脚本只调用该 Kit 工具，验证后复用可跨平台 SFNT，不链接 Dwrite。实际报告和字体/图标预览在 `build/debug/verification/`。

在 X11/xcb 与真实 Wayland 会话中分别从带中文/空格的任意目录启动绝对程序路径，确认 qrc 资源、字体和三壳体均正常。媒体加载不代表歌曲播放/设备验收，R24/R25 再实现相应检查。
