# 原生Kit、工程与构建

本文只维护Kit发现、C++23能力、构建/模块与开发运行。产品范围见迁移计划，渲染接线见rhi-integration.md，组件验证见各契约。

- CMake project：`yalahoo`；目标：`YalahooCore`、`Yalahoo`；输出：`build/<configuration>/bin/yalahoo.exe`，macOS 为同位置的 `yalahoo.app`，Linux 为 `yalahoo`。
- QML URI：`Yalahoo.Music`；入口：`loadFromModule("Yalahoo.Music", "App")`；资源：`:/qt/qml/Yalahoo/Music/App.qml`。源目录中的 `App.qml` 设置相对别名，不依赖当前工作目录。
- 非 QML shader：`resources/shaders/probe.frag` → 同 Kit `qsb` → `:/assets/shaders/probe.frag.qsb`。Qt 标准默认变体包含 SPIR-V、GLSL、HLSL、MSL。
- organization/applicationName 均为 `yalahoo`，display/title 为 `yalahOO!!!`。身份设置在 QGuiApplication、引擎与 QStandardPaths 之前；更改展示名不改数据目录。探针不创建或读取用户音乐库。
- Windows 仅在自己的分支选择 D3D11，macOS 选择 Metal，Linux 选择 OpenGL；全平台采用逻辑像素和 Qt 公开 API。平台选项在 `cmake/Platform.cmake` 隔离，macOS bundle 使用 `cmake/Info.plist.in`。

## 最新 Kit 与编译器

公共 presets 保留 `qt-debug` / `qt-release`，分别输出到 `build/debug/` / `build/release/`。常规构建、测试、GUI/性能验证与验收只使用 Release；只有明确需要获取堆栈时才编译和运行 Debug，不执行例行双配置验证。Qt 模块不固定 minor/patch 或安装目录。`cmake/QtKit.cmake` 读取本机 Qt CMake 配置的真实版本，与每个 Kit 自带 `qmake -query QT_VERSION` 交叉核对；用 qconfig 架构/交叉编译标记及 `QMAKE_XSPEC` 排除 Android/Wasm/交叉 Kit。按数值版本选原生平台/架构的最高版本，再验证 ABI 和全部必需模块。最新 Kit 缺模块或 ABI 不匹配会报错，不退回旧版。

可通过本机环境 `YALAHOO_QT_ROOTS` 或被忽略的 `CMakeUserPresets.json` 中同名 cache variable 增加安装根，列表分隔符为分号。可设置 `YALAHOO_QT_KIT` 显式前缀，但它必须等于已发现的最新 Kit；自定义 Kit 路径不会绕过已安装版本枚举。默认搜索 Qt 官方安装布局的 `C:/Qt` / 用户 Qt 目录、Unix 用户 Qt 目录和 `/opt/Qt`、`/usr/local/Qt`。系统发行版的非标准布局须提供具有 Qt CMake、mkspec 与 qmake 的完整前缀；不混用任意 PATH 中的 Qt 工具。

configure 每次重新发现 Kit；旧 `YALAHOO_SELECTED_QT`、任意已缓存的 `Qt6*_DIR` 或显式过期前缀都会阻止配置。升级后移除旧覆盖并执行 `cmake --preset qt-release --fresh`。qmake、qmllint、qsb 从该 Kit 的 bin/libexec 定位并校验实际版本；构建用的 Qt imported tool targets 来自同一套配置。balsam 和部署工具在执行对应资产/部署轮次时加入同 Kit 校验。

C++23 required、无扩展，并实际编译 `std::to_underlying` 和 `std::optional::transform`。MSVC 的 CMake C++23 映射可能为 `/std:c++latest`；能力探针失败时配置直接停止。Windows 脚本通过 vswhere 找实际 VS C++ 安装，导入 VsDevCmd 环境到当前进程；CMake/Ninja 从现有 PATH 或标准 CMake 安装位置发现，不修改全局环境。

## 构建、测试与运行

Windows 在 PowerShell 中执行：

```powershell
./tools/build/build.ps1 -Preset qt-release
```

脚本执行以下三个统一命令。需要自行使用它们时，先在当前 PowerShell dot-source 编译器环境：

```powershell
. ./tools/build/enter-msvc.ps1
cmake --preset qt-release
cmake --build --preset qt-release
ctest --preset qt-release -R '对应受影响测试' --output-on-failure
```

本阶段开发运行使用选中 Kit 的 DLL/插件，不是独立部署包。从 cache 获取已选择前缀后启动：

```powershell
$selectedKit = (Get-Content build/release/CMakeCache.txt | Select-String '^YALAHOO_SELECTED_QT:PATH=(.*)$').Matches.Groups[1].Value
$env:PATH = "$selectedKit/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$selectedKit/plugins"
& ./build/release/bin/yalahoo.exe
```

macOS/Linux 使用 `sh tools/build/build.sh qt-release`；详见平台实机清单。原生程序不依赖 Node、浏览器或 HTTP 服务。旧 Web 参照仍在 `legacy-web/` 使用冻结依赖；新构建不写入归档。

Review另要求同Kit的Quick3DPrivate/Quick3DRuntimeRenderPrivate，仅用于隔离的Review MSAA能力与真实附件探针；`YALAHOO_REVIEW_MSAA_API`配置期按实际头文件编译，缺失时明确失败，不降级Kit。普通产品不链接这些Review模块。

基础CTest覆盖Kit数值排序/平台/架构/ABI/模块/过期覆盖、身份/模块插件和qsb变体；具体选择见test-gates.md，格式命令见code-style.md。

`--verify-runtime 报告路径`为Review基础资源/窗口探针，可组合motion/library/shading参数；不设置歌曲来源或播放音频。它不替代相应生产组件/平台验收。工具输出位置与字段以实现及test-gates为准，不在本契约复制各轮次通过记录。

## 启动入口

根`yalahoo.command`/`yalahoo.sh`/`yalahoo.ps1`启动普通原生音乐入口；`tools/launchers/demo.*`与`lighting-lab.*`分别传`--demo`和`--lighting-lab`。共用run-native脚本相对自身位置解析Release产物与cache中所选Kit，保留参数、Unicode/空格路径及进程退出码；PowerShell使用Process对象等待GUI进程。开发启动器不自动构建或发布。

`--demo`与`--lighting-lab`只创建九列二十七张匿名qrc封面专辑，不读取/保存偏好或真实库，不扫描、不自动启动歌曲/BGM/音效；后者额外展开实验容器。不能与数据根、扫描/导入或验证参数组合。普通入口的设置按钮仍可打开同一调节面板。启动模式不改变渲染后端、普通片头或已有播放接口；演示曲目为空，不能伪装真实歌曲来源。
