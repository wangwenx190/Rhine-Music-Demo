# 设置、偏好与实例仲裁

本文唯一维护SettingsController/SettingsPanel、PreferencesStore的schema/持久化/显式偏好导入，以及SingleInstance的启动仲裁。资料查询与LibraryStore归[library-data.md](library-data.md)，声音归[playback.md](playback.md)，画质与光场数值分别归[occlusion.md](occlusion.md)、[lighting.md](lighting.md)。

## 设置与持久化

SettingsPanel沿冻结music-app的renderSettingsPanel/music.css：两列主题卡、排列、介绍/制作信息、阵列与列记忆、景深设置、减少动态/速度、全屏/开场重播/界面重载、三路声音和折叠MusicBrainz配置。面板沿用LibraryPanels的几何、模态输入、唯一SurfaceTransition与背景资源；来源布局与当前适配判定见source-fidelity-review.md。歌曲、BGM、音效及设备接口只由playback.md维护，画质字段及归一化只由occlusion.md维护。

SettingsController::updateQuality在C++合并单字段与当前完整画质值，再交RenderQuality归一化；未知字段拒绝，不重置其他选择。SettingsController按字段路由至既有LibrarySession、SelectionController、MotionController/FrameClock、SelectionLighting和RenderQuality。主题只监听目标nightChanged，动画渐变不保存偏好；速度不改变开场或歌曲速度。排列显式更新列模型，theme/speed不重建库/媒体/材质/封面。App的reloadRequested关闭并完成面板退出，销毁/重建MusicStage视觉树；原生控制器、正在播放对象、音源/意图、数据、用户画质和会话列记忆继续存活。重载先取消资料请求generation，QML异步结果不能回写旧视图。

原生preferences.json的schemaVersion为1；无版本对象在显式导入或读取时归入1，其他版本/坏JSON/非法已知字段保留原文件并阻止自动覆盖。theme、sortMode、arrayMode、rememberColumnPosition、singleInstance、reduced、motionSpeed、volume/bgm/bgmVolume/sound/soundVolume、lighting和renderQuality保存当前选择；renderQuality仅保存艺术效果参数，原生尺寸与固定生产采样由occlusion.md维护。已知字段校验类型、离散选项及音量/速度范围；画质数值交唯一RenderQuality归一化，不改变其未知扩展。未知根字段与对象扩展在更新/导入/保存中保留。会话各列位置仍只在ColumnMemory，不写偏好JSON。

`singleInstance`为布尔值，默认开启，设置面板可修改，重启后生效。普通产品在启动资料Provider、声音、曲库或创建窗口前，以规范化数据目录的QLockFile和仅当前用户可访问的QLocalServer仲裁；相同目录再次启动只请求已有窗口显示/恢复与激活，确认后退出，不转发导入/扫描参数。不同数据目录相互独立；关闭此模式允许多开。持锁进程不会因运行时间过长被驱逐；崩溃锁按Qt的死进程判定恢复，锁/端点/响应失败明确报错，不能降级多开。演示与验证入口不读取用户此项设置、不参与产品实例仲裁。启动阶段的有界IPC等待不进入常规交互路径。

PreferencesStore在事件循环合并变化，只有一个在途worker保存和一个最新意图；JSON序列化与QSaveFile提交在worker，禁止direct-write fallback，文件仅owner读写。显式导入规则见下节；原文件不写。损坏/未知schema使用内存默认值并暴露错误，写失败保留旧文件、不自动重试失败快照；派发时只消费该快照的保存意图，在途期间或错误观察者提交的新设置均保留一次独立的保存机会。析构只完成已接受的最后保存。

介绍/制作信息/MusicBrainz区域的来源、当前查询、全库补缺、取消、网络开关及配置行为遵守[library-data.md的联网资料与离线缓存](library-data.md#联网资料与离线缓存)，不在UI另建状态或请求机制。

## 显式偏好导入

`--import-preferences /绝对/浏览器导出.json`或`PreferencesStore::importJson`只读用户明确提供的对象JSON（≤1MiB），不读取浏览器数据库。worker解析后合并未知字段并原子保存；单个在途导入，期间新设置写入按revision拒绝旧结果。可与普通扫描组合，不能与匿名演示或验证参数组合。

## 检查入口

`settings`检查完整偏好持久化/导入/坏文件/未知schema、字段路由、原子合并保存、配置未知字段与重载状态保留。`settings-qml`（`ui-tests --settings 输出目录`）完整工具可在实际MusicStage上覆盖九尺寸、双主题、DPR1/1.5/2、折叠/滚动/Qt输入/反向打开、离线按钮及测试Provider状态/能力，另在真实App上十次重建视觉树并核对播放源/意图/用户画质。窗口分类按[test-gates.md](test-gates.md)执行；截图只检查自身正常输出，无图像评分或屏幕/VSYNC推断。

`single-instance`使用临时数据根及真实子进程验证锁/IPC；物理激活、最小化恢复及目标平台运行缺口只由迁移计划R36维护。工具完整重建次数属于覆盖能力，实际执行按[test-gates.md](test-gates.md)选择。
