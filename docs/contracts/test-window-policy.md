# 测试窗口与桌面不打扰契约

本契约由用户明确要求，适用于本轮、所有后续 session，以及原生单元/集成/GPU/视觉/性能测试、品牌验证、冻结 Web harness 和证据重建工具。普通用户主动启动的播放器窗口保持正常行为。

不需要真实显示窗口的检查保持隐藏：模型、格式、字体、资源、图像比较、shader bake、参数与静态审计不调用 show。纯 QML 组件检查不为方便而加载可见 ApplicationWindow。浏览器采集使用已有 headless 模式，后台进程通过 windowsHide/CreateNoWindow 或 Start-Process 的 `-WindowStyle Hidden` 启动。

需要真实 exposed 窗口、真实 RHI 呈现或 hide/minimize/restore 行为的测试使用 `src/app/VerificationWindow.h`。在创建/第一次 show 之前设置 `Qt::WindowStaysOnBottomHint` 和 `Qt::WindowDoesNotAcceptFocus`；需要无边框时额外加 FramelessWindowHint。禁止 WindowStaysOnTopHint、raise、requestActivate、activateWindow、系统前台激活和自动点击用户桌面。窗口重建、改变尺寸和 showNormal 后也必须保留置底/不接受焦点标志。不能依靠隐藏启动进程代替 Qt 窗口标志：程序后续 show 仍可能显示窗口。

`yalahoo --verify-runtime`（含 motion/library/shading）、`--verify-scene`、`--verify-materials` 在 QML 构建前注入 verificationRun/verificationFlags，ApplicationWindow 先保持隐藏；参数验证后，必要的 GPU 入口才 show。QML root 在首次显示前已有置底/不接受焦点标志；SceneReview、MaterialReview、RhiAudit、RhiShellAudit 和品牌窗口均复用同一 helper，不在某个测试中恢复为普通 Window flags。报告写入 windowPolicy，检查实际窗口不活跃，违反策略使测试失败。测试输入使用直接发送到自身对象的 Qt 事件，不激活窗口或全局模拟用户键鼠。

QQuickWindow 使用自身 grabWindow / GPU texture readback 获取完整渲染结果，置底或被其他应用遮挡不成为提高 Z 顺序的理由。品牌 QWindow 使用自己真实 expose/paint 产生的 backing 图像，显式记录 captureMethod；不调用 QScreen::grabWindow 采集桌面区域，避免把用户工作内容混入证据。导出的自己渲染图不能宣称是桌面截屏。手工窗口外观检查由用户主动执行，不成为后台测试抢焦点的理由。

Release CTest 的 `verification-window-policy` 创建隐藏的真实原生 QWindow，验证初次创建和 flags 重建保持隐藏；Windows 同时核对系统 WS_EX_NOACTIVATE、非 topmost 和前台 HWND 不等于测试窗口。它不为测试标志而显示窗口。实际运行/运动/资料及两个 RHI Review 的 windowPolicy 检查覆盖必须显示的窗口和恢复行为；图形对照继续使用原 DPR/fixture/目标尺寸，不因置底而降低画质或改用假渲染。

常规验证：

```sh
cmake --build --preset qt-release --target format
cmake --build --preset qt-release --target check-format
cmake --build --preset qt-release
ctest --preset qt-release --output-on-failure
cmake --build --preset qt-release --target Yalahoo_qmllint
```

Windows 完整 Review 与生产回归由 `tools/build/verify-r10b.ps1` 保存本轮证据，CPU 窗口标志检查和所有实际 GPU 报告均应 passed。macOS/Linux 检查同一 helper/QML 初始属性、平台分支与采集所有权；本阶段结论仍为“静态审计通过，实机待用户验证”，不能从 Windows 结果推断目标窗口管理器或 compositor 已实机验证。
