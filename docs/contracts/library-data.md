# 数据模型、排序、搜索与离线资料契约

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 中的 `src/music-types.ts`、`music-order.ts`、`music-search.ts`、`data.ts:setMusicAlbums`及`scripts/music-library.mjs`。本文管理值模型、身份/排序/搜索、本地扫描/存储序列化与Provider协议；媒体/UI消费者按计划对应轮次接入。

`src/core/MusicTypes.h` 中的 Album/Track/Producer/Genre/LibraryRoot/LibrarySnapshot 是值类型。专辑、曲目、制作人员、来源、介绍/制作查询、MusicBrainz、扫描与批量进度字段均保留；可选技术参数不猜测。各级 `fields` 保留未知扩展与当前元数据，写回时覆盖已知字段。已由值模型持有的子数组不再重复存入扩展对象。`readLibrary` 使用临时值解析，重复/错配 ID 或非法集合返回错误，原快照不变。`Snapshot` 是共享的 const 快照；`LibrarySession::setSnapshot` 仅在 GUI 线程应用一次模型重置。

歌曲 `sourceUrl`、目录 `folder` 和 roots.path 使用本地 QUrl；绝对 Unicode 原生路径用 `QUrl::fromLocalFile`，不从当前目录猜相对音乐路径。歌曲 URL 只接受本地文件；封面额外允许 qrc。来源/候选链接保留文本。显式目录导入见下节。`MediaSupport` 为 Unknown/Supported/Unsupported，只有实际媒体状态才能更新；无声标签加载不证明可解码播放，不按扩展名猜测。

排序使用 zh_CN QCollator、numericMode、CaseInsensitive；文本 NFKC、合并空白、trim、lowercase。相等时按规范化字符串、ID 稳定排序。流派模式保持快照顺序，列顺序为已使用的配置流派、再按首次出现补充未配置流派；列用流派 ID 分组，重名不合并。歌手模式用规范化 key 分组，显示原始歌手经空白整理的名称，空歌手为“未知歌手”。专辑模式每列 12 张，标签为 01–12 等；slotStride=max(32, 最大列长+12)。QCollator 平台后端可能与浏览器 ICU 排列细节不同；视觉 fixture 的既有列顺序仍以 `library-order.json` 固定。API 依据：[QCollator](https://doc.qt.io/qt-6/qcollator.html)。

排序时每张专辑的标题/歌手规范化键预计算一次，比较器保留相同QCollator、规范化字符串及ID的顺序；列位置用key→index索引查找，避免不同歌手/流派很多时反复扫描已建立的列。索引仅用于查找，列顺序仍由原规则确定，不迭代QHash来决定展示顺序。算法与成本审计按 [performance.md](performance.md) 执行。

SearchListModel按不可变快照建立专辑ID→索引、每专辑曲目ID→索引及搜索字符串/流派名缓存；同一快照改变query只归一化查询，复用全部条目。查询结果保留相同前后缀，至多一次区间删除和插入，不按键重置模型；快照更换/清空时事务重置并重建/清空索引。role与resultAt按稳定ID索引读取，曲目定位仍在所属专辑内，不假设跨专辑曲目ID全局唯一。内存为O(专辑+曲目+搜索文本)，查询为O(总搜索文本+结果)，不改变匹配和返回顺序。

搜索逐式保留 `music-search.ts`：仅 trim 和 lowercase，匹配专辑标题/歌手/流派显示名（缺失则“未分类”）与曲目标题/歌手；匹配专辑后仍逐曲匹配，空查询只返回专辑，流派过滤按 ID。不匹配备注、制作信息或原始流派数组，也不加入排序的 NFKC/空白归并。`tools/review/export-search-golden.mjs` 从 Git 冻结源码去掉类型声明后执行原函数，生成 216 个用例与源码/fixture hash；Node 仅用于可选的 Web 验证数据导出，原生运行、CMake 和 CTest 不依赖它。

模型角色在 `LibraryModels.h` 的枚举及 `roleNames()` 固定，新角色只能追加。AlbumListModel 提供 albumId/title/artist/year/genreId/coverUrl/folder/tracks/producers/description/descriptionSource/introduction/creditsLookup/online/localNote/offline/albumData，另有 count/columns/slotStride。Tracks 中保留每首稳定 ID、源 QUrl 和 mediaSupport。SearchListModel 提供 kind/albumId/trackId/title/artist/count。QML 类型通过 `src/app/QmlTypes.h` 的 foreign 类型注册，不允许 QML 自行创建或更换 Provider。

LibrarySession 的 navigationAlbumId/navigationTrackId 与 playbackAlbumId/playbackTrackId 独立。`navigate`、`search`、`sort`、`requestMetadata` 不调用播放或修改播放 ID；播放ID只由受控C++入口同步：通用setPlaybackIds验证当前库，PlaybackController可为已验证并保留的旧队列同步孤立ID；Qt媒体状态与设备边界见[playback.md](playback.md)。库替换使过期导航失效，播放 ID 保留以便后续正在播放的媒体独立存活。

IMetadataProvider 构造注入，固定 Introduction/Credits/Enrichment/cancel 异步接口。AlbumQuery 预留批量 ID、联系人、enabled/force/configuration；ProviderCapabilities 预留各类能力，产品只注册 OfflineMetadataProvider，networkEnabled=false。ProviderResult 带 requestId/generation/albumId/kind/status/payload/source/checkedAt/retryAt/error；状态为 Deferred/Matched/Partial/NotFound/Uncertain/Error/Cancelled。来源与日期为缓存原值，不伪造检查时间。

OfflineMetadataProvider 对不可变本地缓存快照进行事件循环异步读取；已有介绍或制作信息及历史查询状态直接返回，即使 force 也不重查。没有缓存返回 Deferred、“联网资料功能暂未启用”，不写成 NotFound，不生成 retryAt。历史缓存中的 NotFound/error/retryAt 原样保留。使用绑定 QObject 上下文的 QTimer，取消只改变对应 token，销毁后回调不执行；语义见 [QTimer](https://doc.qt.io/qt-6/qtimer.html)。不创建网络对象、请求、子进程或写缓存。LibrarySession 用 queued connection 路由结果，检查 requestId、generation、实体和 kind，处理请求覆盖/取消/库替换/销毁，重复或过期结果丢弃。

## 本地扫描与原生缓存

`src/library/DirectoryScanner`沿冻结`safeRootList/walkAlbums/readAlbum`的16个音频扩展名集合。根直属文件各自为单曲专辑；每个有直属歌曲的非根目录为独立专辑，多CD子目录不合并；只忽略以`.`开头的子目录，隐藏歌曲文件仍可索引。遍历不跟随文件/目录符号链接，显式根为符号链接时标记离线；目录迭代/文件stat任一失败时整根回滚并保留旧专辑为offline，避免部分读取失败表现为删除。迭代器使用error_code且不启用skip_permission_denied；目录前沿在堆上保存，取消在每个目录/文件与旧索引构建处检查。

本地根最多64个，必须为无NUL的绝对本地QUrl；词法清理、重复/嵌套去重不执行GUI磁盘IO。Windows按不区分大小写比较根，其他平台按路径字符串区分；不使用canonical路径重写已有身份。专辑ID为`album-`加直属单曲文件或专辑目录的原生绝对路径UTF-8 SHA256前24位，曲目ID同理使用`track-`和文件路径；Windows哈希前还原原生分隔符。`_fingerprint`保留冻结`size:mtimeMs`语义（含文件系统实际小数毫秒），不读音频内容。相同文件及fingerprint复用元数据/未知JSON字段；变化时恢复文件名/未知艺术家、Unknown支持状态并移除旧探测版本，不按扩展名断言可播放。扫描结果继续交给下节的Qt无声元数据阶段。

文件时间先转换到system_clock，再以整秒与亚秒部分计算mtimeMs，避免把完整epoch缩窄成int64纳秒。MSVC按其[官方file_clock接口](https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class?view=msvc-170)使用clock_cast（其file_clock提供to_utc）；libc++/libstdc++使用to_sys，不假定各标准库都提供同一个可选转换函数。

`LibraryStore`在一个私有单线程池串行读取/扫描/序列化/保存；至多一个在途job、一个最后扫描意图和一个合并的已接受保存快照，不向GUI逐文件发送通知。worker建立const快照，GUI按generation接受后调用一次`LibrarySession::setSnapshot`；切根/取消淘汰旧结果。取消不撤销此前已接受快照的保存。加载中切根仍读取旧缓存作为复用基线并完成安全检查，但不显示被替代的加载结果。旧库ID/路径索引为O(专辑+曲目)，遍历为O(目录+文件)加各目录排序，根判定至多64倍线性；内存随库和当前目录/待访目录增长，不是逐帧或无限请求队列。GUI只做最终模型/排序交接；其完整产品时延由R28测量。

新dataDir下的`library-index.json`使用`nativeSchemaVersion:1`，已知值与未知JSON字段共同写回；读取/写入上限256MiB，超限报错并保留原文件。`QSaveFile`禁止direct-write fallback，临时文件仅所有者读写，提交失败不覆盖旧文件。损坏/未知schema缓存阻止自动写回，仍允许内存扫描；用户将原文件另行保留或修复后可重新load。扫描/保存失败状态通过store.error/failed暴露；无后台重试循环。退出只等待唯一worker，丢弃未接受扫描并完成已接受的最后保存，不在正常GUI事件路径等待worker或序列化。

普通产品异步加载原生缓存；`--music-root /绝对路径`可重复，替换根后扫描；`--rescan-library`扫描已存根；`--data-dir /临时数据目录`隔离缓存。未配置库时为空库。Review验证仍只读显式匿名fixture，不读真实缓存；库/搜索面板的目录选择和状态UI归R23。

## 无声元数据、封面与显式导入

`src/media/MediaMetadataProbe`拥有一个带事件循环的专用QThread。生产后端只构造并复用两个QMediaPlayer，不接QAudioOutput、不调用play；每槽以独立任务serial、整批generation和取消token处理LoadedMedia/metadata/duration/InvalidMedia及8秒异步超时。更换来源先断开旧连接、停止计时器、清空source，再排队下一任务；取消立即释放任务描述和媒体来源，退出时在所属线程销毁player。只有析构允许等待线程。游标直接遍历已去重的不可变扫描快照，不额外排入逐曲QObject/GUI回调；纯缓存路径每64条让出事件循环。LibraryStore同时只运行扫描/导入/保存或一个探测批次，`probing`与`scanning`暴露实际阶段；整批完成后核对generation，只应用一次GUI快照并保存。

公开映射为Title、AlbumTitle、AlbumArtist、ContributingArtist、TrackNumber、Date、Genre、Duration、AudioBitRate与CoverArtImage。依据所选Kit的QMediaMetaData公开头文件；[Qt元数据文档](https://doc.qt.io/qt-6/qmediametadata.html)明确CoverArtImage语义。毫秒duration转换为秒；无值不覆盖旧技术参数、制作信息、备注、来源及未知字段。`_common`只合并实际可得标签；曲目按disc/track稳定排序，专辑取首曲可得标签。流派沿冻结resolveGenre的NFKC/去空白标点符号匹配，以名称/ID/alias索引查找，保留导入人工override，未知流派按源hash建立列。

`_probeStatus`为loaded/invalid/timeout，原Qt错误保留在`_probeError`。Loaded不等于解码/设备播放已验证，因此mediaSupport保持Unknown；明确InvalidMedia/无音轨为Unsupported，超时为Unknown，不删除歌曲。Supported提升属于R24真实播放路径。`_qtProbeVersion`加扫描fingerprint复用完成的探测，文件或Qt版本变化重新探测；目录图移除后若尚无嵌入图判定，或嵌入缓存已丢失，也重新探测；超时下次显式扫描可重试。

目录图片候选按cover、folder、front、其他图片排序；根直属单曲只取同名图片。无效优先图片继续尝试下一候选；目录图片优先于嵌入图，直属单曲的嵌入图优先于同名图。ArtworkStore在worker中读取PNG/JPEG/WebP，压缩文件≤30MiB、尺寸已知且≤4,194,304像素才完整验证解码；内容SHA256决定新dataDir的`artwork/`文件名，QSaveFile按owner权限原子保存，拒绝缓存目录/文件符号链接，不改原图/歌曲。相同内容复用，内容变化产生新URL供现有租约定向失效。Qt公开CoverArtImage返回的QImage已由Qt解码，原生侧再检查像素上限、保存PNG及30MiB输出上限；原始嵌入压缩字节和Qt后端内部解码峰值公开API不可观测，不冒充已在分配前检查它们。缓存/图集/归位所有权按[covers.md](covers.md)。

`--import-legacy /绝对/music-data-v3 --data-dir /独立/原生数据`显式读取旧`library-index.json`（version:1，曲目_metadataVersion:3或4）、可选config.json/genre-rules.json，不扫描用户目录或触碰浏览器数据库。新旧目录含符号链接解析后互不嵌套。索引上限256MiB，配置/规则各1MiB；本机URL、rawGenres和封面路径提供原生值；查询配置存metadataConfiguration，人工流派引用存genreOverrides，产品始终离线。以本机绝对直属路径重建稳定ID并重映射已知制作人员/人工流派引用，不猜测跨平台路径。只复制被引用且位于允许音乐根或旧artwork内的封面，不复制PCM、整棵缓存或歌曲；坏图只记录_coverImportError并回退缺图，索引/路径/schema错误不发布快照、不改旧文件或现有缓存。导入通过LibraryStore唯一worker和一次换代提交，不逐条重建模型；下一次显式重扫再按fingerprint补齐Qt标签。

`--import-preferences /绝对/浏览器导出.json`或`PreferencesStore::importJson`只读用户明确提供的对象JSON（≤1MiB），在worker解析后合并未知字段并原子保存。导航字段类型校验，单个在途导入；期间有新的导航偏好写入时按revision拒绝旧导入。全偏好schema及设置控件仍按R26实施。导入/扫描命令互斥，普通数据根要求绝对路径；它们不与Review读取私人数据的隔离规则混用。

## 音乐库面板、搜索与歌曲定位

`panels/LibraryPanel.qml`通过LibraryStore公开接口编辑绝对路径、选择文件夹、保存扫描、重扫/取消并显示实际扫描/标签/保存/错误/离线状态；路径转换统一由C++ QUrl完成。曲库统计与已用流派只随快照发布，不按媒体或视觉帧重建。FolderDialog不触碰歌曲标签，匿名Review不提供真实数据根。

`SearchPanel.qml`保留冻结music-app/music.css的标题、搜索框、流派按钮、49px contain封面/缺图音符、结果与无结果状态。结果ListView与按实际字体宽度换行的流派行均虚拟化；大量流派可在有界区域滚动，曲库大小不扩大可见委托池。面板标题和查询区域随列表滚动，重新打开恢复查询入口。`PanelController`复用SurfaceTransition的300/200ms、10/6px及中断语义，由唯一FrameClock的motionDt驱动；Popup关闭Qt样式自身的enter/exit动画，仅由SurfaceTransition管理可见生命周期和模态焦点；退出/重开只保留最后完成动作。

面板背景复用现有可见内容，两个RGBA8逻辑像素附件执行源CSS sigma=8的离散Gaussian，线性采样配对、3 sigma截断；该滤波网格对应逻辑CSS像素，不改变三维输出或其他原生内容的DPR/画质。阴影按源0/24/90/.13的矩形Gaussian积分直接绘制，不增加捕获附件。Loader只在面板可见生命周期保留背景；ResourcePlan记录两份颜色/保守深度和resize新旧重叠，Qt/driver内部开销仍未知。关闭销毁所属捕获，不新增生产readback、mips、MSAA或独立时钟。

`TrackFocusController`验证稳定专辑/所属曲目身份，复用PresentationController的archiveRoute：必要时退字/归架，再以既有五次轨迹定位和重新打开，真正detailEnabled后才交付行号。当前打开专辑直接切歌单并滚动，不重新抽取。搜索结果和播放歌名使用同一个request接口，只读播放ID，任何定位均不调用媒体命令。最后目标覆盖未完成请求，回退/换库/重播/打开面板与用户滚轮/按下/触摸/按键/resize取消旧revealing revision；旧布局回调不能恢复高亮。

TrackList按已布局的实际歌曲行（不含CD标题）/页签尺寸居中，以原生列表实际首尾范围钳制滚动；只对有界已实现项完成polish；虚拟行实现导致originY或未显示行高度估计变化时，以实际目标行转换同一坐标基准，保持到目标的源cubic距离轨迹，避免远距离滚动跳变或目标尚未实现。滚动后按revision等待布局再聚焦；切换介绍页取消旧高亮，定位返回歌单时保留当前请求。滚动沿冻结music-track-focus的cubic in/out与min(700,320+距离×.12)ms；到达后聚焦目标行，以1000ms两次、0/.32/.48/.88/1的CSS ease-in-out高亮。减少动态立即滚动并保持高亮1100ms。该定位消费唯一FrameClock的原始dt，隐藏暂停视觉，不修改歌曲或全局动效速度。

## 检查入口

`library-data`核对冻结库列、216搜索用例、JSON/URL/模型通知、导航播放独立与Provider竞态；`library-qml`核对稳定角色及离线/测试Provider读值。`runtime-library`是匿名库的窗口/资源检查，分组见test-gates.md。源码输入由`node tools/review/export-search-golden.mjs`导出，历史薄入口verify-r04.ps1/audit-r04.ps1；`--verify-library 匿名JSON`仅配合Review运行验证，默认不读真实库。

`directory-scanner`用临时合成目录覆盖扫描/重扫/改名/删除/离线/读取拒绝、取消/切根/加载竞态、缓存原子读写/损坏保护、源hash及GUI事件循环/一次模型重置；不加载媒体或显示窗口。`node tools/review/verify-scan-fingerprint.mjs`额外用本机Node的真实stat核对原fingerprint与UTF-8哈希；原生构建/CTest不依赖Node。`python3 tools/audit/audit-library-scan.py`生成COMMON/PERF和三平台逐符号/hash矩阵至本地verification；目标环境待验归计划R21-P。

`media-metadata`用运行时匿名RIFF/INFO/ID3-PNG输入实际执行Qt无声加载、内嵌封面/同名图优先级及损坏文件；注入仅用于复用生产调度器测试公开字段、两个槽位、8秒超时、取消/晚到/复用。另核对坏图/30MiB/展开上限、Unicode、v3/v4导入、只读源、未知字段、一次模型重置、显式偏好JSON与探测中切根。`python3 tools/audit/audit-media-metadata.py`生成COMMON/PERF/平台逐符号/hash矩阵；目标欠验入口和门槛集中于迁移计划R22-P。

`track-focus`使用实际Selection/SceneAdapter/Presentation状态机验证跨歌手列的最后目标、当前专辑复用、取消/过期回调、减少动态和播放隔离；`library-data`同时核对冻结216例的缓存搜索及1024专辑连续查询无reset。`library-panels-qml`（`ui-audit --library 输出目录`）使用实际MusicStage、九尺寸/双主题/DPR1/1.5/2、4000命中虚拟列表、真实Qt按键、模态/关闭、详情滚动聚焦、背景附件释放与默认隐藏策略；自身输出用于质量检查，不做图像评分或正常呈现性能推断。`python3 tools/audit/audit-library-panels.py`绑定COMMON/PERF/三平台符号与冻结hash，目标/物理输入欠验归计划R23-P。
