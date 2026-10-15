# Presentation 与详情界面契约

冻结定义为 `legacy-web/src/music-presentation.ts`、音乐入口 `music-app.ts` 的实际 ports、`ui-transitions.ts`、`music-title.ts` 及 `music.css`，来源标识沿用 `web-baseline.md`。三维抽取、换片与归位通过 DetailPorts 接入 DetailMotion 和 ReturningCasePool；同步轨迹、场景 readiness、拖动门禁与产品回放范围见 [detail-motion.md](detail-motion.md)。

`PresentationController` 是 GUI 线程的唯一展示状态所有者，保留 archive/opening/detail/switch-hiding/switching/hiding/returning/selecting。退 browse 后才请求相机；详情的场景 readiness 与当前 layoutRevision 都满足才显示文字。退字中的新输入替换一个 pendingSelection；搜索/歌曲定位的 archiveRoute 等归位和选中行完全就绪再重开。back 清掉 pending；reset 使旧回调和布局结果失效。嵌套完成路径批量发一次状态通知，重复 update 不发通知。各相机/选中 ports 不修改播放状态，不发网络请求。

两个 SurfaceTransition 使用同一 FrameClock 的 motionDt，保留 browse 720/140ms 与 detail 360/240ms，源码 Bezier、隐藏初始位移、退场位移和打断后的 `max(72ms, duration * sqrt(opacityDistance))`。改速不重置 progress，减少动态完成相同回调路径。一个 surface 只有一个可替换完成回调；revision 也覆盖 changed 信号重入。browse 的 veil/信息/导航/footer 分别改透明度，Header 保持自身绘制层级。详情退出期仍可见但禁用；InputRouter 与 SceneItem 的鼠标/触摸路径共同阻止底层选择。

DetailModel 持有不可变库快照及索引取得的当前专辑指针。prepare 一次计算八项 facts，保留标题/英文译名、编号、曲目/分碟/时长、介绍、本地备注、制作人员、来源 URL/许可证和目录；不会把全歌单序列化为 QVariant 数组。相同快照、专辑和编号不重置模型。TrackList 使用可见/复用委托和 100px 预留，曲目与制作人员各有模型类型稳定的 ListView；切 tab 重建相应视图，不让旧角色缓存跨模型类型复用。布局回调只排一个，新准备替换其 revision；完成后以 positionViewAtBeginning 显示 header。退出焦点请求在绑定更新后交付，恢复仍有效且可交互的原 Item，旧展示请求不能重新取得焦点。
## 顶部与焦点

TrackList准备期保持header位于可视顶部；header高度/polish变化只合并一次restoreBeginning。用户移动、非首项键盘索引、滚动条或显式positionViewAtIndex即取消保持，排队回调再次核对，不能跳回用户已滚动位置。采集读取afterSynchronizing/对应呈现revision，不能将中间Row布局当按钮位移。

## 检查入口

`presentation`覆盖双ready门禁、退字连选/四种archiveRoute/取消重开/reset/速度/减少动态与10,000曲目。`presentation-qml`/`ui-audit --presentation 输出目录`使用生产Detail/TrackList，九尺寸双主题18结构、跳第8000曲目、介绍切换/快速重开、退出禁用、十次恢复/最终释放；委托门槛0<数量<100，不把某次31个当固定值。结构≤1逻辑px，时序≤一个60Hz帧，warnings为空。

源码审计`python3 tools/audit/audit-presentation.py`；门禁分组/窗口授权见公共契约。运动ports与播放隔离消费边界分别见detail-motion.md及计划R23/R24；目标系统/物理输入/焦点/窗口欠验只在R34/R17-P维护。
