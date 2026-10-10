# 连续运动内核与帧源契约

时间单位为秒，计算使用 double。`src/motion/` 编入 `YalahooCore`，不依赖 Qt Quick、媒体或操作系统 API；仅时钟和组合控制器使用 QObject。`App.qml` 的一个 `FrameAnimation` 向注入的 `MotionController.clock` 提交 `frameTime`，其 `frame(dt, motionDt)` 是后续生产视觉系统的统一输入。禁止再建 QTimer、另一个 FrameAnimation 或媒体驱动的视觉循环。

- `FrameClock` 在入口钳制 `dt` 到 0–0.05 秒，忽略非有限输入；`motionDt=dt*speed`，速度钳制到 0.25–3，只在这里缩放。改速不修改任何轨迹的起点或已用时间。普通 `visualTime` 积累 motionDt。
- `startOpening()` 重置独立开场时间，从相同 dt 积累至 11.6 秒；不受 speed 影响。`finishOpening()` 在运行时到达终点并发出一次完成通知。它提供计时接口，开场 UI 和交接仍由 R20 实现。
- 窗口隐藏、最小化，或应用进入 Hidden/Suspended 时停止唯一帧源并停用时钟。恢复后丢弃首个帧间隔，避免后台时间消耗轨迹；不保留积压，不改变媒体对象。
- `ScalarSpring::update(target, rate, dt)` 原样计算临界阻尼解析解及精确加速度。`observe()` 计算从弹簧交接到五次轨迹的加速度。rate=3.7/9/6.5 分别供普通真实列/详情/总览使用。
- `QuinticTrack` 保存位置、速度、加速度、目标、系数及累计时间；`observe()` 仅在未运行时接收状态，`retarget()` 从当前 x/v/a 接续到静止终点，同目标返回 false 且不重启。`translate()` 只改变位置、目标和常数项；`finish()` 到达精确目标并归零 v/a；`reset()` 结束轨迹并解除目标记忆，保留当前状态。
- `ArchiveMotion` 按 rail/column/shoulder/lane 顺序保存四条轨迹，时长统一为 `1.4+clamp((hypot(lanes,rows/8)-1)/7,0,1)`。observe 接收显示弹簧及其目标/rate，retarget 接收显示 x/v 并保留上次精确 a。一项目标变化时四条轨迹共用新时长，同组目标不重启。单独使用内核时 translate 后需同步平移显示弹簧；`MotionController::translate()` 已同时处理二者。
- `ColumnScalar` 逐式移植冻结 `MusicRealColumnScalar` 的弹簧/五次切换、settled 阈值及 finish。实际列成员、选择、计数、行位和居中布局由 R13 接入，没有提前创建列控制实现。
- `MotionController` 从唯一帧信号更新已缩放时间。新目标递增 revision；只为已完成的请求发送 `completed(revision)`，替换掉的目标不完成，重复 finish 不重发。减少动态立即完成当前请求和运行中的开场时钟，回调可用于后续焦点/状态交接。与媒体状态没有连接。

`MotionFunctions` 保留冻结 motion.ts 的平滑、波浪、抽取、空闲、列强度、合成场与归位函数及 InspectionLift/AlignmentEpsilon。它们只是数学函数，R05 不实现实际场景或开场动画。唯一主动行为差异是按长期契约给 archive 请求增加同目标不重启保护；冻结真实列已有相同保护。

## 验证与来源

```powershell
./tools/build/verify-r05.ps1
./tools/audit/audit-r05.ps1
```

常规验证只使用 Release。首次给现有构建树增加 QObject 源文件时，如本机旧 autogen 依赖仍引用原源码集合，执行 `./tools/build/verify-r05.ps1 -Clean` 重建 Release moc 产物。Windows 证据在 `verification/windows/motion/`；平台矩阵在 `verification/{macos,linux}-static/motion/`。

`MotionTest` 对照 R01 的 694 个输入帧，并对照新增的 694 个非零初始导数/反向/改速/长帧/负帧/rebase/减少动态帧，分别记录 x/v/a 最大绝对误差（门槛 1e-6）。另验证 13 组全函数向量、弹簧时间分割恒等性、五次 C2 接续、同目标、终点精确归零、恢复、开场速度独立与 revision 回调。`runtime-motion` 使用真实生产 QML 窗口隐藏/最小化并恢复，检查后台计时不增长、两次恢复首帧 dt=0、减少动态终点和无媒体来源。

新增 oracle 位于 `tests/fixtures/motion/kernel-golden.json`，仅用现有冻结 Web 开发依赖重建：

```powershell
node tools/review/export-motion-golden.mjs
```

导出器读取冻结 Git 提交的 motion.ts、music-archive-motion.ts、music-real-columns.ts、music-boot-timing.ts，使用已有 TypeScript 去除类型后执行原函数体；archive-loop.ts 仅解析实际行/列间距常数，不调用其数据加载路线。源 hash 与 R01 oracle hash 都写入 fixture 并在验收脚本核验。Node/TypeScript 仅用于重建测试数据，原生构建、测试和运行不依赖它们。
