# Presentation 与详情界面契约

冻结定义为 `legacy-web/src/music-presentation.ts`、音乐入口 `music-app.ts` 的实际 ports、`ui-transitions.ts`、`music-title.ts` 及 `music.css`，来源标识沿用 `web-baseline.md`。三维抽取、换片与归位通过 DetailPorts 接入 DetailMotion 和 ReturningCasePool；同步轨迹、场景 readiness、拖动门禁与产品回放范围见 [detail-motion.md](detail-motion.md)。

`PresentationController` 是 GUI 线程的唯一展示状态所有者，保留 archive/opening/detail/switch-hiding/switching/hiding/returning/selecting。退 browse 后才请求相机；详情的场景 readiness 与当前 layoutRevision 都满足才显示文字。退字中的新输入替换一个 pendingSelection；搜索/歌曲定位的 archiveRoute 等归位和选中行完全就绪再重开。back 清掉 pending；reset 使旧回调和布局结果失效。嵌套完成路径批量发一次状态通知，重复 update 不发通知。各相机/选中 ports 不修改播放状态，不发网络请求。

两个 SurfaceTransition 使用同一 FrameClock 的 motionDt，保留 browse 720/140ms 与 detail 360/240ms，源码 Bezier、隐藏初始位移、退场位移和打断后的 `max(72ms, duration * sqrt(opacityDistance))`。改速不重置 progress，减少动态完成相同回调路径。一个 surface 只有一个可替换完成回调；revision 也覆盖 changed 信号重入。browse 的 veil/信息/导航/footer 分别改透明度，Header 保持自身绘制层级。详情退出期仍可见但禁用；InputRouter 与 SceneItem 的鼠标/触摸路径共同阻止底层选择。

DetailModel 持有不可变库快照及索引取得的当前专辑指针。prepare 一次计算八项 facts及介绍来源显示文本，保留标题/英文译名、编号、曲目/分碟/时长、介绍、本地备注、制作人员、来源 URL/许可证和目录；不会把全歌单序列化为 QVariant 数组。相同快照、专辑和编号不重置模型。TrackList 使用可见/复用委托和 100px 预留，曲目与制作人员各有模型类型稳定的 ListView；切 tab 重建相应视图，不让旧角色缓存跨模型类型复用。制作人员在prepare时按NFKC人名/来源分组，以trackId索引解析当前曲名，角色/来源/参与曲目分别去重；可见行按源210px最小列宽/26px列距分配，展开阅读保留全部本地署名与URL。大量曲目署名在高度不超过详情视口70%的独立ListView中虚拟化，避免单个制作人展开后创建全量文本节点；这是原生有界委托适配。布局回调只排一个，新准备替换其 revision；完成后以 positionViewAtBeginning 显示 header。退出焦点请求在绑定更新后交付，恢复仍有效且可交互的原 Item，旧展示请求不能重新取得焦点。

## 排版、tab与离线资料

Detail使用冻结最终CSS的宽度断点与aspect布局，content与caption分别锚定。返回按钮须先应用music-theme-switch.css最终竖屏覆盖（left22/top118），其余宽度断点与窄屏9px字号沿music.css；不能只读取较早的compact/mobile规则。返回按钮的显式字号遵守[source-defect-exceptions.md的SDE-01](source-defect-exceptions.md#sde-01--详情返回按钮字号被通用reset覆盖)，不复刻通用reset意外覆盖。事实网格、标题/译名、离线badge、分碟/曲目边框均按真实字体框与border-box布局；CSS normal曲名按整行fallback的ascent/descent分别取整，不能用固定主字体行高截断emoji。文本/字体键的行高缓存采用256KiB计费上限（字符串载荷及每项64bytes元数据估算），超大键不入缓存；不把计费值当allocator实占。长内容的thin scrollbar占11逻辑px；占位在布局后合并更新，避免宽度/高度绑定循环。

Tabs从滚动header的自然位置吸附到content顶部；两个类型稳定的列表保留同一滚动距离，内容变短时钳到有效末端。prepare才恢复顶部，用户滚动/切tab不触发prepare定位。指示条的180ms和内容的150ms/.35起始透明度沿冻结Bezier，由现有FrameClock一次缩放；打断从当前值继续，减少动态走相同终点，隐藏不推进时钟。所有tab均有Qt焦点/Accessible状态与左右键切换，退出期共同禁用。指示条在实际按钮x/width变化及prepared时同步布局，初始未构造完的按钮暂不读取；延后的焦点/定位使用所属对象方法与当前revision，视觉树销毁后不保留引用旧页签的闭包。

源backdrop blur(18px)使用sigma18、半径54的归一化高斯，两pass成对线性采样。先将滚动内容按预乘alpha合到场景/竖屏背景，再水平/竖直过滤，最后叠veil和原生DPR文字。一个可见Loader拥有三张RGBA8逻辑网格附件（两crop、一水平结果），每张为(contentWidth+108)×(tabHeight+108)；竖直结果直接合成，退出或完全裁切时释放Loader，不改三维资源键。该局部设计模糊不参与整场清晰度补偿。

介绍、备注、目录、流派、来源/许可和已有制作资料均可离线阅读；introduction/creditsLookup保留原JSON，展示error/uncertain/not-found/partial/matched、重试时间及未知status。查询暂未启用的文案独立于已保存内容，不发起请求或打开外部URL。

## 顶部与焦点

TrackList准备期保持header位于可视顶部；header高度/polish变化只合并一次restoreBeginning。用户移动、非首项键盘索引、滚动条或显式positionViewAtIndex即取消保持，排队回调再次核对，不能跳回用户已滚动位置。采集读取afterSynchronizing/对应呈现revision，不能将中间Row布局当按钮位移。

## 检查入口

`presentation`覆盖双ready门禁、退字连选/四种archiveRoute/取消重开/reset/速度/减少动态与10,000曲目。`presentation-qml`/`ui-tests --presentation 输出目录`使用生产Detail/TrackList；九尺寸、双主题、DPR1/1.5/2按test-gates.md的独立边界选择，不将全部54组合固定为每次门禁，必要入口调整按test-gates.md。跳第8000曲目、真实Qt tab点击/滚动距离钳制、介绍切换/快速重开和退出禁用在代表条件执行；恢复/释放先审所有权与取消路径，仅运行未决行为才按test-gates.md补必要转换。委托门槛0<数量<100，不把某次31个当固定值。结构≤1逻辑px，时序≤一个60Hz帧，warnings为空。

`node tools/migration/export-browse-ui.mjs --presentation`从冻结生产renderDetail/trackList/albumAbout及最终CSS导出独立结构；只读取DOM/字体框，外部请求被拦截。`python3 tools/migration/compare-presentation-structure.py 原生report.json 冻结report.json 输出.json`核对content/caption/返回按钮/标题/facts/tabs/曲目锚点与字号至≤1逻辑px。CSS采集先交付viewport及布局属性变更，再读取新DOM，不用上一次样式状态。运动ports与播放隔离消费边界分别见detail-motion.md及计划R23/R24；目标系统/物理输入/焦点/窗口欠验只在R36/R17-P维护。
