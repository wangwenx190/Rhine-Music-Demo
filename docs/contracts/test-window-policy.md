# 测试窗口与桌面不打扰契约

本契约由用户明确要求，适用于本轮、所有后续 session，以及原生单元/集成/GPU/视觉/性能测试、品牌验证、冻结 Web harness 和证据重建工具。普通用户主动启动的播放器窗口保持正常行为。

用户明确要求某轮次进行可见窗口验证，即授权该轮次验证窗口抢前台、前置和获得焦点，无需重复确认。该例外只覆盖指定轮次与验证用途；显式设置 `YALAHOO_VERIFY_FOREGROUND=1`，macOS 可见 CTest 同时以 `YALAHOO_MACOS_VISIBLE_TESTS=ON` 配置。`VerificationWindow` 在该进程创建 QGuiApplication 之前即使用 macOS Regular，随后使用普通窗口 flags 与正常窗口层级，报告 `mode=user-authorized-foreground`、`nonInterferenceEnforced=false`，按实际策略验收，不冒充置底无激活通过。自身图像采集、Release、匿名输入及串行采样要求仍适用；不得截取用户桌面。轮次结束必须清除进程开关，恢复可见测试 OFF；默认隐藏策略复用有效结果，仅策略实现或实际恢复行为变化时运行`verification-window-policy`定向检查，不附带组件矩阵。后续轮次/session 不继承此前授权。

默认macOS可见测试OFF；授权窗口例外与离屏选择由迁移计划的动态能力规则决定。OffscreenSurface在隐藏QQuickWindow上用RenderControl渲染真实RGBA8+depth/stencil，显式target DPR，合并有界render请求，只读自身texture；offscreenRendering只由Review tokens启用。

用户同时要求置底或不抢焦点时，该轮仅授权显示，`YALAHOO_VERIFY_FOREGROUND`保持0；可见入口必须同时满足原生窗口策略，不能将显示授权解释为前台授权。

macOS 必须落实原生窗口策略，不能只检查 Qt flags。Qt Cocoa 的 `windowLevel` 没有处理置底标志，应用启动委托还可能主动激活进程。所有 CTest 在创建 QGuiApplication 前设置 `QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1`；独立图形验证工具及产品 `--verify-*` 入口调用 `prepareApplication()`。QML 验证 root 保持隐藏，取得对象后、首次 show 前调用 `prepareWindow()`。

`VerificationWindowMac.mm` 仅在验证进程使用 AppKit/CoreGraphics 公开 API；默认模式使用 Prohibited 策略禁止应用激活，并关闭 Qt 启动激活，以 `NSNormalWindowLevel-1` 置于普通工作窗口以下，保留拒绝 key 焦点的 Qt 标志，并禁止原生鼠标事件进入测试窗口。测试输入只通过自身 Qt 对象发送。守卫观察 NSView 的 window 和 NSWindow 的 level，Qt 重设层级/替换窗口时同步重新应用；surface 销毁前移除观察，恢复/重建继续生效。报告原生层级、key/main 状态、应用是否激活与前台应用是否变化；同时观察自身AppKit与NSWorkspace激活事件，初始/当前前台PID均须有效且不属于验证进程，自身任何激活/焦点历史仍使默认策略失败。其他应用之间的切换单独记录externalForegroundChanges/foregroundUnchanged，不冒充验证进程抢前台；默认模式的实际 key/main 或应用激活（含短暂通知）使窗口策略失败；授权前台模式核对 Regular/正常层级，并记录实际激活与曝光。该边界不包含音频桥接，不修改普通产品窗口。

不需要真实显示窗口的检查保持隐藏：模型、格式、字体、资源、合成图像算法单元检查、shader bake、参数与静态审计不调用 show。纯 QML 组件检查不为方便而加载可见 ApplicationWindow。浏览器采集使用已有 headless 模式，后台进程通过 windowsHide/CreateNoWindow 或 Start-Process 的 `-WindowStyle Hidden` 启动。

需要真实 exposed 窗口、真实 RHI 呈现或 hide/minimize/restore 行为的测试使用 `src/app/VerificationWindow.h`。默认在创建/第一次 show 之前设置 `Qt::WindowStaysOnBottomHint` 和 `Qt::WindowDoesNotAcceptFocus`；需要无边框时额外加 FramelessWindowHint。默认禁止 WindowStaysOnTopHint、raise、requestActivate、activateWindow 与系统前台激活；本契约的显式授权轮次可按需前置、激活。任何模式均不自动点击用户桌面。窗口重建、改变尺寸和 showNormal 后必须继续保持选定策略；默认模式保留置底/不接受焦点标志。不能依靠隐藏启动进程代替 Qt 窗口标志：程序后续 show 仍可能显示窗口。

生产`--verify-app`/部署/详情/总览/导航入口在QML构建前注入verificationRun/verificationFlags，ApplicationWindow先保持隐藏；必要的GPU入口才show。独立MaterialReview、RhiOpaqueChecks、RhiShellChecks和品牌窗口同样复用统一helper，在首次显示前落实策略。报告写入 windowPolicy；默认模式核对无激活，授权前台模式明确记录该例外，违反所选策略使测试失败。测试输入使用直接发送到自身对象的 Qt 事件，不全局模拟用户键鼠；前置/激活只在授权前台模式进行。

QQuickWindow 使用自身 grabWindow / GPU texture readback 获取完整渲染结果；未获轮次授权时，置底或被其他应用遮挡不成为提高 Z 顺序的理由。品牌 QWindow 使用自己真实 expose/paint 产生的 backing 图像，显式记录 captureMethod；不调用 QScreen::grabWindow 采集桌面区域，避免把用户工作内容混入证据。导出的自己渲染图不能宣称是桌面截屏。手工窗口外观检查由用户主动执行，后台测试前置仍须本契约的轮次授权。

Release CTest 的 `verification-window-policy` 创建隐藏的真实原生 QWindow，验证初次创建、flags 变化和 surface 重建保持隐藏；Windows 同时核对系统 WS_EX_NOACTIVATE、非 topmost 和前台 HWND 不等于测试窗口。macOS 隐藏检查核对原生低层级、拒绝 key、关闭启动激活、忽略原生鼠标事件及验证进程从未占据前台/获得激活或焦点；不为检查标志而显示窗口。实际运行/运动/资料及两个 RHI Review 的 windowPolicy 检查覆盖必须显示的窗口和恢复行为；图形对照继续使用原 DPR/fixture/目标尺寸，不因置底而降低画质或改用假渲染。

授权可见复现使用进程环境开关，不写进公共 preset。`ui-tests` 另设 `YALAHOO_VERIFY_VISIBLE=1` 选择 native window；`node tools/migration/verify-title.mjs --visible --backend <目标后端> --dpr 1|1.5|2 --source 同机参照 --output 新目录` 保留有效结构/运动/缓存/释放检查；其来源/运动/资源断言归title-reels.md。macOS OpenGL 按当前 AppKit 要求使用 basic，Metal 使用 threaded；原生 frameSwapped 的驱动回调排队到 GUI，basic 的 surface 重建与最终 renderer 清理帧单列，不能称为 render-thread 隔离。当前 CTest 中 `verification-window-visible` 核对首次曝光、实际帧、原生 onScreen、最小化及无边框/带边框两种 flags/surface 重建；`verification-window-policy` 强制 `YALAHOO_VERIFY_FOREGROUND=0`，独立证明默认隐藏策略。

## 授权可见入口

```sh
cmake --preset qt-release -DYALAHOO_MACOS_VISIBLE_TESTS=ON
YALAHOO_VERIFY_FOREGROUND=1 ctest --preset qt-release-visible -R '^runtime-overview$'
cmake --preset qt-release -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
```

结束时恢复OFF；策略或实际恢复行为改变才定向运行verification-window-policy，否则复用有效结果。可见开关只选择入口，不自行授予前台权限；授权来自用户指定轮次。UI独立native入口另需YALAHOO_VERIFY_VISIBLE=1。工具仅发送自身Qt事件，不全局模拟用户键鼠。

实际Review/原生可见分类、Review OFF的注册范围和测试选择统一见[test-gates.md](test-gates.md#当前review入口分类)，本文件不另维护入口清单。
