# yalahOO!!! · Qt 原生项目开发约定

- [QT_QUICK_MIGRATION_PLAN.md](QT_QUICK_MIGRATION_PLAN.md) 是项目内最高优先级的实施契约；新的用户明确要求优先于已有项目文档。其他约定仅补充未冲突内容。
- 正式展示名称为 `yalahOO!!!`；技术标识使用 `yalahoo`，CMake 目标 `Yalahoo`/`YalahooCore`、QML 模块 `Yalahoo.Music`。完整身份与数据路径规则按计划执行，历史归档与作者署名保留原名。
- 原项目所有文件位于 `legacy-web/`，用于冻结 Web 基线、资产来源与历史追溯。旧模块名从该目录解析；旧 npm/构建命令在该目录运行，使用隔离的数据目录。根 `.git/` 保留当前仓库历史。
- 新项目采用 C++23、开发机已安装的最新 Qt 版本、Qt Quick/QML、Qt Quick 3D、QtMultimedia 与根 CMake。C++ 在 `src/`，QML 在 `qml/Yalahoo/Music/`，资产在 `resources/`，测试在 `tests/`，工具在 `tools/`；详细结构与模块契约以计划为准。
- 所有后续轮次强制遵守 [Qt 源码风格契约](docs/contracts/code-style.md) 及根 `.clang-format`、`.qmlformat.ini`、`.editorconfig`：四空格缩进，不使用 Tab，不把多个语句、短函数体、控制语句体或多个 QML 属性挤在一行。提交前运行 `cmake --build --preset qt-release --target check-format`；Release CTest 的 `source-format` 必须通过。维护工具同样使用四空格和展开的语句，冻结的 `legacy-web/`、历史证据及第三方资产保持原貌。
- CMake、公共 presets 与脚本不得锁定 Qt minor/patch 版本或安装目录；执行时按计划自动发现本机最新原生 Kit，模块、工具与插件使用同一 Kit。计划中的版本与路径只供定位；GuiPrivate 变化通过探测和适配处理，不退回旧 Qt。C++23 必须显式启用且不允许静默降级。
- GUI、动画和特效尽最大可能逼近冻结基线；保留双主题、减少动态、速度、响应式、真实列/填充、列记忆及搜索不改变播放等既有行为。
- 所有外观、动画、特效以冻结旧项目源码及实际生产调用路径为准；截图、运行参数和各类采集数据只作参考，不把渲染/采集缺陷当成复刻目标。按用户要求改善可见锯齿，源码优先级和差异验收按迁移计划执行。
- R03B 设计精致、精美且契合音乐收藏主题的原创应用图标与 Logo，主素材必须为真正 SVG，按需导出 PNG/ICO/ICNS。新品牌替换旧标识，页眉/开场布局和动效时序仍遵循复刻契约；具体方案、格式与验收以计划为准。
- 音乐按普通播放器实现，只用 QtMultimedia 公开 API；不实现无缝播放、交叉混音、手动 PCM、自建音频引擎、Swift/CoreAudio 桥接或外部 FFmpeg 命令。歌曲、BGM 与界面音效独立。
- 联网暂缓但保留异步 Provider、来源/缓存字段、状态占位与配置扩展；本阶段不发送请求。原生运行不依赖 Node、浏览器、WebEngine 或本地 HTTP 服务，不新增第三方库，系统自带能力按计划允许使用。
- Windows、macOS、Linux 均须完整实现。Windows 必须严格实机验证；macOS/Linux 通过计划规定的严格静态审计即可完成本阶段验收，结论标明“静态审计通过，实机待用户验证”。真实环境验证留待用户以后执行，不可记为已经运行通过。
- 常规构建、测试、GUI/性能验证与验收只使用 Release（`qt-release`）。只有明确需要获取堆栈时才编译和运行 Debug（`qt-debug`）；不得为了例行双配置验证运行 Debug。该规则适用于后续会话及证据重建脚本。
- 所有测试、视觉/性能采集与证据重建默认不干扰用户桌面：不需要真正显示窗口的保持隐藏；必须显示的 Qt 窗口在第一次 show 之前设置 `Qt::WindowStaysOnBottomHint` 与 `Qt::WindowDoesNotAcceptFocus`，隐藏/恢复/重建后继续保持，禁止 raise/requestActivate/抢焦点。统一使用 `src/app/VerificationWindow.h`，浏览器采集保持 headless、后台进程隐藏；不得用桌面截屏采集被遮挡窗口或用户工作内容。详细规则及验证入口见 [测试窗口契约](docs/contracts/test-window-policy.md)，所有后续 session 强制遵守。
- 依次执行依赖已满足的剩余轮次，一次以一个轮次为边界。维护计划严格遵守：【已经完成的轮次直接移除，不允许输出过程性文字到这个文件中，除非是需要跨轮次传播的信息或者执行的任务】。
- 普通 Qt 音乐启动器在根目录，演示/光照研究启动器在 `tools/launchers/`。不提前生成尚未执行轮次的假实现。
- 保留归档文件、作者署名与资源来源。本阶段不自动发布、上传私人数据、改写发行标签或改动用户音乐文件。
