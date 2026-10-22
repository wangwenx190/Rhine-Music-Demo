# 连续运动内核与帧源契约

时间单位为秒，计算使用 double。`src/motion/` 编入 `YalahooCore`，不依赖 Qt Quick、媒体或操作系统 API；仅时钟和组合控制器使用 QObject。`App.qml`内唯一`MusicStage.qml`的一个`FrameAnimation` 向注入的 `MotionController.clock` 提交 `frameTime`，其 `frame(dt, motionDt)` 是后续生产视觉系统的统一输入。禁止再建 QTimer、另一个 FrameAnimation 或媒体驱动的视觉循环。

- 减少动态且无呈现过渡/歌曲定位高亮时，唯一FrameAnimation仅消费待处理帧；SceneAdapter、运动请求和InputRouter失效唤醒同一时钟。待处理标记在frame回调前消费，帧内晚到失效保留到下一帧；正常波浪与活动动画仍连续推进。
- `FrameClock` 在入口钳制 `dt` 到 0–0.05 秒，忽略非有限输入；`motionDt=dt*speed`，速度钳制到 0.25–3，只在这里缩放。改速不修改任何轨迹的起点或已用时间。普通 `visualTime` 积累 motionDt。
- `startOpening()` 重置独立开场时间，从相同 dt 积累至 11.6 秒；不受 speed 影响。`finishOpening()` 在运行时到达终点并发出一次完成通知。开场UI与交接由[opening.md](opening.md)的OpeningController消费。
- 窗口隐藏、最小化，或应用进入 Hidden/Suspended 时停止唯一帧源并停用时钟。恢复后丢弃首个帧间隔，避免后台时间消耗轨迹；不保留积压，不改变媒体对象。
- `ScalarSpring::update(target, rate, dt)` 原样计算临界阻尼解析解及精确加速度。`observe()` 计算从弹簧交接到五次轨迹的加速度。rate=3.7/9/6.5 分别供普通真实列/详情/总览使用。
- `QuinticTrack` 保存位置、速度、加速度、目标、系数及累计时间，位置及两阶导数用Horner形式求值；`observe()` 仅在未运行时接收状态，`retarget()` 从当前 x/v/a 接续到静止终点，同目标返回 false 且不重启。`translate()` 只改变位置、目标和常数项；`finish()` 到达精确目标并归零 v/a；`reset()` 结束轨迹并解除目标记忆，保留当前状态。
- `ArchiveMotion` 按 rail/column/shoulder/lane 顺序保存四条轨迹，时长统一为 `1.4+clamp((hypot(lanes,rows/8)-1)/7,0,1)`。observe 接收显示弹簧及其目标/rate，retarget 接收显示 x/v 并保留上次精确 a。一项目标变化时四条轨迹共用新时长，同组目标不重启。单独使用内核时 translate 后需同步平移显示弹簧；`MotionController::translate()` 已同时处理二者。
- `ColumnScalar` 逐式移植冻结 `MusicRealColumnScalar` 的弹簧/五次切换、settled 阈值及 finish。实际列成员/选择/居中布局归real-columns.md。
- `MotionController` 从唯一帧信号更新已缩放时间。observe始终保留精确加速度交接，仅在显示弹簧的值或导数变化时发送stateChanged；静止观察不重新唤醒帧源。新目标递增 revision；只为已完成的请求发送 `completed(revision)`，替换掉的目标不完成，重复 finish 不重发。减少动态立即完成当前请求和运行中的开场时钟，回调可用于后续焦点/状态交接。与媒体状态没有连接。

`MotionFunctions` 保留冻结 motion.ts 的平滑、波浪、抽取、空闲、列强度、合成场与归位函数及 InspectionLift/AlignmentEpsilon。精确零权重的时间窗跳过对应packet/场计算；settling传播尚未到达时直接返回零，不截断非零衰减尾部。场景消费者与开场分别见scene.md及opening.md。唯一主动行为差异是按长期契约给 archive 请求增加同目标不重启保护；冻结真实列已有相同保护。

## 数学输入与检查

`node tools/migration/export-motion-golden.mjs`直接执行映射后的冻结motion/archive/real-columns/boot-timing源码，生成`tests/fixtures/motion/kernel-golden.json`及来源hash；Node仅用于维护导出。

`motion`检查两组各694源状态、13函数向量、弹簧时间分割、五次C2、同目标/精确终点/revision、隐藏恢复及片头速度独立，x/v/a各≤1e-6。`runtime-motion`单独验证真实生产帧源的后台不增长、恢复首帧dt=0及减少动态终点；窗口选择由test-gates.md决定。新增QObject源后旧autogen引用失效时以干净Release重建moc。

`MotionPanel`使用同一个SettingsController/FrameClock速度值，范围0.25×–3×、步进.05，恢复为1×；折叠/关闭保留速度。既有设置页速度控件读取同一值，不建立第二套时间或偏好。面板滚动、控件方向键和空格由输入所有权隔离；`experiments`覆盖在途往返改速不改变当前轨迹/revision，片头每步仍只增加未缩放dt。光照面板及共用验证入口见lighting.md。
