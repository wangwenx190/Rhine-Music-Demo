# 普通歌曲播放、队列与输出设备

冻结来源为music-app.ts的playAlbum/播放按钮/Space接线及music-player.ts的toggle/previous/next/stop/seek。按迁移计划的用户差异，仅使用QtMultimedia公开API；没有无缝、歌曲淡变、PCM处理、自建解码/输出引擎或外部FFmpeg进程。BGM与界面音效由各自控制器负责，不共享歌曲播放器或音量。

## 所有权与状态

`PlaybackController`在GUI线程长期拥有一个QMediaPlayer、一个QAudioOutput和QMediaDevices。媒体后端工作由Qt承担；QML重建、浏览/搜索/排序/运动帧与窗口隐藏不重建或轮询播放器。main的aboutToQuit停止播放，控制器销毁先换代、断开操作连接、停止/清空来源，再由成员顺序销毁player和output。

playTrack以LibrarySession的专辑ID索引获取专辑，按所属专辑曲目ID去重建立一次保留值队列；每次启动均检查绝对本地QUrl，不接受远程/qrc/旧PCM字段。下一首按队列递进，最后一首结束只处理一次并停止；上一首在进度>3秒时只归零，其他情况退一首或重播首曲。停止释放媒体来源并清零位置，保留当前歌曲/队列，后续播放从当前曲目重来；无当前歌曲时Space/播放按钮播放当前浏览专辑。

库替换不打断正在播放的保留队列。通用setPlaybackIds仍验证当前库；只有PlaybackController可经私有受控入口同步已验证旧队列的孤立ID。导航与播放身份独立，跨专辑重名trackId仍按albumId+trackId判定歌单当前行。无效新目标发rejected通知，保留现有播放；当前媒体错误保留Qt原因，并提供换曲/重试。

每次来源重设与停止推进source revision。替换前断开旧操作连接；回调检查revision、当前source和实际Qt状态，旧排队回调/重复EndOfMedia不能提交新来源。自动下一首排到当前信号栈之后，并再次检查revision与Playing意图。Intent为Stopped/Paused/Playing，加载完成、设备恢复均不能覆盖用户暂停/停止。playbackRate始终为1，与动效速度没有连接。

每次source revision重设当前运行支持状态为Unknown；当前歌曲mediaSupport由实际Qt播放状态、hasAudio及正位置进度确认Supported；InvalidMedia/FormatError或已加载但无音轨确认Unsupported，Loaded不等于Supported。该运行状态不改写历史元数据索引，不冒充已验证所有格式或实际听感；无声探测/持久化索引仍归library-data.md。

## 进度与通知

position/duration/seekable来自公开属性与信号；位置通知读取当前Qt位置，避免把延迟参数当成最新状态。拖动期间只保留预览值，不发setPosition；释放一次提交后恢复跟随。毫秒进度条显式使用1000ms键盘步长，避免Qt默认小数步长转整数后归零。换源/停止取消旧拖动，旧控件的释放不能seek新歌曲。不可seek或未知总时长禁用进度条；公开seek接口仅在seekable时接受非负目标，已知duration时钳制上界。

媒体信号只通知当前歌名/状态/进度/设备等相关控件。没有视觉帧查询、定时轮询、逐帧JSON、文件读取或全库模型重置。曲目值队列内存O(当前专辑曲目)，一次构建O(专辑曲目)；索引查询O(1)，上一/下一与进度更新O(1)。拖动、结束回调和设备刷新各只有一个当前状态/有限完成动作。

## 设备恢复

系统默认与显式设备选择分开保存。设备ID使用Qt公开id的规范base64；列表与实际QAudioDevice来自QMediaDevices，不构造模拟描述符或修补私有字段。默认策略每次通知读取最新系统默认；显式设备仍在时不随系统默认改变。显式设备消失则清除偏好、回系统默认并提示。设备通知合并成一个QObject所属queued刷新，不逐个绑定过期列表。

没有输出时暂停并脱离output，保留意图和最后有效位置；新歌曲只保留待播来源，不启动解码。保存的有效位置不会被设备失效前的临时0覆盖，显式seek(0)/停止/换曲仍正确清零。重连重新绑定公开output、恢复可seek位置，再按当前意图决定是否播放；不可seek恢复起点。输出对象保留音量，普通设备切换直接setDevice，不重建player。仅待播来源或无输出期间观察到实际Qt错误时重新设置source，不按平台名/Qt版本盲目重建会话，也不自动重试既有格式错误。

REFDB-04只采用公开API的无输出/意图/恢复位置隔离思路和事件顺序。实际读取文件hash、作者、MIT许可及采用边界见[随包参考说明](../../resources/licenses/qt-playback-reference-NOTICE.md)；没有引入其私有布局补丁、在线来源或模拟设备描述符。

## 界面与检查

Header沿用原播放/停止/当前歌名容器，歌名定位复用TrackFocusController。播放与输出面板集中提供上一/下一、进度/时间、系统默认/实际Qt设备、歌曲音量及Qt错误/重试，保留详情静态排版。Space复用InputRouter的输入所有权判定，编辑器/滑块/Popup及修饰快捷键保留自身输入；进入库/搜索或定位不调用媒体命令。音频面板是本轮入口，完整设置与其他声音控制仍归对应后续轮次。

`playback`使用现场生成的匿名RIFF/WAV和损坏样本，真实执行Qt播放/暂停/seek/停止、自然结束/换曲与错误；另用真实Qt设备对象或空列表通知覆盖无输出待播、加载替换、暂停/停止重连、最后有效位置、显式选择回退及重复/晚到事件。通知注入仅验证状态分支，不作为物理拔插证据，禁止伪造QAudioDevice私有句柄。

`playback-qml` / `ui-audit --playback 输出目录`通过实际MusicStage播放按钮启动低音量匿名样本，回放九尺寸、双主题、DPR1/1.5/2、输出面板与播放进度、搜索/库替换/定位播放隔离及默认隐藏策略；记录实际GPU/Qt/系统/音频设备和输入/二进制hash。测试输入生成器只在tests/support，产品没有PCM路径。自身输出只用于实际质量检查，不做图像评分，不代替正常VSYNC、物理输入/设备拔插、格式全集或听感。

`python3 tools/audit/audit-playback.py`绑定COMMON-01–05/PERF-01–07、三平台源码/hash和冻结/REFDB来源；目标欠验与原门槛只在迁移计划R24-P维护。共享测试选择与复用见test-gates.md，窗口与产物边界沿公共契约。
