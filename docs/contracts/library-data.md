# 数据模型、排序、搜索与资料查询契约

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 中的 `src/music-types.ts`、`music-order.ts`、`music-search.ts`、`data.ts:setMusicAlbums`及`scripts/music-library.mjs`。本文管理值模型、身份/排序/搜索、本地扫描/存储序列化与Provider协议；媒体与UI消费者按各自专项契约接入。

`src/core/MusicTypes.h` 中的 Album/Track/Producer/Genre/LibraryRoot/LibrarySnapshot 是值类型。专辑、曲目、制作人员、来源、介绍/制作查询、MusicBrainz、扫描与批量进度字段均保留；可选技术参数不猜测。各级 `fields` 保留未知扩展与当前元数据，写回时覆盖已知字段。已由值模型持有的子数组不再重复存入扩展对象。`readLibrary` 使用临时值解析，重复/错配 ID、非法集合或已知字段类型、非整数或超int范围的可选整数、非数值/负数/非有限时长返回错误，原快照不变；缺失/null可选参数保持未知。本地file URL须有效、绝对且没有query/fragment/NUL，非法来源保持空URL，不猜测为可播放文件。`Snapshot` 是共享的 const 快照；`LibrarySession::setSnapshot` 仅在 GUI 线程应用一次模型重置。

歌曲 `sourceUrl`、目录 `folder` 和 roots.path 使用本地 QUrl；绝对 Unicode 原生路径用 `QUrl::fromLocalFile`，不从当前目录猜相对音乐路径。歌曲 URL 只接受本地文件；封面额外允许 qrc。来源/候选链接保留文本。显式目录导入见下节。`MediaSupport` 为 Unknown/Supported/Unsupported，只有实际媒体状态才能更新；无声标签加载不证明可解码播放，不按扩展名猜测。

排序使用 zh_CN QCollator、numericMode、CaseInsensitive；文本 NFKC、合并空白、trim、lowercase。相等时按规范化字符串、ID 稳定排序。流派模式保持快照顺序，列顺序为已使用的配置流派、再按首次出现补充未配置流派；列用流派 ID 分组，重名不合并。歌手模式用规范化 key 分组，显示原始歌手经空白整理的名称，空歌手为“未知歌手”。专辑模式每列 12 张，标签为 01–12 等；slotStride=max(32, 最大列长+12)。QCollator 平台后端可能与浏览器 ICU 排列细节不同；视觉 fixture 的既有列顺序仍以 `library-order.json` 固定。API 依据：[QCollator](https://doc.qt.io/qt-6/qcollator.html)。

排序时每张专辑的标题/歌手规范化键和QCollatorSortKey各预计算一次，比较器只比较缓存键、规范化字符串及ID；列位置用key→index索引查找，避免不同歌手/流派很多时反复扫描已建立的列。索引仅用于查找，列顺序仍由原规则确定，不迭代QHash来决定展示顺序。算法与成本审计按 [performance.md](performance.md) 执行。

LibrarySession在快照替换时缓存已使用流派列表。搜索筛选由GenreRowsModel在C++测量和分行，字体/字体库变化才重新测量，宽度变化只重新分行；行通过QAbstractListModel供QML ListView虚拟化，显示顺序与22px内距/7px间距保持原规则。模型内容与测量/分行均为O(流派数量)，不随视觉帧运行。全部选项和空库仍保留“全部”。

SearchListModel按不可变快照建立专辑ID→索引、每专辑曲目ID→索引及搜索字符串/流派名缓存；同一快照改变query只归一化查询，复用全部条目。交互查询由单个worker筛选隐式共享的只读条目，最多一个在途任务与一个最新查询；新输入/换库按generation取消旧扫描，GUI仅接纳当前结果，筛选中禁止激活旧查询结果，退出先取消再等待worker。重复的归一化查询复用当前结果。查询结果保留相同前后缀，中间重叠行以单批dataChanged复用，仅数量差触发一次区间删除或插入，不按键重置模型；每次数据/行通知后核对generation，观察者同步换库或发起新查询时停止旧结果余下的修改，count按实际已发布值去重通知；新查询清除旧键盘选择行号；委托按下时保存专辑/曲目身份，释放时身份已变化则取消定位；快照更换/清空时事务重置并重建/清空索引。role与resultAt按稳定ID索引读取，曲目定位仍在所属专辑内，不假设跨专辑曲目ID全局唯一。内存为O(专辑+曲目+搜索文本)，非空查询为O(总搜索文本+结果)，空查询通过专辑边界索引跳过曲目，成本为O(专辑数量)；流派过滤同样整段跳过被排除专辑的曲目，非空筛选仅访问专辑头及被接纳流派的搜索文本，不改变匹配和返回顺序。

搜索逐式保留 `music-search.ts`：仅 trim 和 lowercase，匹配专辑标题/歌手/流派显示名（缺失则“未分类”）与曲目标题/歌手；匹配专辑后仍逐曲匹配，空查询只返回专辑，流派过滤按 ID。不匹配备注、制作信息或原始流派数组，也不加入排序的 NFKC/空白归并。`tools/migration/export-search-golden.mjs` 从 Git 冻结源码去掉类型声明后执行原函数，生成 216 个用例与源码/fixture hash；Node 仅用于可选的 Web 验证数据导出，原生运行、CMake 和 CTest 不依赖它。

模型角色在 `LibraryModels.h` 的枚举及 `roleNames()` 固定，新角色只能追加。AlbumListModel 提供 albumId/title/artist/year/genreId/coverUrl/folder/tracks/producers/description/descriptionSource/introduction/creditsLookup/online/localNote/offline/albumData，另有 count/columns/slotStride。Tracks 中保留每首稳定 ID、源 QUrl 和 mediaSupport。SearchListModel 提供 kind/albumId/trackId/title/artist/count。QML 类型通过 `src/app/QmlTypes.h` 的 foreign 类型注册，不允许 QML 自行创建或更换 Provider。

LibrarySession 的 navigationAlbumId/navigationTrackId 与 playbackAlbumId/playbackTrackId 独立。`navigate`、`search`、`sort`、`requestMetadata` 不调用播放或修改播放 ID；播放ID只由受控C++入口同步：通用setPlaybackIds验证当前库，PlaybackController可为已验证并保留的旧队列同步孤立ID；Qt媒体状态与设备边界见[playback.md](playback.md)。库替换使过期导航失效，播放 ID 保留以便后续正在播放的媒体独立存活。

## 联网资料与离线缓存

IMetadataProvider构造注入，提供Introduction/Credits/Enrichment/cancel异步接口。三类资料的公开名称、缓存字段和Provider能力由同一描述表映射；设置页和Provider复用`cachedMetadata`解析已存状态、内容及来源，状态统一为ProviderStatus的展示名。AlbumQuery携带专辑值、本地曲目用于匹配、enabled/force/configuration；本地路径与曲目文件不发送。请求bool表示是否接纳，拒绝时不建立订阅。ProviderResult携带requestId/generation/albumId/kind/status/payload/source/checkedAt/retryAt/error及输入摘要；状态为Deferred/Matched/Partial/NotFound/Uncertain/Error/Cancelled。LibrarySession在结果进入缓存前检查请求、代次、实体和kind；取消、换库、扫描、视觉重载及销毁淘汰过期结果。导航与播放始终独立。

普通产品注入OnlineMetadataProvider，演示和验证入口使用OfflineMetadataProvider。`metadataConfiguration.networkEnabled`默认false；用户在“高级 · 联网资料”显式开启后才能发送查询。`onlineEnabled`独立控制扫描后对尚无MusicBrainz查询状态的专辑自动补充结构化资料，初次加载缓存不自动查询。手动当前专辑按钮刷新该类资料；全库按钮串行补缺或刷新过期项，不强制重查有效缓存。原MusicBrainz联系字段可选，空值使用项目公开网址标识User-Agent；`proxyEnabled`默认true，关闭时明确直连；`proxyMode`默认system，区分系统默认代理与manual手动代理，手填地址在`proxyUrl`独立保存。手动代理必须为不含认证信息的有效HTTP地址；空或无效手填地址不能静默回退系统代理。配置修改仅更新配置并取消在途请求，保留已接受资料、曲目模型、导航与播放，不作为换库。所有配置通过LibraryStore保存，验证字段类型、联系方式长度/换行及代理结构，保留未知扩展。忙碌/损坏库拒绝配置写入。

OnlineMetadataProvider的专用QThread拥有调度器、QNetworkAccessManager、响应缓冲和JSON解析。GUI只交接隐式共享的值；包括等待、在途、取消确认和完成邮箱在内最多八个已投递请求，同一时刻一个HTTP请求。取消立即停止worker中对应timer/reply，GUI在worker确认释放前仍计入容量，避免反复取消造成无限queued closure。析构仅在退出路径等待线程；QObject/timer/reply在所属线程清理。

HTTP仅允许`musicbrainz.org`、`zh.wikipedia.org`、`en.wikipedia.org`与`www.wikidata.org`的HTTPS；不跟随重定向、不发送/保存cookies或复用认证，不忽略TLS错误。MusicBrainz及Wikimedia各自请求起点至少间隔1100ms；使用对象所属单次timer唤醒，不轮询。单响应解压后最多2MiB，有12秒传输与15秒总时限。429/503/Retry-After及Wikimedia maxlag触发服务冷却，保留retryAt；没有自动重试，force也不能绕过冷却。源错误、响应格式变化、缺资料和匹配歧义分别报告，不能以断网写成NotFound。依据：[MusicBrainz API](https://musicbrainz.org/doc/MusicBrainz_API)、[Wikimedia API etiquette](https://www.mediawiki.org/wiki/API:Etiquette)、[Qt Network](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)。

- **MusicBrainz**：已知release ID先验证格式再查询并复核专辑/歌手/可用年份及完整曲目；缺ID时搜索至多十个发行版本，仅完整搜索范围内唯一符合标题、歌手、曲目总数和可用年份的高分候选自动进入详情；截断搜索或多版本返回Uncertain并缓存至多十个候选的日期/地区/厂牌/标识，由当前专辑的版本选择控件明确确认后再查询，不能静默取首项。逐曲按标题、已知disc/track及实际可用时长（差≤3秒）唯一匹配，不能把不同发行版本或live/remix混用。匹配先验证远端曲目总数并预留容量，每个本地/远端标题只规范化一次；按规范化标题索引候选，保留原顺序及已使用记录、disc/track/时长约束，遇第二个合法候选立即判歧义。制作信息读取release/recording及所关联work的艺术家关系，保留作词、作曲、编曲、演奏、制作、混音/母带等具体职责、属性、来源和稳定trackId；署名别名按关系方向读取，空别名回退人员姓名；有资料仍返回Partial，不将“有若干署名”等同完整。每次最多2000条远端曲目、4096条署名和128个流派。Enrichment保存发行标识及在线流派，保持本地名称、标签、文件与人工genreId。资料模型与关系依据：[MusicBrainz relationships](https://musicbrainz.org/doc/Relationships)。
- **Wikipedia/Wikidata**：按中文→英文查询明确标题及至多五个搜索候选；检索片段不作为介绍。使用Action API的TextExtracts公开纯文本导言（每页最多1200字符），同时获取pageprops和revision；该接口可批量绑定导言与实体，第三方客户端不依赖HTML解析或浏览器。Wikidata每批至多50个实体，核对标题别名、P175歌手与P577日期；无本地年份时须有结构化歌手证据。缺结构化证据时，仅允许标题匹配且正文同时证实歌手与发行年份；已知结构化矛盾、缺失歌手实体、消歧页或多个符合实体不能被正文“救回”。没有足够证据返回Uncertain。保存正文、来源页链接、revision ID、查询时间、明确的CC BY-SA 4.0与许可链接。依据：[Wikidata data access](https://www.wikidata.org/wiki/Wikidata:Data_access)、[TextExtracts](https://www.mediawiki.org/wiki/Extension:TextExtracts)、[Wikimedia复用要求](https://foundation.wikimedia.org/wiki/Terms_of_Use)。

缓存输入摘要覆盖查询种类、专辑名/歌手/年份及本地曲目身份、名称、时长、disc/track；输入变化使相应缓存失效。成功/部分资料有效30天，NotFound/Uncertain有效一天，错误按retryAt（默认60秒）保留，连接配置变更可重新尝试普通连接错误，但不能绕过服务限流冷却；未知字段和有效历史来源保留，不伪造checkedAt。关闭联网仍可读取过期缓存。失败刷新只更新查询状态，保留已有正文/制作人员；成功制作查询在creditsLookup保存已核对的发行标识并清除旧候选；制作与流派查询状态各自保存，后续查询采用较新的已核对标识，避免不同查询种类互相覆盖；只替换MusicBrainz来源，保留本地及其他来源。OfflineMetadataProvider以对象所属FIFO timer异步读取快照，最多八项，不创建网络对象或写缓存；无缓存返回Deferred，已有历史状态原样交付。

LibrarySession以稳定ID串行推进批次，metadataProgress显示真实状态与完成/错误数；取消保留已经接受的结果。metadataRevision驱动当前实体读取，QML不解析JSON。当前专辑的元数据通过DetailModel刷新，保持曲目模型、滚动与播放；模型metadata提交使用定向dataChanged，不触发排序、导航或整个场景重建。来源、许可和查询错误可离线查看。

LibraryStore按实体/kind合并已接受结果；批次结束、取消或累计载荷达到8MiB时，在其唯一worker中按索引合并进最新快照。批次先按专辑ID索引，只为命中的专辑验证输入摘要和构造候选；整批被拒绝时保留原快照身份，不分离专辑数组、不发送提交通知或保存全库。仅修改资料字段，不写音乐文件、不覆盖人工分类。元数据快照与保存共用既有有界串行调度和QSaveFile/256MiB缓存门槛；GUI不逐结果复制/序列化全库。单次合并最坏O(专辑数+被修改专辑曲目/资料数)，暂存最多当前库每专辑三类结果，HTTP等待/在途/完成容量独立有界。元数据提交不作为换库，不中断批次。保存失败保留原文件并暴露错误，退出处理已接纳结果；内存结果与磁盘保存成功是独立事实。

## 本地扫描与原生缓存

`src/library/DirectoryScanner`沿冻结`safeRootList/walkAlbums/readAlbum`的16个音频扩展名集合。根直属文件各自为单曲专辑；每个有直属歌曲的非根目录为独立专辑，多CD子目录不合并；只忽略以`.`开头的子目录，隐藏歌曲文件仍可索引。遍历不跟随文件/目录符号链接，显式根为符号链接时标记离线；目录迭代/文件stat任一失败时整根回滚并保留旧专辑为offline，避免部分读取失败表现为删除。迭代器使用error_code且不启用skip_permission_denied；目录前沿在堆上保存，取消在每个目录/文件与旧索引构建处检查。

本地根最多64个，必须为无NUL的绝对本地QUrl；词法清理、重复/嵌套去重不执行GUI磁盘IO。Windows按不区分大小写比较根，其他平台按路径字符串区分；不使用canonical路径重写已有身份。专辑ID为`album-`加直属单曲文件或专辑目录的原生绝对路径UTF-8 SHA256前24位，曲目ID同理使用`track-`和文件路径；Windows哈希前还原原生分隔符。`_fingerprint`保留冻结`size:mtimeMs`语义（含文件系统实际小数毫秒），不读音频内容。相同文件及fingerprint复用元数据/未知JSON字段；变化时恢复文件名/未知艺术家、Unknown支持状态并移除旧探测版本，不按扩展名断言可播放。扫描结果继续交给下节的Qt无声元数据阶段。

文件时间先转换到system_clock，再以整秒与亚秒部分计算mtimeMs，避免把完整epoch缩窄成int64纳秒。MSVC按其[官方file_clock接口](https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class?view=msvc-170)使用clock_cast（其file_clock提供to_utc）；libc++/libstdc++使用to_sys，不假定各标准库都提供同一个可选转换函数。

`LibraryStore`在一个私有单线程池串行读取/扫描/序列化/保存；至多一个在途job、一个最后扫描意图和一个合并的已接受保存快照，不向GUI逐文件发送通知。worker建立const快照，GUI按generation接受后调用一次`LibrarySession::setSnapshot`；切根/取消淘汰旧结果。取消不撤销此前已接受快照的保存。加载中切根仍读取旧缓存作为复用基线并完成安全检查，但不显示被替代的加载结果。旧库ID/路径索引为O(专辑+曲目)，遍历为O(目录+文件)加各目录排序，根判定至多64倍线性；内存随库和当前目录/待访目录增长，不是逐帧或无限请求队列。GUI只做最终模型/排序交接；其完整产品时延由R28测量。

新dataDir下的`library-index.json`使用`nativeSchemaVersion:1`，已知值与未知JSON字段共同写回；读取/写入上限256MiB，超限报错并保留原文件。`QSaveFile`禁止direct-write fallback，临时文件仅所有者读写，提交失败不覆盖旧文件。损坏/未知schema缓存阻止自动写回，仍允许内存扫描；用户将原文件另行保留或修复后可重新load。扫描/保存失败状态通过store.error/failed暴露；无后台重试循环。退出只等待唯一worker，丢弃未接受扫描并完成已接受的最后保存，不在正常GUI事件路径等待worker或序列化。

普通产品异步加载原生缓存；`--music-root /绝对路径`可重复，替换根后扫描；`--rescan-library`扫描已存根；`--data-dir /临时数据目录`隔离缓存。未配置库时为空库。Review验证仍只读显式匿名fixture，不读真实缓存；库/搜索面板的目录选择和状态UI见下文“音乐库面板、搜索与歌曲定位”。

## 无声元数据、封面与显式导入

`src/media/MediaMetadataProbe`拥有一个带事件循环的专用QThread。生产后端只构造并复用两个QMediaPlayer，不接QAudioOutput、不调用play；每槽以独立任务serial、整批generation和取消token处理LoadedMedia/metadata/duration/InvalidMedia及8秒异步超时。更换来源先断开旧连接、停止计时器、清空source，再排队下一任务；取消立即释放任务描述和媒体来源，退出时在所属线程销毁player。只有析构允许等待线程。游标直接遍历已去重的不可变扫描快照，不额外排入逐曲QObject/GUI回调；纯缓存路径每64条让出事件循环。LibraryStore同时只运行扫描/导入/保存或一个探测批次，`probing`与`scanning`暴露实际阶段；整批完成后核对generation，只应用一次GUI快照并保存。

公开映射为Title、AlbumTitle、AlbumArtist、ContributingArtist、TrackNumber、Date、Genre、Duration、AudioBitRate与CoverArtImage。依据所选Kit的QMediaMetaData公开头文件；[Qt元数据文档](https://doc.qt.io/qt-6/qmediametadata.html)明确CoverArtImage语义。毫秒duration转换为秒；无值不覆盖旧技术参数、制作信息、备注、来源及未知字段。`_common`只合并实际可得标签；曲目按disc/track稳定排序，专辑取首曲可得标签。流派沿冻结resolveGenre的NFKC/去空白标点符号匹配，以名称/ID/alias索引查找，保留导入人工override，未知流派按源hash建立列。

`_probeStatus`为loaded/invalid/timeout，原Qt错误保留在`_probeError`。Loaded不等于解码/设备播放已验证，因此mediaSupport保持Unknown；明确InvalidMedia/无音轨为Unsupported，超时为Unknown，不删除歌曲。Supported提升属于R24真实播放路径。`_qtProbeVersion`加扫描fingerprint复用完成的探测，文件或Qt版本变化重新探测；目录图移除后若尚无嵌入图判定，或嵌入缓存已丢失，也重新探测；超时下次显式扫描可重试。

目录路径的QCollatorSortKey和图片优先级在比较前逐项生成；曲目以已排序的路径为初始顺序，再按disc/track稳定排序，保留自然路径平局规则。目录图片候选按cover、folder、front、其他图片排序；根直属单曲只取同名图片。无效优先图片继续尝试下一候选；目录图片优先于嵌入图，直属单曲的嵌入图优先于同名图。ArtworkStore在worker中读取PNG/JPEG/WebP，压缩文件≤30MiB、尺寸有效，再由Qt完整验证解码；相同压缩内容与有效Qt分配上限的验证成功结果按worker线程保留至多128项LRU，重扫仍读取实际字节并计算SHA256，命中只省去重复完整解码，不缓存失败或绕过格式、符号链接与原子保存检查；图片分配准入唯一见[covers.md](covers.md)。内容SHA256决定新dataDir的`artwork/`文件名，QSaveFile按owner权限原子保存，拒绝缓存目录/文件符号链接，不改原图/歌曲。相同内容复用，内容变化产生新URL供现有租约定向失效。Qt公开CoverArtImage返回的QImage已由Qt解码，原生侧在编码前按同一有效分配上限核对现有载荷（至少32bpp），保存PNG并检查30MiB输出上限；原始嵌入压缩字节和Qt后端内部解码峰值公开API不可观测，不冒充已在分配前检查它们。缓存/图集/归位所有权按封面契约。

`--import-legacy /绝对/music-data-v3 --data-dir /独立/原生数据`显式读取旧`library-index.json`（version:1，曲目_metadataVersion:3或4）、可选config.json/genre-rules.json，不扫描用户目录或触碰浏览器数据库。新旧目录含符号链接解析后互不嵌套。索引上限256MiB，配置/规则各1MiB；本机URL、rawGenres和封面路径提供原生值；查询配置存metadataConfiguration，人工流派引用存genreOverrides；联网授权仅由上述networkEnabled控制。以本机绝对直属路径重建稳定ID并重映射已知制作人员/人工流派引用，不猜测跨平台路径。只复制被引用且位于允许音乐根或旧artwork内的封面，不复制PCM、整棵缓存或歌曲；坏图只记录_coverImportError并回退缺图，索引/路径/schema错误不发布快照、不改旧文件或现有缓存。导入通过LibraryStore唯一worker和一次换代提交，不逐条重建模型；下一次显式重扫再按fingerprint补齐Qt标签。

`--import-preferences /绝对/浏览器导出.json`或`PreferencesStore::importJson`只读用户明确提供的对象JSON（≤1MiB），在worker解析后合并未知字段并原子保存。导航字段类型校验，单个在途导入；期间有新的导航偏好写入时按revision拒绝旧导入。全偏好schema及设置控件见下节。导入/扫描命令互斥，普通数据根要求绝对路径；它们不与Review读取私人数据的隔离规则混用。

## 音乐库面板、搜索与歌曲定位

自动创建的未分类流派名称与缺失歌手以空值保存在领域数据中，不把翻译后的占位文案当成元数据或状态标记。列表、浏览信息与详情通过genreDisplayName/artistDisplayName显示本地化提示；真实标签即使恰好与提示同名也保持原值，Provider查询仍接收原始数据。

`panels/LibraryPanel.qml`通过LibraryStore公开接口编辑绝对路径、选择文件夹、保存扫描、重扫/取消并显示实际扫描/标签/保存/错误/离线状态；路径转换统一由C++ QUrl完成。曲库统计与已用流派只随快照发布，不按媒体或视觉帧重建。FolderDialog不触碰歌曲标签，匿名Review不提供真实数据根。

`SearchPanel.qml`保留冻结music-app/music.css的标题、搜索框、流派按钮、49px contain封面/缺图音符、结果与无结果状态。结果ListView与按实际字体宽度换行的流派行均虚拟化；大量流派可在有界区域滚动，曲库大小不扩大可见委托池。面板标题和查询区域随列表滚动，重新打开恢复查询入口。`PanelController`复用SurfaceTransition的300/200ms、10/6px及中断语义，由唯一FrameClock的motionDt驱动；Popup关闭Qt样式自身的enter/exit动画，仅由SurfaceTransition管理可见生命周期和模态焦点；退出/重开只保留最后完成动作。

面板背景复用现有可见内容，两个RGBA8逻辑像素附件执行源CSS sigma=8的离散Gaussian，线性采样配对、3 sigma截断；该滤波网格对应逻辑CSS像素，不改变三维输出或其他原生内容的DPR/画质。阴影按源0/24/90/.13的矩形Gaussian积分直接绘制，不增加捕获附件。Loader只在面板可见生命周期保留背景；ResourcePlan记录两份颜色/保守深度和resize新旧重叠，Qt/driver内部开销仍未知。关闭销毁所属捕获，不新增生产readback、mips、MSAA或独立时钟。

`TrackFocusController`验证稳定专辑/所属曲目身份，复用PresentationController的archiveRoute：必要时退字/归架，再以既有五次轨迹定位和重新打开，真正detailEnabled后才交付行号。当前打开专辑直接切歌单并滚动，不重新抽取。搜索结果和播放歌名使用同一个request接口，只读播放ID，任何定位均不调用媒体命令。最后目标覆盖未完成请求，回退/换库/重播/打开面板与用户滚轮/按下/触摸/按键/resize取消旧revealing revision；旧布局回调不能恢复高亮。

TrackList只采集原生列表实际几何并执行视图操作；TrackFocusController按已布局的歌曲行（不含CD标题）/页签尺寸居中，以实际首尾范围钳制滚动，持有origin修正与目标偏移状态；只对有界已实现项完成polish；虚拟行实现导致originY或未显示行高度估计变化时，以实际目标行转换同一坐标基准，保持到目标的源cubic距离轨迹，避免远距离滚动跳变或目标尚未实现。滚动后按revision等待布局再聚焦；切换介绍页取消旧高亮，定位返回歌单时保留当前请求。滚动沿冻结music-track-focus的cubic in/out与min(700,320+距离×.12)ms；到达后聚焦目标行，以1000ms两次、0/.32/.48/.88/1的CSS ease-in-out高亮。减少动态立即滚动并保持高亮1100ms。该定位消费唯一FrameClock的原始dt，隐藏暂停视觉，不修改歌曲或全局动效速度。

`ui-tests --library 输出目录 --representative`使用桌面/窄屏、DPR1/2与双主题的既有代表组合，保留该范围内的实际输入、焦点及资料状态断言；省略选项保持完整库面板检查范围。设置入口同样支持`--representative`。代表检查不冒称其余矩阵或物理设备事实。

## 设置与偏好

SettingsPanel沿冻结music-app的renderSettingsPanel/music.css：两列主题卡、排列、介绍/制作信息、阵列与列记忆、景深设置、减少动态/速度、全屏/开场重播/界面重载、三路声音和折叠MusicBrainz配置。面板沿用LibraryPanels的几何、模态输入、唯一SurfaceTransition与背景资源；来源布局与当前适配判定见source-fidelity-review.md。歌曲、BGM、音效及设备接口只由playback.md维护，画质字段及归一化只由occlusion.md维护。

SettingsController::updateQuality在C++合并单字段与当前完整画质值，再交RenderQuality归一化；未知字段拒绝，不重置其他选择。SettingsController按字段路由至既有LibrarySession、SelectionController、MotionController/FrameClock、SelectionLighting和RenderQuality。主题只监听目标nightChanged，动画渐变不保存偏好；速度不改变开场或歌曲速度。排列显式更新列模型，theme/speed不重建库/媒体/材质/封面。App的reloadRequested关闭并完成面板退出，销毁/重建MusicStage视觉树；原生控制器、正在播放对象、音源/意图、数据、用户画质和会话列记忆继续存活。重载先取消资料请求generation，QML异步结果不能回写旧视图。

原生preferences.json的schemaVersion为1；无版本对象在显式导入或读取时归入1，其他版本/坏JSON/非法已知字段保留原文件并阻止自动覆盖。theme、sortMode、arrayMode、rememberColumnPosition、singleInstance、reduced、motionSpeed、volume/bgm/bgmVolume/sound/soundVolume和renderQuality保存当前选择；renderQuality仅保存艺术效果参数，原生尺寸与固定生产采样由occlusion.md维护。已知字段校验类型、离散选项及音量/速度范围；画质数值交唯一RenderQuality归一化，不改变其未知扩展。未知根字段与对象扩展在更新/导入/保存中保留。会话各列位置仍只在ColumnMemory，不写偏好JSON。

`singleInstance`为布尔值，默认开启，设置面板可修改，重启后生效。普通产品在启动资料Provider、声音、曲库或创建窗口前，以规范化数据目录的QLockFile和仅当前用户可访问的QLocalServer仲裁；相同目录再次启动只请求已有窗口显示/恢复与激活，确认后退出，不转发导入/扫描参数。不同数据目录相互独立；关闭此模式允许多开。持锁进程不会因运行时间过长被驱逐；崩溃锁按Qt的死进程判定恢复，锁/端点/响应失败明确报错，不能降级多开。演示与验证入口不读取用户此项设置、不参与产品实例仲裁。启动阶段的有界IPC等待不进入常规交互路径。`single-instance`使用匿名临时目录和真实子进程验证仲裁/激活/隔离/正常退出/崩溃恢复；设置校验和持久化由`settings`覆盖。

PreferencesStore在事件循环合并变化，只有一个在途worker保存和一个最新意图；JSON序列化与QSaveFile提交在worker，禁止direct-write fallback，文件仅owner读写。显式导入最多1MiB、单个在途，按revision拒绝期间发生设置变化后的旧结果；原文件不写。损坏/未知schema使用内存默认值并暴露错误，写失败保留旧文件、无自动重试循环；析构只完成已接受的最后保存。

介绍/制作信息/MusicBrainz区域的来源、当前查询、全库补缺、取消、网络开关及配置行为遵守上节“联网资料与离线缓存”，不在UI另建状态或请求机制。

## 检查入口

`library-data`核对冻结库列、216搜索用例、JSON/URL/模型通知、导航播放独立与Provider竞态；`library-qml`核对稳定角色及离线/测试Provider读值。生产资料面板接线由`library-panels-qml`验证，窗口/资源由生产根与部署检查覆盖。源码输入由`node tools/migration/export-search-golden.mjs`导出。

`directory-scanner`用临时合成目录覆盖扫描/重扫/改名/删除/离线/读取拒绝、取消/切根/加载竞态、缓存原子读写/损坏保护、源hash及GUI事件循环/一次模型重置；不加载媒体或显示窗口。`node tools/migration/verify-scan-fingerprint.mjs`额外用本机Node的真实stat核对原fingerprint与UTF-8哈希；原生构建/CTest不依赖Node。

`media-metadata`用运行时匿名RIFF/INFO/ID3-PNG输入实际执行Qt无声加载、内嵌封面/同名图优先级及损坏文件；注入仅用于复用生产调度器测试公开字段、两个槽位、8秒超时、取消/晚到/复用。另核对坏图/30MiB/展开上限、Unicode、v3/v4导入、只读源、未知字段、一次模型重置、显式偏好JSON与探测中切根。

`track-focus`使用实际Selection/SceneAdapter/Presentation状态机验证跨歌手列的最后目标、当前专辑复用、取消/过期回调、减少动态和播放隔离；`library-data`同时核对冻结216例的缓存搜索及1024专辑连续查询无reset。`library-panels-qml`（`ui-tests --library 输出目录`）使用实际MusicStage、九尺寸/双主题/DPR1/1.5/2、4000命中虚拟列表、真实Qt按键、模态/关闭、详情滚动聚焦、背景附件释放与默认隐藏策略；自身输出用于质量检查，不做图像评分或正常呈现性能推断。

`settings`检查完整偏好持久化/导入/坏文件/未知schema、字段路由、原子合并保存、配置未知字段与重载状态保留；`library-data`覆盖单请求及批次取消/不同代订阅者晚到结果。`settings-qml`（`ui-tests --settings 输出目录`）在实际MusicStage上覆盖九尺寸、双主题、DPR1/1.5/2、折叠/滚动/Qt输入/反向打开、离线按钮及测试Provider状态/能力，另在真实App上十次重建视觉树并核对播放源/意图/用户画质。macOS默认Metal真正隐藏，其他目标入口按test-gates.md/window契约执行；截图只检查自身正常输出，无图像评分或屏幕/VSYNC推断。

`online-metadata`使用合成音乐资料与注入的QNetworkAccessManager响应，验证真实worker/HTTP调度、保守匹配、职责/曲目映射、来源/许可、缓存、取消确认容量、限流冷却、超大/坏响应、离线复用及原子持久化和详情增量更新，不访问真实网络。`metadata-network-probe`仅用于显式公开专辑联网验证，不注册为常规CTest、不读用户曲库：`build/release/tools/metadata-network-probe --kind introduction --title 'OK Computer' --artist Radiohead --year 1997 [--proxy http://127.0.0.1:7897]`；`--kind credits|enrichment`可配`--release`指定公开MusicBrainz发行ID，`--tracks`读取显式公开测试曲目数组（title/duration/disc/number），不接受歌曲文件作为输入。成功/部分资料退出0，查询未匹配/错误退出2，总超时退出4。目标系统的Qt Network/TLS/代理实际运行仍须对应环境证明。
