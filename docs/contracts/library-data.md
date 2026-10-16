# 数据模型、排序、搜索与离线资料契约

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 中的 `src/music-types.ts`、`music-order.ts`、`music-search.ts`、`data.ts:setMusicAlbums`及`scripts/music-library.mjs`。本文管理值模型、身份/排序/搜索、本地扫描/存储序列化与Provider协议；媒体/UI消费者按计划对应轮次接入。

`src/core/MusicTypes.h` 中的 Album/Track/Producer/Genre/LibraryRoot/LibrarySnapshot 是值类型。专辑、曲目、制作人员、来源、介绍/制作查询、MusicBrainz、扫描与批量进度字段均保留；可选技术参数不猜测。各级 `fields` 保留原 JSON，写回时覆盖已知字段，其余未知字段不丢失。`readLibrary` 使用临时值解析，重复/错配 ID 或非法集合返回错误，原快照不变。`Snapshot` 是共享的 const 快照；`LibrarySession::setSnapshot` 仅在 GUI 线程应用一次模型重置。

歌曲 `sourceUrl`、目录 `folder` 和 roots.path 使用本地 QUrl；绝对 Unicode 原生路径用 `QUrl::fromLocalFile`，不从当前目录猜相对音乐路径。歌曲 URL 只接受本地文件；封面额外允许 qrc。来源/候选链接保留文本。旧 browserPlayable、audioUrl、decodedAudioUrl 与 localDecodable 仅作为原始 JSON 字段留存，不转换为播放能力或原生媒体来源；实际旧目录导入归 R22。`MediaSupport` 为 Unknown/Supported/Unsupported，只有后续真实媒体探测才能更新；不按扩展名猜测。

排序使用 zh_CN QCollator、numericMode、CaseInsensitive；文本 NFKC、合并空白、trim、lowercase。相等时按规范化字符串、ID 稳定排序。流派模式保持快照顺序，列顺序为已使用的配置流派、再按首次出现补充未配置流派；列用流派 ID 分组，重名不合并。歌手模式用规范化 key 分组，显示原始歌手经空白整理的名称，空歌手为“未知歌手”。专辑模式每列 12 张，标签为 01–12 等；slotStride=max(32, 最大列长+12)。QCollator 平台后端可能与浏览器 ICU 排列细节不同；视觉 fixture 的既有列顺序仍以 `library-order.json` 固定。API 依据：[QCollator](https://doc.qt.io/qt-6/qcollator.html)。

排序时每张专辑的标题/歌手规范化键预计算一次，比较器保留相同QCollator、规范化字符串及ID的顺序；列位置用key→index索引查找，避免不同歌手/流派很多时反复扫描已建立的列。索引仅用于查找，列顺序仍由原规则确定，不迭代QHash来决定展示顺序。算法与成本审计按 [performance.md](performance.md) 执行。

SearchListModel按不可变快照建立专辑ID→索引、每专辑曲目ID→索引；同一快照改变query复用索引，快照更换/清空立即重建或清空。role读取不再逐次扫描全库两遍，曲目定位仍在所属专辑内，不假设跨专辑曲目ID全局唯一；索引内存为O(专辑+曲目)，不改变搜索命中或返回顺序。

搜索逐式保留 `music-search.ts`：仅 trim 和 lowercase，匹配专辑标题/歌手/流派显示名（缺失则“未分类”）与曲目标题/歌手；匹配专辑后仍逐曲匹配，空查询只返回专辑，流派过滤按 ID。不匹配备注、制作信息或原始流派数组，也不加入排序的 NFKC/空白归并。`tools/review/export-search-golden.mjs` 从 Git 冻结源码去掉类型声明后执行原函数，生成 216 个用例与源码/fixture hash；Node 仅用于可选的 Web 验证数据导出，原生运行、CMake 和 CTest 不依赖它。

模型角色在 `LibraryModels.h` 的枚举及 `roleNames()` 固定，新角色只能追加。AlbumListModel 提供 albumId/title/artist/year/genreId/coverUrl/folder/tracks/producers/description/descriptionSource/introduction/creditsLookup/online/localNote/offline/albumData，另有 count/columns/slotStride。Tracks 中保留每首稳定 ID、源 QUrl 和 mediaSupport。SearchListModel 提供 kind/albumId/trackId/title/artist/count。QML 类型通过 `src/app/QmlTypes.h` 的 foreign 类型注册，不允许 QML 自行创建或更换 Provider。

LibrarySession 的 navigationAlbumId/navigationTrackId 与 playbackAlbumId/playbackTrackId 独立。`navigate`、`search`、`sort`、`requestMetadata` 不调用播放或修改播放 ID；播放 ID 只能经 C++ 的 `setPlaybackIds` 明确同步。本阶段没有实现播放引擎。库替换使过期导航失效，播放 ID 保留以便后续正在播放的媒体独立存活。

IMetadataProvider 构造注入，固定 Introduction/Credits/Enrichment/cancel 异步接口。AlbumQuery 预留批量 ID、联系人、enabled/force/configuration；ProviderCapabilities 预留各类能力，产品只注册 OfflineMetadataProvider，networkEnabled=false。ProviderResult 带 requestId/generation/albumId/kind/status/payload/source/checkedAt/retryAt/error；状态为 Deferred/Matched/Partial/NotFound/Uncertain/Error/Cancelled。来源与日期为缓存原值，不伪造检查时间。

OfflineMetadataProvider 对不可变本地缓存快照进行事件循环异步读取；已有介绍或制作信息及历史查询状态直接返回，即使 force 也不重查。没有缓存返回 Deferred、“联网资料功能暂未启用”，不写成 NotFound，不生成 retryAt。历史缓存中的 NotFound/error/retryAt 原样保留。使用绑定 QObject 上下文的 QTimer，取消只改变对应 token，销毁后回调不执行；语义见 [QTimer](https://doc.qt.io/qt-6/qtimer.html)。不创建网络对象、请求、子进程或写缓存。LibrarySession 用 queued connection 路由结果，检查 requestId、generation、实体和 kind，处理请求覆盖/取消/库替换/销毁，重复或过期结果丢弃。

## 本地扫描与原生缓存

`src/library/DirectoryScanner`沿冻结`safeRootList/walkAlbums/readAlbum`的16个音频扩展名集合。根直属文件各自为单曲专辑；每个有直属歌曲的非根目录为独立专辑，多CD子目录不合并；只忽略以`.`开头的子目录，隐藏歌曲文件仍可索引。遍历不跟随文件/目录符号链接，显式根为符号链接时标记离线；目录迭代/文件stat任一失败时整根回滚并保留旧专辑为offline，避免部分读取失败表现为删除。迭代器使用error_code且不启用skip_permission_denied；目录前沿在堆上保存，取消在每个目录/文件与旧索引构建处检查。

本地根最多64个，必须为无NUL的绝对本地QUrl；词法清理、重复/嵌套去重不执行GUI磁盘IO。Windows按不区分大小写比较根，其他平台按路径字符串区分；不使用canonical路径重写已有身份。专辑ID为`album-`加直属单曲文件或专辑目录的原生绝对路径UTF-8 SHA256前24位，曲目ID同理使用`track-`和文件路径；Windows哈希前还原原生分隔符。`_fingerprint`保留冻结`size:mtimeMs`语义（含文件系统实际小数毫秒），不读音频内容。相同文件及fingerprint复用元数据/未知JSON字段；变化时恢复文件名/未知艺术家、Unknown支持状态并移除旧探测版本，不按扩展名断言可播放。封面、Qt元数据与旧索引导入由R22接入。

文件时间先转换到system_clock，再以整秒与亚秒部分计算mtimeMs，避免把完整epoch缩窄成int64纳秒。MSVC按其[官方file_clock接口](https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class?view=msvc-170)使用clock_cast（其file_clock提供to_utc）；libc++/libstdc++使用to_sys，不假定各标准库都提供同一个可选转换函数。

`LibraryStore`在一个私有单线程池串行读取/扫描/序列化/保存；至多一个在途job、一个最后扫描意图和一个合并的已接受保存快照，不向GUI逐文件发送通知。worker建立const快照，GUI按generation接受后调用一次`LibrarySession::setSnapshot`；切根/取消淘汰旧结果。取消不撤销此前已接受快照的保存。加载中切根仍读取旧缓存作为复用基线并完成安全检查，但不显示被替代的加载结果。旧库ID/路径索引为O(专辑+曲目)，遍历为O(目录+文件)加各目录排序，根判定至多64倍线性；内存随库和当前目录/待访目录增长，不是逐帧或无限请求队列。GUI只做最终模型/排序交接；其完整产品时延由R28测量。

新dataDir下的`library-index.json`使用`nativeSchemaVersion:1`，已知值与未知JSON字段共同写回；读取/写入上限256MiB，超限报错并保留原文件。`QSaveFile`禁止direct-write fallback，临时文件仅所有者读写，提交失败不覆盖旧文件。损坏/未知schema缓存阻止自动写回，仍允许内存扫描；用户将原文件另行保留或修复后可重新load。扫描/保存失败状态通过store.error/failed暴露；无后台重试循环。退出只等待唯一worker，丢弃未接受扫描并完成已接受的最后保存，不在正常GUI事件路径等待worker或序列化。

普通产品异步加载原生缓存；`--music-root /绝对路径`可重复，替换根后扫描；`--rescan-library`扫描已存根；`--data-dir /临时数据目录`隔离缓存。未配置库时为空库。Review验证仍只读显式匿名fixture，不读真实缓存；库/搜索面板的目录选择和状态UI归R23。

## 检查入口

`library-data`核对冻结库列、216搜索用例、JSON/URL/模型通知、导航播放独立与Provider竞态；`library-qml`核对稳定角色及离线/测试Provider读值。`runtime-library`是匿名库的窗口/资源检查，分组见test-gates.md。源码输入由`node tools/review/export-search-golden.mjs`导出，历史薄入口verify-r04.ps1/audit-r04.ps1；`--verify-library 匿名JSON`仅配合Review运行验证，默认不读真实库。

`directory-scanner`用临时合成目录覆盖扫描/重扫/改名/删除/离线/读取拒绝、取消/切根/加载竞态、缓存原子读写/损坏保护、源hash及GUI事件循环/一次模型重置；不加载媒体或显示窗口。`node tools/review/verify-scan-fingerprint.mjs`额外用本机Node的真实stat核对原fingerprint与UTF-8哈希；原生构建/CTest不依赖Node。`python3 tools/audit/audit-library-scan.py`生成COMMON/PERF和三平台逐符号/hash矩阵至本地verification；目标环境待验归计划R21-P。
