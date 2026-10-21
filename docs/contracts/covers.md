# 封面解码、图集与租约契约

R12 的生产填充池采用单张合并 atlas 与分块高清区域；其容量、准备中低清晰度回退、归位快照固定、资源峰值与窗口重建调度见 [archive-pool.md](archive-pool.md)。下述 2048² 分页/独立 selected texture 数值仍描述 R11 的通用 allocator 和独立 Review，不替代生产池汇总。

冻结来源为原始提交 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/cover-atlas.ts` 和 `src/viewer-album-cover.ts`，当前工作树位于 `legacy-web/src/`；历史 Git 读取继续使用既有清理映射。contain、1/64 留边、透明背景、选中/归位独立所有权和 tile 像素中心钳制按源码执行。

生产调用链是 `SceneItem -> CoverCache::request -> decodePool/ QImageReader -> CoverSnapshot -> SelectedCoverPool -> SceneItem::schedule/updatePolish -> RhiItem::submit/synchronize/renderFrame`。消费者异步请求缩略图和投影需求清晰图，先低清再升级；生产池布局与副本消费者分别见archive-pool.md/detail-motion.md。

## 请求、像素与失效

CoverCache 仅在 GUI 线程调用。请求键为稳定identity、完整本地/qrc path、resolution和标题；已解码的有效图片按无标题别名复用，缺图及解码失败占位保留完整标题依赖，路径使用长度前缀，允许 Unicode、空格和分隔符。调用者的源内容或缺图标题变化必须`invalidate(identity)`并重新获取对应租约；完整场景更换才reset generation。原生扫描封面使用内容寻址URL携带revision，SceneItem在libraryChanged时只比较有界可见池的来源和占位标题（有效图片单独改名保留租约），释放变化身份的图集/选中租约，保留其余槽位和serial；不能仅用相同路径复用旧revision。完成占位携带fallbackTitle，过期标题不得发布到新来源；有效缩略图不被高清解码失败的占位覆盖。命中先 lookup，再 request 清晰度；request 命中不重复发 ready。容量满时返回 false，调用者按当前可见需求稍后重试，不在另一层无限排队。cancel(identity) 移除未开始需求并标记在途工作，保留已完成缓存；reset 取消全部旧 generation，清理 LRU，但保留在途容量记账直到完成。

QImageReader 在 worker 中读取尺寸、缩放能力和错误，设置 autoTransform，仅在原图超过请求缓存层最长边时设置 scaledSize，小图直接绘制到目标画布，避免先放大再缩小。插件不支持解码缩放时由 Qt 完整解码后缩放；带有效 ICC 配置的图像先在 worker 转换到 sRGB，与 shader 的颜色空间一致。未知尺寸或解码失败返回 error 与缺图画面。

图片解码准入使用 Qt 的有效 `QImageReader::allocationLimit()`，不由产品覆盖；进程启动前的 `QT_IMAGEIO_MAXALLOC` 以 MiB 控制该值，0 明确关闭检查。Qt 首次查询时缓存环境值，运行中不改环境变量或在 worker 改全局上限。PNG/JPEG/WebP 的输出图像通过 `QImageIOHandler::allocateImage` 按格式且至少32bpp核算；JPEG 还按实际缩放输出分配，并将上限传给解码库。此为单次图像分配边界，不是全部 worker、编解码内部缓冲、颜色转换、画布/mip、字体或进程总内存上限。依据为[Qt分配接口](https://doc.qt.io/qt-6/qimageiohandler.html#allocateImage)、[Qt环境值解析](https://github.com/qt/qtbase/blob/dev/src/gui/image/qimagereader.cpp)、[JPEG](https://github.com/qt/qtbase/blob/dev/src/plugins/imageformats/jpeg/qjpeghandler.cpp)、[PNG](https://github.com/qt/qtbase/blob/dev/src/gui/image/qpnghandler.cpp)与[WebP](https://github.com/qt/qtimageformats/blob/dev/src/plugins/imageformats/webp/qwebphandler.cpp)源码。

worker 用统一方形逻辑画布绘制一次 contain+1/64 留边，生成拥有内存的 RGBA8888 premultiplied 图像。缺图保留中性唱片圆环和标题提示，不替代成别张专辑。ready 发布 `shared_ptr<const CoverPixels>`，消费者不得通过外部存储修改像素；标题/来源字符串同样有界。paint接受正的二次幂缓存层。生产 trusted immutable 标志仅用于该所有权链；外部 Review 输入仍在同步/入队隔离一次。

全局 QThreadPool 用 tryStart，不向 Qt 内部排队。每个 cache 的 active 同时包含正在解码与等待 GUI 消费的结果，直到 complete 才释放。generation/wanted 在读取前、绘图前、绘图后和发布前核对。销毁只锁住极短的回调投递区间、清空 receiver；不等待解码线程。已投递的 QObject 回调由 Qt 销毁规则取消，ready 接收者立即删除 cache 也有 guard。关闭后仍在解码的旧任务占用同一个全局三线程池，新 cache 不能叠加并发。

## 分页、上传与独立租约

AtlasAllocator 最多 432 个槽位，页面每边最多 2048、按设备上限向下取整到 256；典型设备是七张 2048² 页面，最后一页仅有 48 个有效槽。较小 GPU 使用更多固定页面，不缩小 tile 精度；小于 256 的设备显式失败。真实 RHI 分配前仍核对当前设备的尺寸、格式、采样和累计 ResourcePlan，不能仅相信调用者的上限。

acquire 引用计数固定身份槽，release 后可复用；重用分配单调 lease，publish 核对 allocator generation、identity、lease 和 decode generation。每槽 serial 单调，重复快照不重新标脏。uploads每页提供有界增量；replay缓存该页已就绪的不可变描述符，按实际目标列跨度映射slot，供合并提交及scene graph重建共享。身份准入/最后释放、像素发布、generation或目标跨度变化使缓存失效；释放与换代立即清除缓存持有的旧快照，已提交值保持独立有效期。不能把完整CPU页面合成后上传。

RhiItem 的 coverTiles/coverLeases 配对传入，render 独占 AtlasUploadQueue。预期租约变更使旧像素立即不再可采样，只有该租约 tile 的上传和绘制命令在同一 QRhi command buffer 提交时，uniform/实例 UV 才有效；未就绪使用零 UV extent 并 discard。队列先拒绝旧 generation/lease/serial，再转换/复制像素，按槽合并。enqueueAtlas 只在同步或资源重建时接收新快照，预期租约/布局未变化时复用共享值；renderFrame 仅排出实际上传批次，不反复提交旧批次。重建重新播放当前 retained tile，旧结果不能污染新资源。旧 Review 未提供 coverLeases 的原始 fixture 保持历史语义。

SelectedCoverPool 同时最多四个独立 lease，提供 selected 与最多三个 returning 的预算。fork 共享当前不可变像素，却取得独立 lease；selected 升级清晰层不改变 returning 的图像。身份/source generation 不匹配及降清晰度升级拒绝；reset 后旧 lease 不能再更新，且仍占容量直到主人释放。对应独立 RhiItem cover texture 由 render 线程持有，每次只上传最新 revision；R18 使用这些租约时不得覆盖旧 returning 封面。

worker以线性光、预乘alpha对每张封面独立做面积归约，生成完整mip链；不对合并图集整体生成mip。各层仍以sRGB编码的预乘RGBA8随对应dirty分块增量上传，字节全部计入原上传预算，静止不重算/重传。生产cover-sampling.glsl以texelFetch读取每个原始texel，先除alpha、解码sRGB并在线性光下重新预乘，再做双线性、跨mip与足迹积分；最终alphaTest后仅还原线性颜色，不重复解码。不能先对编码值插值再解码。每个积分点最多读取两层各四个texel，32点上界为256次texel读取；整数LOD只读取一层，1×1层只读取一个texel。GPU纹理格式、CPU不可变快照与上传预算不变。opaque.frag根据UV导数的椭圆足迹选择mip和自适应长轴采样；最多32个重叠积分点，极端斜角相应扩大mip足迹，控制工作量并避免欠采样。每级以实际textureSize换算整数原点与范围并钳制半像素边界，不跨封面串色；透射捕获保留独立mip链。alpha测试门槛仍为.025，颜色还原的分母以同门槛为下界，使透明helper片元也保持有限，不改变存活片元的颜色。

清晰封面缓存层由实际纸面四角投影边长×窗口DPR×留边系数向上取整至二次幂决定，不设固定清晰图尺寸上限。普通阵列使用256²缩略图，独立selected/returning按自身快照尺寸占用清晰区；归位仍固定接管快照，晚到结果仅升级当前selected。普通阵列缩略图是来源设计的资源取舍，不是安全边界；是否需升级须结合最终可见区域、源壳体透射采样与景深判断，不能仅凭投影边长宣称可见画质缺陷或等价。QImageReader分配能力与实际GPU尺寸不足时显式报告错误，不更改屏幕空间目标。

封面尺寸/generation改变只替换cover texture和兼容bindings，保持其他附件/阴影/postprocess；coverResourceRevision使资源清单失效。CPU缓存将每级mip按真实字节计费。在途工作集估算只按请求画布面积×36bytes计入paint、浮点工作层、编码mip与分块；不包含源解码、颜色转换或编解码内部存储，不称总内存预留。统计独立给出有效单图分配上限（关闭时为null）与实际返回QImage的最大载荷。

## 容量与资源清单

- 描述符队列最多96，全局worker/未消费结果最多3；完成LRU按含mip真实字节保持16MiB。取消/换代不提前释放仍在途的容量。
- 缩略图与清晰快照以不可变共享值保留，mip约增加三分之一像素载荷；清晰分块仅在内容/区域变化时切分，replay引用已有像素，不合成整页CPU图集。
- render上传常态2MiB、初始化16MiB，包含全部层级。GPU纹理、Qt附件和resize旧/新同时存活由ResourcePlan/ResourceInventory按实际尺寸/层级展开；不以估算代替driver显存。

- 原生主入口/使用SceneItem的Review在QGuiApplication之后建立退出scope guard：局部QObject先销毁并取消generation，再以finishDecoders等待全局三worker结束，最后销毁Qt应用/字体服务。此等待仅用于最终退出；普通cache/视觉树销毁仍不等待worker。
- CoverCache暂停时保留有界待解码队列/缓存/合法租约，停止retry和新worker准入；最多三个在途结果仍按generation/wanted验收。恢复继续有界队列，不重复已命中解码。
- AtlasUploadQueue尚有待上传tile时沿真实渲染续帧排空，每批仍遵守初始化/稳态原预算；静止/减少动态不依赖下一次用户输入完成封面。

## 检查入口

`rhi-scale-audit 新目录 --filtering-only`仅执行共享生产采样helper的12组解析UV/颜色/alpha/跨mip/长轴/边界检查，以及生产纸面图集隔离与稳定零上传检查；不附带DPR、整场或压力矩阵。解析期望从本次输入texel独立按double计算线性预乘插值，实际输出为RGBA16F。

`CoverCacheTest`另核对重复命中/同专辑换曲、库快照替换不重解码/重传图集、单身份内容变化不改其他槽位，以及选中更新后归位像素仍固定。它覆盖横竖方/alpha/缺图、PNG/JPEG/WebP单图分配准入及JPEG解码缩放、Unicode路径、重复/拒绝/LRU、取消/reset、在途关闭及发布时销毁。定向准入检查可用 `CoverCacheTest imageAllocationPolicy`，改变环境上限须启动独立进程。`CoverAudit`不再评分raw/painted图像；contain/留边使用源UV公式与`CoverCacheTest`解析输入，实际GPU纹理尺寸/上传载荷和自身正常输出独立核对。它覆盖八tile/跨帧预算、旧槽隐藏/晚到拒绝、selected独立完整mip上传、稳定零复制/上传、封面尺寸不重建无关目标及真实SG释放恢复。96张2048²合成JPEG的准备时间、事件响应及各阶段峰值分开记录；不据其推导产品帧率。

目标入口`cover-cache`及Review `cover-gpu`，按[test-gates.md](test-gates.md)选择实际隐藏/窗口组；源码自动安全检查使用`tools/audit/audit-best-practices.py`，架构与生命周期仍需逐路径审查。R19A的缺图绘制修正固定为首行CSS500、脚注恢复Normal400，其余画布/基线/contain不变。资产来源归assets.md，生产合并atlas布局归archive-pool.md，目标欠验归计划R36。

`archive-pool-gpu`额外沿生产SceneItem→AtlasUploadQueue→QRhi提交验证快照中的同专辑换曲零解码/上传，以及单封面revision恰好一个256²阵列tile加16个高清tile的实际上传；兼容render/cover资源generation不变，收敛后零上传。选中/归位旧像素隔离另由上述租约单元检查证明，不用源图评分替代资源事实。
