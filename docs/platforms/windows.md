# Windows 构建、部署与验证

本文负责 Windows 工具、普通启动及部署操作；Kit/C++23 规则见[原生工程](../contracts/native-bootstrap.md)，格式和测试选择见各自契约。欠验目标及退出门槛只在迁移计划 R30-P 维护。

## 工具与构建

使用原生 Windows、PowerShell 7.2 或更新版本、Visual Studio C++ 工具（含 Windows SDK、对应架构官方 VC Redistributable）、CMake/Ninja、clang-format 与本机最新完整 Qt Desktop MSVC Kit。`enter-msvc.ps1` 通过 vswhere 初始化当前进程，Qt 由 CMake 发现，不在公共脚本固定安装路径或版本。自定义安装根使用 `YALAHOO_QT_ROOTS`，不从其他平台复制 cache。

```powershell
pwsh -NoProfile -File ./tools/build/build.ps1 -Preset qt-release -Action Configure
pwsh -NoProfile -File ./tools/build/build.ps1 -Preset qt-release -Action Build
ctest --preset qt-release -R '^(identity|kit-selection|source-format)$' --output-on-failure
pwsh -NoProfile -File ./yalahoo.ps1 --demo
```

开发启动器优先使用完整的 `out/windows/` 包，否则解析 Release cache 的 Kit；不自动构建。全部启动器从自身路径定位，逐个保留 Unicode/空格参数，等待真实 GUI Process.ExitCode。普通入口为本地音乐库，演示入口不读取私人数据；数据/扫描/导入参数见[数据契约](../contracts/library-data.md)。

## 生成生产包

```powershell
pwsh -NoProfile -File ./tools/deploy/windows.ps1
```

脚本重新发现最新 Kit，以 `build/windows-release/` 的独立生产 Release 配置构建 `Yalahoo`，强制 Review OFF。应用 CMake 同时生成 `deployment.json`，其 QML 清单来自应用实际编译列表。部署扫描副本 `build/windows-release/deploy-scan/qml/` 只包含根 `qml/` 中的生产闭包，保留相对目录；同 Kit 的 windeployqt 以此为 `--qmldir`，避免扫描源码目录时把未编入产品的 Quick3D 研究场景当成生产依赖。不复制整个 Qt 安装目录，不从研究工具复制 DLL。

[windeployqt](https://doc.qt.io/qt-6/windows-deployment.html)从真实 PE 和 QML 导入扫描运行库、插件及其依赖，包括 C++ `Qt6::CanvasPainter`、Qt Quick、Templates、Dialogs、Multimedia 和 Qt 随附媒体依赖。生产滑尺与二维序章使用 C++ Canvas Painter。SVG image/icon 插件显式纳入。字体、品牌、封面、音频、模型、lookup 与 qsb 已在应用/静态库的 qrc 中；导入与链接闭包不依赖当前工作目录。`qt.conf` 将插件和 QML 解析限定到包内相对位置。未采用的 Quick3D、测试库、qmltooling 和 PDB 不进入成品。

输出 `out/windows/` 含 `yalahoo.exe`、Qt DLL、插件目录、`qml/`、`qt.conf`、许可证、使用说明、验证脚本、`package.json` 的相对路径/体积/SHA256 清单和 `prerequisites/vc_redist.<架构>.exe`。MSVC 运行库用所选 Visual Studio 的官方安装程序提供；脚本不复制开发机的散装 compiler runtime DLL，不自动执行安装或修改系统。目标用户先按系统提示安装该 prerequisites 文件，再直接运行 `yalahoo.exe`。这是本地部署目录，脚本不发布、签名或上传。

生成先写独占 staging；工具失败、运行库/插件缺失或混入研究组件时拒绝替换既有包。完成后才替换 `out/windows/`；替换失败恢复旧包。包和本机清单保留本地，不提交 Git。

Windows PE嵌入branding.md定义的原创多尺寸ICO，窗口图标使用同源各尺寸 PNG（16/24/32 使用小稿），设置的“开发与资源”区使用同源 SVG。技术身份见[product.md](../contracts/product.md#技术身份)，图形来源见[品牌契约](../contracts/branding.md)，随包版权见[NOTICE.md](../../resources/licenses/NOTICE.md)。

最终逐个用 MSVC dumpbin 检查实际 EXE/DLL 的 import 与 delay import，依赖只允许解析到包内、Windows System32/API set 或随包提供的官方 VC Redistributable；Qt/媒体依赖不能由系统目录掩盖。解析失败或未收集的依赖使部署失败，解析边写入本地 `build/windows-release/deployment-dependencies.json`。这不能替代目标系统的动态插件加载和干净机器运行。

## 脱离 Kit 检查

可从源码树或部署目录运行 `verify-windows.ps1`。每次使用全新输出目录；默认放系统临时目录。脚本核对包清单后，仅对子进程清除 Qt/QML/图形覆盖环境，PATH 只含 Windows 与 System32，工作目录在包外；不影响调用终端。默认后端 D3D11，默认`deployment`执行隐藏产品QML/资源/媒体加载探针，`browse`映射到同一隐藏加载协议；交互和GPU事实仍用对应专项入口。`help` 仅证明进程与参数解析成功（在字体、日志和数据会话创建前退出），不能推断 GUI、媒体播放或性能。

```powershell
pwsh -NoProfile -File ./out/windows/verify-windows.ps1
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario browse
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario navigation
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario detail
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario overview
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario demo
pwsh -NoProfile -File ./out/windows/verify-windows.ps1 -Scenario normal `
    -ApplicationArguments @('--music-root', 'D:/匿名音乐 库')
```

`deployment/browse/navigation/detail/overview`检查本次 Process.ExitCode 与严格布尔通过的`product/report.json`；失败、超时或缺报告均失败。`help`只检查命令行退出；`demo/normal`等待用户关闭，不要求产品报告。自动交互只发送自身Qt事件，stdout/stderr与环境说明保留本地。`normal` 自动指定临时数据根，音乐输入须使用匿名副本。演示与普通模式由用户检查后关闭；退出码为零本身不表示视觉/听感验收通过。验证窗口策略遵守[窗口契约](../contracts/test-window-policy.md)，未授权时不提升前台。普通用户主动启动产品保持正常窗口行为。

将完整包复制到无开发Qt的兼容Windows及中文/空格路径，从包外执行上述验证器。普通产品的Explorer/任务栏品牌、中文/空格音乐路径、系统输入法/目录对话框与辅助技术、设备/听感及跨屏/窗口操作共用迁移计划R36的目标产品会话。验收范围和结果解释唯一见[test-gates.md](../contracts/test-gates.md)及对应组件契约；不要重复整套GPU/媒体矩阵，物理缩放记录真实值。

## 工具检查

PowerShell格式/解析入口唯一见[code-style.md](../contracts/code-style.md#格式化与提交门槛)，采用已安装的PSScriptAnalyzer。平台源码审查按[test-gates.md](../contracts/test-gates.md#源码审查边界)；目标成功路径还需核对当前Process.ExitCode、Unicode/含空格参数、子进程环境与退出后的目录栈恢复。

```powershell
python3 tests/tools/WindowsDeploymentTest.py build/windows-release/bin/yalahoo.exe
```

该入口调用`pwsh`，覆盖以下工具协议；缺少环境不能记为通过：

- `product-report.ps1`严格要求对象报告及布尔`passed=true`，拒绝缺失/坏JSON/非对象/字符串或数字成功标记。
- 包清单要求Release、布尔Review OFF、唯一合法相对路径和准确bytes/SHA256；新增（含隐藏）、缺失、空/重复/非法路径及错误体积/hash均失败。
- 复制所选Kit的完整许可目录，覆盖祖先查找、缺失/空目录失败和媒体许可保留。
- 全部公开Scenario映射至实际产品CLI；加载探针缺失/坏报告、非零退出和超时均阻止发布，成功才替换旧包，替换失败恢复旧包。

上述回归与真实Windows的MSVC构建、Tool Help实际加载模块枚举、staging隐藏QML/媒体加载及干净机器运行分别证明，不能互相替代。

## 可选图形后端

开发或普通启动可在当前PowerShell设置`$env:QSG_RHI_BACKEND='d3d12'`或`'vulkan'`后运行`./yalahoo.ps1`；`'opengl'`同样可选，移除该环境变量恢复默认。对应Qt编译特性及驱动必须可用；D3D12需要Windows 10 1703或更新版本和支持的设备，Vulkan需要匹配架构的loader/ICD及开发Kit头。选择、能力与失败语义见[后端契约](../contracts/render-backend.md)。
