# Presentation 与详情界面契约

冻结定义为 `legacy-web/src/music-presentation.ts`、音乐入口 `music-app.ts` 的实际 ports、`ui-transitions.ts`、`music-title.ts` 及 `music.css`，来源标识沿用 `web-baseline.md`。三维抽取、换片与归位通过 DetailPorts 接入 DetailMotion 和 ReturningCasePool；同步轨迹、场景 readiness、拖动门禁与产品回放范围见 [detail-motion.md](detail-motion.md)。

`PresentationController` 是 GUI 线程的唯一展示状态所有者，保留 archive/opening/detail/switch-hiding/switching/hiding/returning/selecting。退 browse 后才请求相机；详情的场景 readiness 与当前 layoutRevision 都满足才显示文字。退字中的新输入替换一个 pendingSelection；搜索/歌曲定位的 archiveRoute 等归位和选中行完全就绪再重开。back 清掉 pending；reset 使旧回调和布局结果失效。嵌套完成路径批量发一次状态通知，重复 update 不发通知。各相机/选中 ports 不修改播放状态，不发网络请求。

两个 SurfaceTransition 使用同一 FrameClock 的 motionDt，保留 browse 720/140ms 与 detail 360/240ms，源码 Bezier、隐藏初始位移、退场位移和打断后的 `max(72ms, duration * sqrt(opacityDistance))`。改速不重置 progress，减少动态完成相同回调路径。一个 surface 只有一个可替换完成回调；revision 也覆盖 changed 信号重入。browse 的 veil/信息/导航/footer 分别改透明度，Header 保持自身绘制层级。详情退出期仍可见但禁用；InputRouter 与 SceneItem 的鼠标/触摸路径共同阻止底层选择。

DetailModel 持有不可变库快照及索引取得的当前专辑指针。prepare 一次计算八项 facts，保留标题/英文译名、编号、曲目/分碟/时长、介绍、本地备注、制作人员、来源 URL/许可证和目录；不会把全歌单序列化为 QVariant 数组。相同快照、专辑和编号不重置模型。TrackList 使用可见/复用委托和 100px 预留，曲目与制作人员各有模型类型稳定的 ListView；切 tab 重建相应视图，不让旧角色缓存跨模型类型复用。布局回调只排一个，新准备替换其 revision；完成后以 positionViewAtBeginning 显示 header。退出焦点请求在绑定更新后交付，恢复仍有效且可交互的原 Item，旧展示请求不能重新取得焦点。

## Release 复现与实际覆盖

只使用 CMake 自动生成的官方 lint targets；不自行组装 qmllint 命令或直接调用程序。

```sh
cmake --preset qt-release
cmake --build --preset qt-release
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
ctest --preset qt-release --output-on-failure
python3 tools/audit/audit-presentation.py
```

`presentation` 覆盖场景/布局双门禁、browse 退场中连选、连续详情目标、四种状态中的 archiveRoute、取消重开、快速 open/back、reset 过期回调、改速/减少动态、10,000 曲目的快照与字段。`presentation-qml` 使用生产 Detail/TrackList：九尺寸×双主题、实际标题/header 起点、跳转第 8,000 曲目的有限委托、介绍切换、退出禁用/快速重开、十次离屏释放恢复、最终释放与窗口策略。运行时 QQmlEngine warnings 和绑定/类型/shader 日志另行检查，不替代官方 lint target。

R17 阶段的 25 项启用 Release CTest 全部通过；当时四项可见 runtime 禁用，后续可见覆盖见下节及 detail-motion.md；官方两组 lint targets 与 check-format 均无新增警告。macOS/arm64、自动发现的 Qt 6.12.0、AppleClang 21、Release/真实隐藏 Metal、实际 DPR1 通过上述 18 项结构与资源检查，10,000 曲目的可见/复用池 31 个，QML warnings 为零。报告与自身 texture 的 PNG 位于 `build/release/verification/presentation/`，不提交。这里没有做冻结 Web 成对 SSIM，也没有测 present/VSYNC、输入到光子或 GPU 时间；完整 GUI 成对补修仍按 R29，实际可见和目标平台门槛保留 R34。

## 平台实机清单与交接

三平台 COMMON/PERF/平台源码审计由 `tools/audit/audit-presentation.py` 生成到 `verification/{windows,macos,linux}-static/presentation/`，每项记录实际符号、源码 hash、调用链推演和限制。Windows/Linux 为严格静态审计，尚未运行目标系统；macOS 上述 Metal 离屏为真实运行，窗口/焦点/正常呈现仍待 R34。不能将其他平台图片或 C++ readiness fixture 当目标系统窗口证明。

在可靠目标环境先运行上述 Release/官方 lint/CTest，再运行 `ui-audit --presentation 输出目录`（Windows 位于 `build/release/tools/ui-audit.exe`，macOS/Linux 位于 `build/release/tools/ui-audit`）。输出需核对实际 Qt、后端、DPR、18 项 geometry、31 仅作为本次样本的委托数而非跨设备固定值、warnings、10 次 lifecycle 和 windowPolicy；委托门槛是非零且少于 100。macOS 优先隐藏 RenderControl；Windows/Linux 当前入口有 native show，默认遵守 VerificationWindow 的置底/拒绝焦点，用户明确要求可见验证的指定轮次允许前置/激活。所选策略不能可靠采集时停止并保留任务。

R34 的可见产品检查使用匿名库和 R17 交付源码：九尺寸、双主题、实际 DPR1/1.5/2，快速 open/back、退字连选/取消、四种 archiveRoute、介绍/歌单滚动、真实鼠标/触摸/键盘、退出时底层禁用和有效 Item 的焦点恢复。核对尚未验收的首次 show、hide/restore/minimize 与 flags/surface 重建，源码顺序和 revision 门禁不变；保持原结构≤1逻辑px、时序≤1个60Hz帧与既有 UI 像素门槛，默认不提高 Z 顺序，授权轮次按测试窗口契约前置。R18 接入运动 ports 后再验收实际相机/抽取 readiness；R23/R24 接入定位/播放后回归滚动和展示不改变播放的边界。当前主力平台同时回归 presentation、presentation-qml、navigation、title-reels 和完整 Release CTest。

## 可见窗口与顶部保持补验

2026-10-14：本机 Release 的可见 Metal/threaded 和 OpenGL/basic 均覆盖有效DPR1/1.5/2，每项九尺寸双主题18结构、10,000曲目、31个可见/复用委托（门槛仍为0<数量<100）、零QML warnings、介绍切换/快速重开和十次恢复通过。采用授权前台策略，不称为不干扰或其他显示设备已验证。

窄屏转宽屏时header的字体/高度在polish后改变，ListView原点可晚于layoutReady更新，曾使标题位于目标顶部之上4.58px。TrackList显式保持准备后的顶部，header高度改变合并restoreBeginning；移动、非首项键盘索引、滚动条按下及显式positionViewAtIndex取消保持，排队回调再次核对取消，不跳回用户已滚动的顶部。采集通过对应afterSynchronizing/presented revision排除先前在途帧。R18运动ports和自动产品回放范围见detail-motion.md；产品系统级有效焦点恢复、真实触摸/设备输入与R23/R24定位播放集成仍保留R34；组件结构通过不冒充这些产品检查。
