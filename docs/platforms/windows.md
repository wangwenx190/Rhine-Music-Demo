# Windows 构建、部署与验证

本文负责 Windows 工具、普通启动及部署操作；Kit/C++23 规则见[原生工程](../contracts/native-bootstrap.md)，格式和测试选择见各自契约。欠验目标及退出门槛只在迁移计划 R30-P 维护。

## 工具与构建

使用原生 Windows、PowerShell 7.2 或更新版本、Visual Studio C++ 工具（含 Windows SDK、对应架构官方 VC Redistributable）、CMake/Ninja、clang-format 与本机最新完整 Qt Desktop MSVC Kit。`enter-msvc.ps1` 通过 vswhere 初始化当前进程，Qt 由 CMake 发现，不在公共脚本固定安装路径或版本。自定义安装根使用 `YALAHOO_QT_ROOTS`，不从其他平台复制 cache。

```powershell
pwsh -NoProfile -File ./tools/build/build.ps1 -Preset qt-release -Action Configure
pwsh -NoProfile -File ./tools/build/build.ps1 -Preset qt-release -Action Build
ctest --preset qt-release -R 'identity|kit|source-format' --output-on-failure
pwsh -NoProfile -File ./yalahoo.ps1 --demo
```

开发启动器优先使用完整的 `out/windows/` 包，否则解析 Release cache 的 Kit；不自动构建。全部启动器从自身路径定位，逐个保留 Unicode/空格参数，等待真实 GUI Process.ExitCode。普通入口为本地音乐库，演示入口不读取私人数据；数据/扫描/导入参数见[数据契约](../contracts/library-data.md)。

## 生成生产包

```powershell
pwsh -NoProfile -File ./tools/deploy/windows.ps1
```

脚本重新发现最新 Kit，以 `build/windows-release/` 的独立生产 Release 配置构建 `Yalahoo`，强制 Review OFF。应用 CMake 同时生成 `deployment.json`，其 QML 清单来自应用实际编译列表。部署扫描副本 `build/windows-release/deploy-scan/qml/` 只包含根 `qml/` 中的生产闭包，保留相对目录；同 Kit 的 windeployqt 以此为 `--qmldir`，避免扫描源码目录时把未编入产品的 Quick3D 研究场景当成生产依赖。不复制整个 Qt 安装目录，不从研究工具复制 DLL。

[windeployqt](https://doc.qt.io/qt-6/windows-deployment.html)从真实 PE 和 QML 导入扫描运行库、插件及其依赖，包括 C++ `Qt6::CanvasPainter`、Qt Quick、Templates、Dialogs、Multimedia 和 Qt 随附媒体依赖。生产滑尺与二维序章使用 C++ Canvas Painter，没有 QtCanvas2D 导入。SVG image/icon 插件显式纳入。字体、品牌、封面、音频、模型、lookup 与 qsb 已在应用/静态库的 qrc 中；导入与链接闭包不依赖当前工作目录。`qt.conf` 将插件和 QML 解析限定到包内相对位置。未采用的 Quick3D/QtCanvas2D、测试库、qmltooling 和 PDB 不进入成品。

输出 `out/windows/` 含 `yalahoo.exe`、Qt DLL、插件目录、`qml/`、`qt.conf`、许可证、使用说明、验证脚本、`package.json` 的相对路径/体积/SHA256 清单和 `prerequisites/vc_redist.<架构>.exe`。MSVC 运行库用所选 Visual Studio 的官方安装程序提供；脚本不复制开发机的散装 compiler runtime DLL，不自动执行安装或修改系统。目标用户先按系统提示安装该 prerequisites 文件，再直接运行 `yalahoo.exe`。这是本地部署目录，脚本不发布、签名或上传。

生成先写独占 staging；工具失败、运行库/插件缺失或混入研究组件时拒绝替换既有包。完成后才替换 `out/windows/`；替换失败恢复旧包。包和本机清单保留本地，不提交 Git。

Windows PE 嵌入 R03B 原创多尺寸 ICO，窗口图标使用同源各尺寸 PNG（16/24/32 使用小稿），设置的“开发与资源”区使用同源 SVG。标识为 `yalahoo` / `yalahoo.music`，显示名为 `yalahOO!!!`；许可与来源由[品牌契约](../contracts/branding.md)维护。

最终逐个用 MSVC dumpbin 检查实际 EXE/DLL 的 import 与 delay import，依赖只允许解析到包内、Windows System32/API set 或随包提供的官方 VC Redistributable；Qt/媒体依赖不能由系统目录掩盖。解析失败或未收集的依赖使部署失败，解析边写入本地 `build/windows-release/deployment-dependencies.json`。这不能替代目标系统的动态插件加载和干净机器运行。

## 脱离 Kit 检查

可从源码树或部署目录运行 `verify-windows.ps1`。每次使用全新输出目录；默认放系统临时目录。脚本核对包清单后，仅对子进程清除 Qt/QML/图形覆盖环境，PATH 只含 Windows 与 System32，工作目录在包外；不影响调用终端。默认后端 D3D11。`help` 仅证明进程、嵌入字体初始化及参数解析成功，不能推断 GUI、媒体播放或性能。

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

自动验证只发送自身 Qt 事件，检查本次 Process.ExitCode 和本次 `product/report.json`；失败、超时或缺报告均失败，stdout/stderr 与环境说明保留本地。`normal` 自动指定临时数据根，音乐输入须使用匿名副本。演示与普通模式由用户检查后关闭；退出码为零本身不表示视觉/听感验收通过。验证窗口策略遵守[窗口契约](../contracts/test-window-policy.md)，未授权时不提升前台。普通用户主动启动产品保持正常窗口行为。

在无 Qt、Node、外部 FFmpeg/开发工具的 Windows 干净机器上重复以上入口，排除开发机已安装运行库及硬编码 Kit 搜索路径掩盖缺失依赖。检查无缺模块/插件/shader/字体错误，MiSans 中文与长标题正确，滑尺和序章轮廓、透明合成/裁切及恢复正常；在 Explorer、窗口/任务栏检查大小图标。正常产品检查空库、演示、Unicode 音乐目录、扫描/错误提示、设备列表、支持的匿名格式、播放/暂停/seek/切歌、设备切换及后台继续播放。格式能力由实际 QtMultimedia 后端决定，缺样本/无设备不得记为通过。

代表尺寸/DPR、浏览/详情/总览/序章、hide/restore/resize、冷启动和资源峰值沿用迁移计划各组件门槛。正式正常 VSYNC 与未限速场景分别按[性能契约](../contracts/performance.md)和 `verify-startup.py` 的目标入口采集，Review 测量工具单独构建保留，不能混入成品。记录真实 OS/GPU/Kit/D3D11、逻辑/物理尺寸/DPR/质量/刷新率/窗口策略，未知 GPU 完成、屏幕显示及 driver 显存保持 null；不能用 macOS 或离屏成绩填写 Windows 产品结果。

## 源码审计

```powershell
python tools/audit/audit-windows-deployment.py
pwsh -NoProfile -File ./tools/format/FormatTools.ps1 -Check
```

审计输出在 `verification/{windows,macos,linux}-static/windows-deployment/`，覆盖 COMMON/WIN/PERF 的实际调用链、平台分支和源 hash。它证明源码接线与可追溯性，不证明目标 PE、windeployqt 或 GPU 已执行；实际 Windows 构建与干净包/设备/呈现欠验按 R30-P 执行。
