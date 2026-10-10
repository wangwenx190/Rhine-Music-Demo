# R02 · 原生工程与工具链契约

根工程为 C++23 / Qt Quick 原生工程。当前 `App.qml` 是最小运行探针：中文、Basic 控件、Qt Quick 3D 几何、qsb ShaderEffect 和无来源的 QtMultimedia 对象；音乐库、模型/字体、品牌与播放功能按后续轮次实现。

- CMake project：`yalahoo`；目标：`YalahooCore`、`Yalahoo`；输出：`build/<configuration>/bin/yalahoo.exe`，macOS 为同位置的 `yalahoo.app`，Linux 为 `yalahoo`。
- QML URI：`Yalahoo.Music`；入口：`loadFromModule("Yalahoo.Music", "App")`；资源：`:/qt/qml/Yalahoo/Music/App.qml`。源目录中的 `App.qml` 设置相对别名，不依赖当前工作目录。
- 非 QML shader：`resources/shaders/probe.frag` → 同 Kit `qsb` → `:/assets/shaders/probe.frag.qsb`。Qt 标准默认变体包含 SPIR-V、GLSL、HLSL、MSL。
- organization/applicationName 均为 `yalahoo`，display/title 为 `yalahOO!!!`。身份设置在 QGuiApplication、引擎与 QStandardPaths 之前；更改展示名不改数据目录。探针不创建或读取用户音乐库。
- Windows 仅在自己的分支选择 D3D11，macOS 选择 Metal，Linux 选择 OpenGL；全平台采用逻辑像素和 Qt 公开 API。平台选项在 `cmake/Platform.cmake` 隔离，macOS bundle 使用 `cmake/Info.plist.in`。

## 最新 Kit 与编译器

公共 presets 为 `qt-debug` / `qt-release`，二者分别输出到 `build/debug/` / `build/release/`。Qt 模块不固定 minor/patch 或安装目录。`cmake/QtKit.cmake` 读取本机 Qt CMake 配置的真实版本，与每个 Kit 自带 `qmake -query QT_VERSION` 交叉核对；用 qconfig 架构/交叉编译标记及 `QMAKE_XSPEC` 排除 Android/Wasm/交叉 Kit。按数值版本选原生平台/架构的最高版本，再验证 ABI 和全部必需模块。最新 Kit 缺模块或 ABI 不匹配会报错，不退回旧版。

可通过本机环境 `YALAHOO_QT_ROOTS` 或被忽略的 `CMakeUserPresets.json` 中同名 cache variable 增加安装根，列表分隔符为分号。可设置 `YALAHOO_QT_KIT` 显式前缀，但它必须等于已发现的最新 Kit；自定义 Kit 路径不会绕过已安装版本枚举。默认搜索 Qt 官方安装布局的 `C:/Qt` / 用户 Qt 目录、Unix 用户 Qt 目录和 `/opt/Qt`、`/usr/local/Qt`。系统发行版的非标准布局须提供具有 Qt CMake、mkspec 与 qmake 的完整前缀；不混用任意 PATH 中的 Qt 工具。

configure 每次重新发现 Kit；旧 `YALAHOO_SELECTED_QT`、任意已缓存的 `Qt6*_DIR` 或显式过期前缀都会阻止配置。升级后移除旧覆盖并执行 `cmake --preset qt-debug --fresh` / `qt-release --fresh`。qmake、qmllint、qsb 从该 Kit 的 bin/libexec 定位并校验实际版本；构建用的 Qt imported tool targets 来自同一套配置。balsam 和部署工具在执行对应资产/部署轮次时加入同 Kit 校验。

C++23 required、无扩展，并实际编译 `std::to_underlying` 和 `std::optional::transform`。MSVC 的 CMake C++23 映射可能为 `/std:c++latest`；能力探针失败时配置直接停止。Windows 脚本通过 vswhere 找实际 VS C++ 安装，导入 VsDevCmd 环境到当前进程；CMake/Ninja 从现有 PATH 或标准 CMake 安装位置发现，不修改全局环境。

## 构建、测试与运行

Windows 在 PowerShell 中执行：

```powershell
./tools/build/build.ps1 -Preset qt-debug
./tools/build/build.ps1 -Preset qt-release
```

脚本执行以下三个统一命令。需要自行使用它们时，先在当前 PowerShell dot-source 编译器环境：

```powershell
. ./tools/build/enter-msvc.ps1
cmake --preset qt-debug
cmake --build --preset qt-debug
ctest --preset qt-debug --output-on-failure
```

本阶段开发运行使用选中 Kit 的 DLL/插件，不是独立部署包。从 cache 获取已选择前缀后启动：

```powershell
$selectedKit = (Get-Content build/debug/CMakeCache.txt | Select-String '^YALAHOO_SELECTED_QT:PATH=(.*)$').Matches.Groups[1].Value
$env:PATH = "$selectedKit/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$selectedKit/plugins"
& ./build/debug/bin/yalahoo.exe
```

macOS/Linux 使用 `sh tools/build/build.sh qt-debug`；详见平台实机清单。原生程序不依赖 Node、浏览器或 HTTP 服务。旧 Web 参照仍在 `legacy-web/` 使用冻结依赖；新构建不写入归档。

CTest 分别验证身份稳定性、合成 Kit 清单/版本数值/平台/架构/ABI/模块/过期覆盖、独立 Quick3D 链接与无声媒体插件、实际 qsb 变体，以及原生窗口的真实渲染/鼠标点击。`--verify-runtime <report.json>` 在窗口创建后点击 Basic 按钮，记录 RHI 类型、渲染帧、真实模型像素、中文 glyph、QML/qsb 资源和媒体可用性，并写同目录 `runtime.png`；错误通过退出码和 Qt 诊断暴露。该选项只作本地测试，不设置来源或播放音频。

完整 Windows 证据重建：`./tools/build/verify-r02.ps1`。输出到 `verification/windows/native-bootstrap/`、截图到 `docs/media/native-bootstrap/`；用户 profile 和数据/缓存绝对路径在入库前匿名化。macOS/Linux 自动覆盖检查：`./tools/audit/audit-r02.ps1`，规则中的逐分支人工推演与 hash 写入各平台矩阵。静态报告仅覆盖 R02，后续轮次必须扩展。
