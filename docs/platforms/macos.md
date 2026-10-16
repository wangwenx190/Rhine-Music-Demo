# macOS开发与部署操作

本文只管macOS工具、应用入口与系统差异。Kit/C++23规则见[native-bootstrap.md](../contracts/native-bootstrap.md)，测试范围与复用见[test-gates.md](../contracts/test-gates.md)，未完成实机门槛集中于迁移计划R31-P、R34与R35。

## 开发环境与构建

- Apple Silicon使用原生arm64终端；Universal Kit须包含本机架构。安装支持项目C++23探针的Xcode/Command Line Tools、CMake/Ninja、现代clang-format及本机最新完整Qt Desktop Kit。构建每次重新发现最新原生Kit，不固定版本，不因缺模块回退。
- 自定义Qt根使用本机`YALAHOO_QT_ROOTS`；formatter可在本机cache设置`YALAHOO_CLANG_FORMAT`。不要复制其他机器的build/cache/UserPresets/Kit/私人数据；冻结来源解析需要完整Git历史。
- `sh tools/build/macos.sh -DYALAHOO_REVIEW=OFF`配置并构建普通Release，产物为`build/release/bin/yalahoo.app`。脚本保留后续CMake参数；测试按受影响项显式选择，格式入口见code-style契约。
- 需要隐藏组件验证时用`sh tools/build/macos.sh -DYALAHOO_REVIEW=ON -DYALAHOO_MACOS_VISIBLE_TESTS=OFF`，再按组件契约选择CTest。`macos-application-menu`检查真实AppKit菜单动作、Qt编辑对象与释放；`verification-window-policy`检查默认隐藏策略。

## 独立生产部署

```sh
sh tools/deploy/macos.sh
python3 tools/deploy/verify-macos.py --output "$PWD/verification/macos-clean"
./yalahoo.command --demo
```

部署入口使用独立`build/macos-release/`，强制Release、Review OFF；不改开发构建的Review开关。它校验选中Kit的qtpaths，调用同Kit macdeployqt，只扫描编入应用的生产QML。`out/macos/yalahoo.app`包含Frameworks、PlugIns、Resources/qml、原始ICNS及qrc资源，署名/资源/字体/音频/安装Qt许可在Resources/licenses中。Qt随附媒体依赖由macdeployqt处理，应用不调用外部FFmpeg。递归QtQuick部署带入的SQL驱动仅保留自含SQLite；产品使用JSON存储，没有外部数据库服务。

打包逐个核对实际Mach-O架构、加载依赖与rpath、QML/插件/CanvasPainter闭包、包内符号链接和本地签名；Info.plist与package.json的最低macOS版本取实际全部二进制切片要求的最大值，不从Qt版本猜测兼容系统。去除重定位后越出bundle的Kit搜索路径，实际依赖仍须全部解析。构建、部署、签名、闭包或隐藏产品探针失败均保留既有包。该包仅作本地ad-hoc签名，不自动上传、公证或发布。

验证器校验package.json中的文件hash/可执行位/符号链接，再以系统PATH、清除Qt/QML/DYLD覆盖的子环境，从包外新目录运行`--verify-deployment`。产品保持隐藏，实例化真实App QML、核对嵌入字体/图标/shader/音频加载及QML warnings，并经真实Qt后端无声加载BGM；实际dyld映像只能来自bundle或系统。输出写入指定全新目录，省略时使用系统临时目录；不能放在部署包内。此入口不发play、不证明GPU画面或实际听感。Python仅用于开发验证，普通应用运行不需要它。

复制整个`out/macos/`到含空格/中文的其他目录后，可在该目录运行`python3 verify-macos.py --output /绝对路径/全新目录`复核重定位；应用本身可直接双击bundle。不要移动或改写bundle内部文件。

## 普通启动、菜单与后台

根`yalahoo.command`及`yalahoo.sh`优先运行完整部署包；无包时使用Release开发bundle和cache中的Kit。`tools/launchers/demo.command`与`lighting-lab.command`同样复用原生入口。路径按脚本自身位置解析，参数和进程退出码保留；启动器不自动构建。

- 应用菜单：⌘,打开现有设置，片头输入锁期间禁用；⌘Q正常退出。
- 文件/窗口：⌘W关闭最后窗口并退出；⌘M最小化，窗口菜单提供缩放。隐藏⌘H、隐藏其他⌥⌘H与全部显示由系统处理。
- 编辑菜单：⌘Z、⇧⌘Z、⌘X/C/V/A调用当前Qt编辑器的撤销、重做、剪切、复制、粘贴、全选；能力随焦点、只读、选区与剪贴板状态变化。菜单不持有可重建的QML编辑对象。
- 隐藏/最小化停止视觉帧与新封面工作，歌曲/BGM由独立Qt媒体会话继续；关闭/退出依次停止歌曲、BGM、音效。普通产品保持正常窗口行为；默认原生数据根由QStandardPaths/AppLocalDataLocation和稳定技术身份决定。

## 用户实机清单

使用匿名或用户明确选定的本地歌曲；隔离数据根示例：

```sh
./yalahoo.command --data-dir "$PWD/verification/macos-manual/data" \
    --music-root "/绝对路径/匿名音乐"
```

| 操作 | 预期与入口 |
| --- | --- |
| 配置/构建/静态 | 上述构建与部署成功；`python3 tools/audit/audit-macos-deployment.py`展开COMMON/MAC/PERF与编译QML闭包。格式/官方qmllint按code-style执行 |
| 脱离Kit | 从包外目录运行验证器，当前进程退出0且新report.json passed；Qt/QML/媒体映像不来自安装Kit；Finder图标/名称正确 |
| 浏览/详情/总览 | 双主题、真实列/填充、搜索与播放定位互不干扰；检查源景深/磨砂、玻璃排序、透明轮廓、裁切与清晰度。隐藏组件入口用`qt-release-render`选择ui/ruler/opening/settings及相应RHI测试 |
| 片头/Canvas/文字 | Logo圆环、滑尺数字与标题/emoji正常合成；重播/跳过交接有效；DPR/resize及十次重建后无漏画或增长。数学/状态和自身正常输出分别验收 |
| 原生菜单/输入 | 物理键盘逐项使用上述Command快捷键；编辑焦点/输入法/只读/片头锁正确，设置视觉重载后仍可用；⌘W/⌘Q退出不残留音频进程 |
| Retina/屏幕 | 在实际DPR1/1.5/2设备与跨屏切换中记录真实值，逻辑尺寸和清晰度正确；显式离屏target DPR不替代物理屏幕 |
| 播放与设备 | 本地播放、暂停/继续、seek、上下曲、停止、错误、默认/显式设备切换与断开恢复；检查三路独立音量、实际听感、后台继续与退出停止 |
| 生命周期/性能 | hide/minimize/restore、同窗口surface和完整视觉重建分别核对；正常VSYNC与独立吞吐、输入/准备时延、资源峰值按performance.md和R28-P取有效样本；不能从隐藏提交成本推断显示延迟 |

## 图形与验证差异

默认Metal/threaded。显式OpenGL在QGuiApplication前选择4.1/CoreProfile，native drawable用basic主线程；缺多采样纹理入口时保持同格式/样本renderbuffer+resolve，动态执行集中于R35。两种线程模式的成本分开。

普通验证遵守[test-window-policy.md](../contracts/test-window-policy.md)，macOS可见测试默认OFF，RenderControl真正隐藏。原生工具需有WindowServer/系统服务访问；受限进程中的无屏幕或不可用系统服务不算运行通过。前台授权不跨轮次；不能为遮挡/节流提升窗口。

实际DPR从窗口/组件报告核对；CoreText与彩色字体适配见assets/title-reels。CanvasPainter在产品使用C++ API，QtCanvas2D仅在比较工具中；生产打包不因比较工具引入该模块。各组件现有Metal/OpenGL隐藏及授权可见运行事实见source-fidelity-review.md；欠验只由迁移计划管理。
