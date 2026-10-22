# 真实列、导航边界与会话位置

`RealColumns`、`ColumnMemory`、`NavigationAvailability` 与 `SelectionController` 位于 `src/render/`。来源为冻结提交 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `music-real-columns.ts`、`music-app.ts`、`scene.ts` 和 `viewport-layout.ts` 的音乐生产路径，读取经 `frozen-git.mjs` 的历史映射解析。

真实模式的合法行是 12…11+count，中点为 12+(count−1)/2。未选列独立居中；当前列单独移动。离开列的深度和活动权重按 1.4–2.4 秒五次轨迹归中，返回时从当前位置、速度和加速度重定向。相邻行使用 3.7 临界阻尼，详情内部行移动使用 9；程序化定位、切列及进行中的五次轨迹保留原时长规则。唯一 FrameClock 已缩放的 dt 只应用一次，减少动态完成轨迹；overview 插值入口由后续总览集成使用。

真实模式中每个稳定专辑 ID 只保留一个合法物理格。9×48 池按每列实际显示深度虚拟化，长列不受 48 限制；越界列和行不产生实例、封面租约或拾取对象。实例、选中/归位副本、SceneFrame、GPU 阴影及 hitTest 共享变换。纵向位置为 `(row−columnDepth)×0.62−2.17`；全局 rail 的抵消不让别列随当前列上下移动。默认 idleWave 使用绝对时间，其他列操作不重启它；选择波纹只作用于本列，活动权重控制闲置/settling 混合。

SelectionController 统一提供 `navigationAvailability()`、`moveRow()`、`navigateRow()`、`moveLane()`、`enterColumn()` 与稳定 ID `select()`。首尾请求在改状态前拒绝，不生成脉冲、归位副本或新选择 revision。真实模式上下有限、左右循环；填充模式保留物理循环，包括单张填充列。SceneItem 的键盘、滚轮、触摸共用这些入口，滑尺/详情后续通过同一控制器接入。触摸遵循冻结 swipeDirection：主要距离≥36逻辑像素，≥次要距离×1.3，时长≤1400ms。测试直接向自身 Qt 对象投递事件，不激活窗口。

列记忆默认开，按列 key 存稳定 albumId。快照或排序换代时重建索引并重置为每列首张；切开关不修改当前选择、实例或播放。关闭时不读写记忆；跨列选择从目标列实际显示深度取最近合法行，归中途中快速折返也如此。填充模式使用实际 rail 深度并保留正负周期。重新打开记录当前列，其他列原记忆保留；程序化 enterColumn 在填充模式取相机附近物理列。

模式切换保留稳定 albumId，规范化物理格并平移既有运动轨道；真实列从当时实际 rail 深度开始归中。清除旧归位副本和脉冲，重置光带跟随。真实列的光带输入启用 independentColumns，复用 RowLightController 的世界坐标原点换算。搜索、列选择、模式/记忆开关不调用播放接口。

`PreferencesStore` 在 `--data-dir` 或 QStandardPaths::AppLocalDataLocation 下保存 `preferences.json`，只写 arrayMode 和 rememberColumnPosition，保留未知 JSON 字段，不写会话行位。启动读取限制为 1MiB；损坏/不可读文件保留原件并显示错误。QSaveFile 原子保存由专用单线程池处理，一项在途任务加一个最新待保存文档，快速切换合并；工作线程不访问控制器/QML。保存失败通知 GUI，退出时仅等待本服务并提交最后一个待保存值。

## 性能与生命周期

快照/排序边界建立 O(N) ID→格、封面与列记忆索引，沿用 LibraryModels 的 zh_CN/NFKC/稳定 ID 排序及未知流派首次出现顺序；重复专辑 ID 在边界去重。帧中最多 432 格与四个接管副本，不访问全库；列数增加不扩大池、图集、buffer 或场景树。

新增 CPU 状态为 C 列的两个 ColumnScalar、count/midpoint/time/epoch 与对齐，以及 C 个会话 key→ID；实际字段 sizeof 随 ABI 计算，不是 RSS 实测。快照边界的 counts 临时数组为 O(C)、身份去重集合为 O(N)，建立结束即释放；帧临时实例数组仍最多 436 项。生产分组 C≤N，空库 C=0；新增 GPU 附件和纹理为 0，沿用既有 ResourcePlan 的实际尺寸清单，不能把减少 drawCount 当作所有固定 GPU 资源减少。

RealColumns 保存 O(C) 的两个标量状态，但 update 只推进统一时间，sample 按可见列或输入目标的时间差求值。临界阻尼的精确解和五次多项式可在未观察区间合并 dt；目的不变且不用 dt 积分近似。sample 每个时间点保留状态，选择前取样，重定向不丢导数；减少动态的 epoch 使未观察列也完成旧目标，新目标在下一次 update 后完成。该优化按冻结独立实现的 20/30/60/120fps、0.25/1/3×、每帧完整系列核对位置/速度/加速度，不能只检查结束值。归中列不在每帧全量遍历，波浪相位仍是共同绝对时间。

共用 R12/R11 的封面 generation、worker/待解码/上传上界、不可变快照和资源计划。真实列过滤后只为合法可见 ID 请求封面；数量/相机/主题变化不改变目标兼容键。模式切换不销毁渲染树，原像素租约按可见身份更新；库替换、隐藏恢复和销毁沿用既有取消/重放规则。真实模式不做循环坐标 rebase，填充 rebase 继续平移运动、归位和绝对波浪原点。

## 检查入口

`real-columns`使用`tests/fixtures/real-columns/trajectories.json`的独立源输入；`node tools/migration/export-real-columns-golden.mjs`生成该轨迹fixture与独立源场数值`fields.json`。核对x/v/a≤1e-6，包含20/30/60/120fps与.25/1/3×、空/单/奇偶/49/106列、长列尾部及归中打断。

Review `real-columns-gpu`检查有效实体/边界事件、列记忆/模式/库替换、物理ID与rebase、封面租约/上传、资源复用与一次SG重建；偏好检查未知字段、最新值合并与损坏原件。输入事件语义只在[navigation.md](navigation.md)维护。入口分组见test-gates.md，剩余目标验收归计划R36/R19B-P。
