# 测试窗口与桌面不打扰契约

本契约由用户明确要求，适用于本轮、所有后续 session，以及原生单元/集成/GPU/视觉/性能测试、品牌验证、冻结 Web harness 和证据重建工具。普通用户主动启动的播放器窗口保持正常行为。

验证方式按迁移计划逐项核对实际可靠能力，不按平台名称固定为实机或静态。当前macOS尚未建立可靠的非激活可见验证入口，默认`YALAHOO_MACOS_VISIBLE_TESTS=OFF`，对应runtime/GPU检查暂缓到最后R34，不把disabled写成通过；已有可靠隐藏/离屏入口必须真实运行。当前二维Review优先使用`OffscreenSurface`：QQuickRenderControl在隐藏QQuickWindow上渲染真实Metal单采样RGBA8+depth/stencil目标，显式设置目标DPR，有界合并render请求；只读回自己的texture，不调用macOS原生show、不依赖exposure或swapchain呈现。offscreenRendering只由Review tokens启用，生产默认false。native窗口恢复与显示效果不由离屏生命周期推断，离屏局部成本不代表呈现性能；当前无法可靠验证的窗口/VSYNC等项目严格审计并保留R34实机任务。

macOS 必须落实原生窗口策略，不能只检查 Qt flags。Qt Cocoa 的 `windowLevel` 没有处理置底标志，应用启动委托还可能主动激活进程（[Qt Cocoa 窗口源码](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/cocoa/qcocoawindow.mm)、[启动委托源码](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/cocoa/qcocoaapplicationdelegate.mm)）。所有 CTest 在创建 QGuiApplication 前设置 `QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1`；独立图形验证工具及产品 `--verify-*` 入口调用 `prepareApplication()`。QML 验证 root 保持隐藏，取得对象后、首次 show 前调用 `prepareWindow()`。

`VerificationWindowMac.mm` 仅在验证进程使用 AppKit 公开 API：使用 Prohibited 策略禁止应用激活，并关闭 Qt 启动激活，以 `NSNormalWindowLevel-1` 置于普通工作窗口以下，保留拒绝 key 焦点的 Qt 标志，并禁止原生鼠标事件进入测试窗口。测试输入只通过自身 Qt 对象发送。守卫观察 NSView 的 window 和 NSWindow 的 level，Qt 重设层级/替换窗口时同步重新应用；surface 销毁前移除观察，恢复/重建继续生效。报告原生层级、key/main 状态、应用是否激活与前台应用是否变化；实际 key/main 或应用激活使窗口策略失败。该边界不包含音频桥接，不修改普通产品窗口。

不需要真实显示窗口的检查保持隐藏：模型、格式、字体、资源、图像比较、shader bake、参数与静态审计不调用 show。纯 QML 组件检查不为方便而加载可见 ApplicationWindow。浏览器采集使用已有 headless 模式，后台进程通过 windowsHide/CreateNoWindow 或 Start-Process 的 `-WindowStyle Hidden` 启动。

需要真实 exposed 窗口、真实 RHI 呈现或 hide/minimize/restore 行为的测试使用 `src/app/VerificationWindow.h`。在创建/第一次 show 之前设置 `Qt::WindowStaysOnBottomHint` 和 `Qt::WindowDoesNotAcceptFocus`；需要无边框时额外加 FramelessWindowHint。禁止 WindowStaysOnTopHint、raise、requestActivate、activateWindow、系统前台激活和自动点击用户桌面。窗口重建、改变尺寸和 showNormal 后也必须保留置底/不接受焦点标志。不能依靠隐藏启动进程代替 Qt 窗口标志：程序后续 show 仍可能显示窗口。

`yalahoo --verify-runtime`（含 motion/library/shading）、`--verify-scene`、`--verify-materials` 在 QML 构建前注入 verificationRun/verificationFlags，ApplicationWindow 先保持隐藏；参数验证后，必要的 GPU 入口才 show。QML root 在首次显示前已有置底/不接受焦点标志；SceneReview、MaterialReview、RhiAudit、RhiShellAudit 和品牌窗口均复用同一 helper，不在某个测试中恢复为普通 Window flags。报告写入 windowPolicy，检查实际窗口不活跃，违反策略使测试失败。测试输入使用直接发送到自身对象的 Qt 事件，不激活窗口或全局模拟用户键鼠。

QQuickWindow 使用自身 grabWindow / GPU texture readback 获取完整渲染结果，置底或被其他应用遮挡不成为提高 Z 顺序的理由。品牌 QWindow 使用自己真实 expose/paint 产生的 backing 图像，显式记录 captureMethod；不调用 QScreen::grabWindow 采集桌面区域，避免把用户工作内容混入证据。导出的自己渲染图不能宣称是桌面截屏。手工窗口外观检查由用户主动执行，不成为后台测试抢焦点的理由。

Release CTest 的 `verification-window-policy` 创建隐藏的真实原生 QWindow，验证初次创建、flags 变化和 surface 重建保持隐藏；Windows 同时核对系统 WS_EX_NOACTIVATE、非 topmost 和前台 HWND 不等于测试窗口。macOS 隐藏检查核对原生低层级、拒绝 key、关闭启动激活、忽略原生鼠标事件及前台应用不变；不为检查标志而显示窗口。实际运行/运动/资料及两个 RHI Review 的 windowPolicy 检查覆盖必须显示的窗口和恢复行为；图形对照继续使用原 DPR/fixture/目标尺寸，不因置底而降低画质或改用假渲染。

常规验证：

```sh
cmake --build --preset qt-release --target format
cmake --build --preset qt-release --target check-format
cmake --build --preset qt-release
ctest --preset qt-release --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

Windows既有完整Review/生产重建入口为`tools/build/verify-r10b.ps1`，只有具备对应可靠环境时执行。所有平台都检查同一helper/QML初始属性、原生分支及采集所有权；可靠本机检查必须实际通过，不可执行的目标窗口/呈现/重建验证严格审计并保留到R34。纯静态与实际运行结论分别记录，其他平台结果不证明目标窗口管理器或compositor已验证。
