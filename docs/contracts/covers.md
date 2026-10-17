# 封面解码、图集与租约契约

R12 的生产填充池采用单张合并 atlas 与分块高清区域；其容量、准备中低清晰度回退、归位快照固定、资源峰值与窗口重建调度见 [archive-pool.md](archive-pool.md)。下述 2048² 分页/独立 selected texture 数值仍描述 R11 的通用 allocator 和独立 Review，不替代生产池汇总。

冻结来源为原始提交 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/cover-atlas.ts` 和 `src/viewer-album-cover.ts`，当前工作树位于 `legacy-web/src/`；历史 Git 读取继续使用既有清理映射。contain、1/64 留边、透明背景、选中/归位独立所有权和 tile 像素中心钳制按源码执行。

生产调用链是 `SceneItem -> CoverCache::request -> decodePool/ QImageReader -> CoverSnapshot -> SelectedCoverPool -> SceneItem::schedule/updatePolish -> RhiItem::submit/synchronize/renderFrame`。消费者异步请求256/1024两级图像，先低清再升级；生产池布局与副本消费者分别见archive-pool.md/detail-motion.md。

## 请求、像素与失效

CoverCache 仅在 GUI 线程调用。请求键为稳定identity、完整本地/qrc path、resolution和标题；已解码的有效图片按无标题别名复用，缺图及解码失败占位保留完整标题依赖，路径使用长度前缀，允许 Unicode、空格和分隔符。调用者的源内容或缺图标题变化必须`invalidate(identity)`并重新获取对应租约；完整场景更换才reset generation。原生扫描封面使用内容寻址URL携带revision，SceneItem在libraryChanged时只比较有界可见池的来源和占位标题（有效图片单独改名保留租约），释放变化身份的图集/选中租约，保留其余槽位和serial；不能仅用相同路径复用旧revision。完成占位携带fallbackTitle，过期标题不得发布到新来源；有效缩略图不被高清解码失败的占位覆盖。命中先 lookup，再 request 清晰度；request 命中不重复发 ready。容量满时返回 false，调用者按当前可见需求稍后重试，不在另一层无限排队。cancel(identity) 移除未开始需求并标记在途工作，保留已完成缓存；reset 取消全部旧 generation，清理 LRU，但保留在途容量记账直到完成。

QImageReader 在 worker 中读取尺寸、缩放能力和错误，设置 autoTransform，仅在原图超过目标最长边 256/1024 时设置 scaledSize，小图直接绘制到目标画布，避免先放大再缩小。插件不支持解码缩放时允许 Qt 有界的完整解码再缩放；带有效 ICC 配置的图像先在 worker 转换到 sRGB，与 shader 的颜色空间一致。未知尺寸或原图超过 4,194,304 像素时显式返回 error 与缺图画面，不尝试无界展开。该限制是本实现的输入资源保护，并非声称 Qt 不能读取更大的图像。PNG/JPEG 插件内部缓冲、字体及 Qt 分配器开销不冒充可测的 QImage 像素。

worker 用统一方形逻辑画布绘制一次 contain+1/64 留边，生成拥有内存的 RGBA8888 premultiplied 图像。缺图保留中性唱片圆环和标题提示，不替代成别张专辑。ready 发布 `shared_ptr<const CoverPixels>`，消费者不得通过外部存储修改像素；标题/来源字符串同样有界。paint 只接受 256/1024。生产 trusted immutable 标志仅用于该所有权链；外部 Review 输入仍在同步/入队隔离一次。

全局 QThreadPool 用 tryStart，不向 Qt 内部排队。每个 cache 的 active 同时包含正在解码与等待 GUI 消费的结果，直到 complete 才释放。generation/wanted 在读取前、绘图前、绘图后和发布前核对。销毁只锁住极短的回调投递区间、清空 receiver；不等待解码线程。已投递的 QObject 回调由 Qt 销毁规则取消，ready 接收者立即删除 cache 也有 guard。关闭后仍在解码的旧任务占用同一个全局三线程池，新 cache 不能叠加并发。

## 分页、上传与独立租约

AtlasAllocator 最多 432 个槽位，页面每边最多 2048、按设备上限向下取整到 256；典型设备是七张 2048² 页面，最后一页仅有 48 个有效槽。较小 GPU 使用更多固定页面，不缩小 tile 精度；小于 256 的设备显式失败。真实 RHI 分配前仍核对当前设备的尺寸、格式、采样和累计 ResourcePlan，不能仅相信调用者的上限。

acquire 引用计数固定身份槽，release 后可复用；重用分配单调 lease，publish 核对 allocator generation、identity、lease 和 decode generation。每槽 serial 单调，重复快照不重新标脏。uploads 每页提供有界增量，replay 仅为实际 scene graph 重建提供该页已就绪的不可变像素；不能把完整 CPU 页面合成后上传。

RhiItem 的 coverTiles/coverLeases 配对传入，render 独占 AtlasUploadQueue。预期租约变更使旧像素立即不再可采样，只有该租约 tile 的上传和绘制命令在同一 QRhi command buffer 提交时，uniform/实例 UV 才有效；未就绪使用零 UV extent 并 discard。队列先拒绝旧 generation/lease/serial，再转换/复制像素，按槽合并。enqueueAtlas 只在同步或资源重建时接收新快照，预期租约/布局未变化时复用共享值；renderFrame 仅排出实际上传批次，不反复提交旧批次。重建重新播放当前 retained tile，旧结果不能污染新资源。旧 Review 未提供 coverLeases 的原始 fixture 保持历史语义。

SelectedCoverPool 同时最多四个独立 lease，提供 selected 与最多三个 returning 的预算。fork 共享当前不可变像素，却取得独立 lease；selected 升级到 1024 不改变 returning 的图像。身份/source generation 不匹配及降清晰度升级拒绝；reset 后旧 lease 不能再更新，且仍占容量直到主人释放。对应独立 RhiItem cover texture 由 render 线程持有，每次只上传最新 revision；R18 使用这些租约时不得覆盖旧 returning 封面。

atlas 无 mip 链、使用线性 clamp 采样，无跨格各向异性。当前 QRhi 共用线性采样为 1×，选中采样不得超过冻结契约的 8×上限；不使用跨 tile 的采样近似。opaque.frag 的 coverPainted 分支避免再应用一次 contain/留边；未绘制的历史 oracle 继续执行原数学。UV 钳制到外侧像素中心。封面尺寸/generation 变化仅替换 cover texture 和布局兼容的 bindings，保持其他附件、阴影、postprocess 和 pipeline；独立 coverResourceRevision 使资源清单失效。

## 容量与资源清单

| 所有者/阶段 | 上限和载荷 | 释放/失效 |
| --- | --- | --- |
| 待解码描述符 | 96；identity≤1024、path≤32768、title≤256 UTF-16 code units，截断保留完整字素 | cancel/reset；入池后移除；满时拒绝 |
| 全局解码与每 cache 未消费结果 | 3；每 worker 源像素按 32 bytes/pixel 保守展开/转向估算，加 12MiB conversion/paint scratch，合计 140MiB/worker、420MiB reservation 峰值 | generation/wanted 取消；GUI completion 释放 reservation；不是进程 RSS 实测 |
| 完成 CPU LRU | 16MiB，即最多 64 个 256² 或四个 1024²；按实际 sizeInBytes 计费 | 有界 LRU 淘汰/reset；外部合法租约单独计费 |
| AtlasAllocator retained tile | 436×256²×4=109MiB（432阵列加最多4个独立身份）；dirty 引用共享 retained 图，不另复制 | release/reset |
| 单页 render 待上传/GUI replay | 最多该页 64 tile=16MiB；生产共享 retained 图，外部输入的隔离副本另有相同上界 | take/替代/重建；稳定帧零上传 |
| 单页上传批次 | 常态≤8 tile=2MiB；初始化≤64 tile=16MiB，剩余跨真实帧 | command buffer 消费；不把 QImage 调用次数当 GPU 流量 |
| selected/returning GUI高清tile | 四槽×16个256² tile≤16MiB，与原1024² lease为独立载荷；生产render replay共享这些immutable图 | owned槽释放/内容revision替换 |
| selected/returning CPU leases | 四个×1024²×4≤16MiB，低清晰度先复用，不与 LRU 共享载荷重复解释为实际占用 | lease owner 释放；reset 后仍计入四槽容量 |
| GPU atlas pages | 典型七页×2048²×4=112MiB，无 mip；其他设备为 pages×实际 pageSize×4 | renderer/page owner 释放；每页各自预算，完整阵列由 R12 汇总调度 |
| GPU selected/returning | 四个1024² RGBA8≤16MiB，无 mip；单个最新 selected 上传≤4MiB | 独立 renderer 释放；与 atlas tile 2MiB 统计分开 |

RHI 场景附件、MSAA/resolve/mips、共享 depth、shadow、AO/DOF/SMAA、lookup、geometry/uniform/instance buffer 和 Qt item 输出仍由 ResourcePlan/ResourceInventory 按实际尺寸计算，见 rhi-integration.md；不再另建整页 CPU atlas 或 tile GPU staging texture。按保守独立所有者相加，CPU reservation/完成缓存/retained/高清租约/高清tile的保守独立上界为577MiB；外部Review输入单页隔离另加16MiB时为593MiB；生产隐式共享实际载荷通常更低。这些数值不包含库模型、固定资产、opaque codec scratch、驱动/PSO/SRB、Qt swapchain/compositor。旧/新封面或页面更换同时存活的 descriptor 上界为两套 cover 载荷，其他兼容资源不重建；现有报告的全场景 old+new 是更保守的 descriptor 对上界，非 VRAM 驻留峰值。measuredVramBytes 保持 null。

- 原生主入口/使用SceneItem的Review在QGuiApplication之后建立退出scope guard：局部QObject先销毁并取消generation，再以finishDecoders等待全局三worker结束，最后销毁Qt应用/字体服务。此等待仅用于最终退出；普通cache/视觉树销毁仍不等待worker。
- CoverCache暂停时保留有界待解码队列/缓存/合法租约，停止retry和新worker准入；最多三个在途结果仍按generation/wanted验收。恢复继续有界队列，不重复已命中解码。
- AtlasUploadQueue尚有待上传tile时沿真实渲染续帧排空，每批仍遵守初始化/稳态原预算；静止/减少动态不依赖下一次用户输入完成封面。

## 检查入口

`CoverCacheTest`另核对重复命中/同专辑换曲、库快照替换不重解码/重传图集、单身份内容变化不改其他槽位，以及选中更新后归位像素仍固定。它覆盖横竖方/alpha/缺图/过大展开、Unicode路径、重复/拒绝/LRU、取消/reset、在途关闭及发布时销毁。`CoverAudit`不再评分raw/painted图像；contain/留边使用源UV公式与`CoverCacheTest`解析输入，实际GPU纹理尺寸/上传载荷和自身正常输出独立核对。它覆盖八tile/跨帧预算、旧槽隐藏/晚到拒绝、selected独立4MiB上传、稳定零复制/上传、封面尺寸不重建无关目标及真实SG释放恢复。96张2048²合成JPEG的准备时间、事件响应及各阶段峰值分开记录；不据其推导产品帧率。

目标入口`cover-cache`及Review `cover-gpu`，按[test-gates.md](test-gates.md)选择实际隐藏/窗口组；源码自动安全检查使用`tools/audit/audit-best-practices.py`，架构与生命周期仍需逐路径审查。R19A的缺图绘制修正固定为首行CSS500、脚注恢复Normal400，其余画布/基线/contain不变。资产来源归assets.md，生产合并atlas布局归archive-pool.md，目标欠验归计划R34。

`archive-pool-gpu`额外沿生产SceneItem→AtlasUploadQueue→QRhi提交验证快照中的同专辑换曲零解码/上传，以及单封面revision恰好一个256²阵列tile加16个高清tile的实际上传；兼容render/cover资源generation不变，收敛后零上传。选中/归位旧像素隔离另由上述租约单元检查证明，不用源图评分替代资源事实。
