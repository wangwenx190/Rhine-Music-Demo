# 音乐库、搜索面板与歌曲定位

本文负责LibraryPanels/PanelContents、库/搜索面板、PanelController与TrackFocusController的展示及定位交接。数据身份、搜索匹配、扫描/Provider协议归[library-data.md](library-data.md)，详情视图归[presentation.md](presentation.md)，播放归[playback.md](playback.md)。

## 面板与筛选

自动创建的未分类流派名称与缺失歌手以空值保存在领域数据中，不把翻译后的占位文案当成元数据或状态标记。列表、浏览信息与详情通过genreDisplayName/artistDisplayName显示本地化提示；真实标签即使恰好与提示同名也保持原值，Provider查询仍接收原始数据。

`panels/LibraryPanel.qml`通过LibraryStore公开接口编辑绝对路径、选择文件夹、保存扫描、重扫/取消并显示实际扫描/标签/保存/错误/离线状态；路径转换统一由C++ QUrl完成。曲库统计与已用流派只随快照发布，不按媒体或视觉帧重建。FolderDialog不触碰歌曲标签，匿名Review不提供真实数据根。

搜索筛选由GenreRowsModel在C++测量和分行，字体/字体库变化才重新测量，宽度变化只重新分行；行通过QAbstractListModel供QML ListView虚拟化，显示顺序与22px内距/7px间距保持原规则。模型内容与测量/分行均为O(流派数量)，不随视觉帧运行。全部选项和空库仍保留“全部”。

`SearchPanel.qml`保留冻结music-app/music.css的标题、搜索框、流派按钮、49px contain封面/缺图音符、结果与无结果状态。结果ListView与按实际字体宽度换行的流派行均虚拟化；大量流派可在有界区域滚动，曲库大小不扩大可见委托池。面板标题和查询区域随列表滚动，重新打开恢复查询入口。`PanelController`复用SurfaceTransition的300/200ms、10/6px及中断语义，由唯一FrameClock的motionDt驱动；Popup关闭Qt样式自身的enter/exit动画，仅由SurfaceTransition管理可见生命周期和模态焦点；退出/重开只保留最后完成动作。

LibraryPanels只绘制模态外壳、背景和阴影；PanelContents负责内容组合、准备与焦点交接。面板背景复用现有可见内容，两个BackdropTexture按窗口实际DPR取整分配，SDR为RGBA8、HDR为RGBA16F并保留完整mip。源CSS sigma=8、3 sigma截断及线性采样配对仍以逻辑像素定义，sampleStep按逻辑宽高换算UV；附件分辨率不改变模糊的逻辑半径。阴影按源0/24/90/.13的矩形Gaussian积分直接绘制，不增加捕获附件。Loader只在面板可见生命周期保留背景；ResourcePlan记录两份颜色/保守深度和resize新旧重叠，Qt/driver内部开销仍未知。关闭销毁所属捕获，不新增生产readback、MSAA或独立时钟。

## 退出与歌曲定位

PanelController::selectResult只接纳搜索面板的稳定专辑/曲目ID，保留一个可替换意图，退出完成后发出resultRequested。显式close/reset、重新open及完成信号中的新命令会清除或替换旧意图；changed/closed同步重入后按revision复核，不能重新显示已关闭面板或交付过期定位。焦点操作和ListView真实几何仍由QML处理。

`TrackFocusController`验证稳定专辑/所属曲目身份，复用PresentationController的archiveRoute：必要时退字/归架，再以既有五次轨迹定位和重新打开，真正detailEnabled后才交付行号。当前打开专辑直接切歌单并滚动，不重新抽取。搜索结果和播放歌名使用同一个request接口，只读播放ID，任何定位均不调用媒体命令。最后目标覆盖未完成请求，回退/换库/重播/打开面板与用户滚轮/按下/触摸/按键/resize取消旧revealing revision；旧布局回调不能恢复高亮。

TrackList只采集原生列表实际几何并执行视图操作；TrackFocusController按已布局的歌曲行（不含CD标题）/页签尺寸居中，以实际首尾范围钳制滚动，持有origin修正与目标偏移状态；只对有界已实现项以显式祖先栈完成polish，保留子先父后及兄弟顺序，不递归；虚拟行实现导致originY或未显示行高度估计变化时，以实际目标行转换同一坐标基准，保持到目标的源cubic距离轨迹，避免远距离滚动跳变或目标尚未实现。滚动后按revision等待布局再聚焦；切换介绍页取消旧高亮，定位返回歌单时保留当前请求。滚动沿冻结music-track-focus的cubic in/out与min(700,320+距离×.12)ms；到达后聚焦目标行，以1000ms两次、0/.32/.48/.88/1的CSS ease-in-out高亮。减少动态立即滚动并保持高亮1100ms；高亮中途开启时立即显示完整静态高亮，并从切换时刻重新计1100ms。该定位消费唯一FrameClock的原始dt，隐藏暂停视觉，不修改歌曲或全局动效速度。

## 检查入口

`track-list-layout`隐藏创建生产TrackList，以深层视觉树检查完整polish顺序，不启动渲染或展开尺寸矩阵；`track-focus`使用实际Selection/SceneAdapter/Presentation状态机验证跨歌手列的最后目标、当前专辑复用、取消/过期回调、减少动态和播放隔离；搜索模型回归见[library-data.md](library-data.md#检查入口)。`library-panels-qml`（`ui-tests --library 输出目录`）使用实际MusicStage、九尺寸/双主题/DPR1/1.5/2、4000命中虚拟列表、真实Qt按键、模态/关闭、详情滚动聚焦、背景附件释放与默认隐藏策略；自身输出用于质量检查，不做图像评分或正常呈现性能推断。

`ui-tests --library 输出目录 --representative`使用桌面/窄屏、DPR1/2与双主题的既有代表组合，保留该范围内的实际输入、焦点及资料状态断言；省略选项保持完整库面板检查范围。设置测试的选择见[settings.md](settings.md#检查入口)。代表检查不冒称其余矩阵或物理设备事实。
