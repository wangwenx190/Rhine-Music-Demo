# 填充实例池与选择坐标契约

`ArchivePool` / `SelectionController` 位于 `src/render/`，连接既有 `MotionController` 和唯一 `FrameClock`。`SelectionController`组合`ArchiveField`维护有界脉冲、idle包络与纯场采样，继续由唯一FrameClock推进；坐标重基同步平移脉冲，不重置绝对波相位。`SceneAdapter` 将实例、相机和选择变换一并发布为不可变 `SceneFrame`；`SceneItem` 的 atlas 租约、世界空间灯光、投影、ray hit、阴影 caster 和归位副本使用同一显示变换。导航通过稳定 albumId 同步 LibrarySession，搜索和导航不改变播放 ID。

## 冻结语义

- 来源是冻结提交 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `archive-loop.ts`、`motion.ts` 和 `scene.ts` 音乐生产路径，经清理后的 Git 来源映射读取。9×48 池，lane 顺序 `0,1,2,3,4,-2,-1,5,6`，row 起点 −8；空间间隔为 5.2 / 0.62，参考中心为 lane 2 / row 15.5。逻辑专辑行从 12 开始，各列专辑数是身份循环周期，48 仅是显示窗口周期。
- `nearestOccurrence` 使用 floor 半周期，正负半周期平局与 JavaScript 一致。上下/左右方向输入走相邻物理格，不把尾到首解释成反向运动；显式稳定 ID 跳转选择最近出现位置，指定格可携带物理行。池槽在 ±4.5 列 / ±24 行边界复用，逻辑库规模不改变池容量。
- 非片头 field 保留 archive/idle 波浪、最多六个 3.2 秒脉冲、0.24 ripple clamp、肩部 settlingWave、columnStrength 与半行差分 slope。闲置开始时间为 2.5 秒，idle/pulse gain 的指数跟随率沿用源码。浏览 preview lift 为 0.9；抽取/归位 primitive 保留 lift、returnStep 和旋转对齐期间的世界 Y。片头调度归opening.md，展示与详情运动分别见presentation.md/detail-motion.md。

音乐静态详情 lift 为盒高 3.35 + 间隙 0.12 = 3.47，与scene.md的冻结相机/模型输入对照。通用电影内核的 4.05 作为独立 primitive/stress 上界，不能代替音乐详情高度。
- 选中盒及最多三个 returning 副本接管物理格；原实例隐藏，以 cell 而非 albumId 判断，循环中同一专辑的其他物理格仍合法。归位完成先移除副本，再恢复该格；返回中的相同格可重新接管并复用对象 ID、lift、rotation 和封面租约。四槽已满时在改状态前拒绝新接管，不能无界追加副本。
- row rebase 周期取各列实际数量的 LCM，超过 1e6 时不做 row rebase；lane 使用逻辑列数。阈值是 2048 与两倍 row 周期。平移选择、所有轨迹原点/目标、returning 格、脉冲和光带跟随 spring；velocity、acceleration 和绝对 idle/archive 相位保持不变。原始库 ID 不变化。

显示位移以选中格为相对原点，避免大 rail/column 相消后污染半周期平局；静止时仅消除原始坐标规模推导的八个 double ulp 范围内的算术残差，不改变运动导数或数学容差。池的光学排序身份按有限物理格保留，跨 rebase 不因为槽号重排而改变等深排序。完整前沿对物理列模9、物理行模48的余数组合恰好各访问一次；固定432项表以完整物理坐标核对身份，回收时才换ID，不逐帧分配身份hash。

旋转按冻结 XYZ 顺序以 double 求矩阵元素，再统一转为 float；低 lift 与 pitch/yaw 同时存在的组合通过独立源码输入校验。零 gain 不计算无效 archive/idle/ripple 分量；显示实例完全相同时复用已发布快照和 revision，减少动态时继续记录时钟而不反复提交静态阵列。

## 热路径与资源

快照/排序边界建立 O(N) 的稳定 ID→格及 ID→本地封面描述索引（完整显示名与有界字素占位标题分开保留）；LibrarySession 在快照边界建立 O(N+T) 的专辑/曲目身份索引，使池导航同步无需线性查库，重复身份保留原先的首项语义。未接入库的 SelectionController 不接管其他运动调用。帧中只处理 432 格和四个有限接管对象；ArchivePool单遍更新，同一物理列的48格共享一次列坐标、规范列查找和行中心计算，引用只在本次更新内保留；发布复用有界实例暂存数组，阵列公共显示偏移/片头位移每批计算一次，相邻行的共同半行场采样仅在本次发布内复用；列强度、真实列中心/活动度及片头混合系数也按本次发布的列采样复用，不跨状态更新保留。renderer 可接收最多 436 个对象，含窗口外有限归位/选中格；固定 buffer 容量不随实际实体数量切换。可见性每帧计算一次，depth key 先生成再排序；剔除只影响可见 draw，完整世界空间阴影 caster 不丢失。实体公共光场缓存按矩阵/灯光键失效，沿用现有渲染图。

生产池使用单张RGBA8图集：缩略图为24列256²槽，固定有界身份436；清晰区按当前投影需求缓存层的最大边长S保留2×2个区域，x起点为0/S，y起点为6144向上对齐S及其下一行。图集宽max(6144,2S)、高alignUp(6144,S)+2S，所有缩略图slot按实际行跨度映射。独立mip的布局、采样与计费归covers.md。只有整个清晰区域的每个分块和mip均已提交才暴露其UV，准备中使用已就绪缩略图。尺寸变化重放现存像素，身份/lease不变；硬件尺寸、格式及分配不足时显式报错。

每个池身份保留有引用计数的不可变 tile。完成 LRU 淘汰后不会重复解码仍被 atlas 租约持有的图。解码准入/完成容量由covers.md维护；满队列拒绝后由ready→polish重试。GUI replay 最多 436 个 thumbnail tile 加 64 个高图分块，隐式共享像素；render queue 按槽/代次/lease/serial 合并，重复结果在复制前拒绝，常态每帧最多上传八个 tile。replay 同时保证 GUI 提交被后续 polish 合并、hide/restore 和 scene graph 重建时不会丢失当前像素。旧 generation 不可发布。

待解码键使用有界 hash 索引，重复请求不扫描描述符队列；生产先接纳 selected 的低/高清请求，并在无待解码容量时停止提交新请求，避免每次完成通知都重复撞满队列。容量释放后 coalesced polish 继续接纳。归位对象固定接管时的快照，晚到高清结果只升级当前 selected；重新接管时才能继续升级。render 对高图全部区域统一核对当前/已上传 lease，未就绪时使用独立低清晰度区域；新租约生效后旧租约不再被 ready 判定接受。

CPU保留载荷按缩略图、清晰快照及其分块/mip的实际字节分别记录，隐式共享不重复解释成实测占用。GPU完整mip链和旧/新图集同时存活均计入既有ResourcePlan/ResourceInventory；动态清晰区不采用固定显存结论。instance buffer仍为436×112bytes，uniform按实际stride与有效cursor计费，descriptor估算不冒充driver驻留显存。

库/排序替换使工作代次、atlas、接管租约失效，保留当前稳定 ID 的新规范格；QObject/QPointer 保证销毁后不访问旧 selection。render 独占 GPU 所有权。Review 采集 request 跨 coalesced polish 保留，避免 cover/camera 通知吞掉请求；生产无同步 readback、磁盘访问或手动转事件循环。

SceneAdapter 销毁后的晚到封面回调直接丢弃；CoverCacheTest 的隐藏 QQuickWindow/SceneItem 检查在解码期间销毁适配器并等待结束，全程不显示窗口。

## 检查入口

`archive-pool`核对池顺序/方向/半周期、field与矩阵、接管/归位/重新接管、非零导数rebase、拾取、空库/90,000库/LCM溢出及播放隔离。数学输入在`tests/fixtures/archive-pool/golden.json`；源导出`node tools/migration/export-archive-pool-golden.mjs`。

Review `archive-pool-gpu`直接运行SceneItem，验证432池、低/高清准备、局部上传/稳态零上传、真实输入与四槽所有权、双主题/rebase、实体数量不重建目标、必要SG重建。源矩阵采样直接消费上述已提交fixture，真实列场数值消费`tests/fixtures/real-columns/fields.json`；核对矩阵/投影、自己输出的有效区域与实际资源，不比较浏览器图片。入口分组见test-gates.md；目标剩余要求归计划R36/R19B-P。
