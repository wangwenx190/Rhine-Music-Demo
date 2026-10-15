# RHI生产接线、同步与遥测

本文负责SceneItem生产输入/失效/生命周期与采集接线；通道实现分别在rhi-opaque/rhi-shell/rhi-postprocess契约，测量指标和采样规则只在[performance.md](performance.md)定义。

## 生产与同步

- `App.qml → SceneItem → SceneAdapter::frame/SelectionLighting/RenderQuality → RhiItem::submit → synchronize/renderFrame`。SceneAdapter独占相机/实体/投影/拾取/focalDepth；QML不维护后端专属状态。生产实体由填充/真实列和有限选中/归位池提供，封面消费者见archive-pool.md。
- 普通构建不链接Quick3D/FixedTextureData，不打包GeometryScene/材质/后处理研究资产；Review才注册ResearchApp及隔离材质入口。几何验证使用生产提交矩阵。
- 相机/布局/实体实际变化才更新几何revision，输入revision独立。polish合并同帧相机/灯光/画质/封面；相同submission不重复复制。封面owned immutable像素共享，外部Review输入隔离一次，cacheKey变化才交接。
- PreparedAssets在首个RhiItem的GUI构造/show前一次预载固定qrc GLB/qsb/PMREM half/lookup，进程内有界不可变；render/同步不读盘或解码。
- 目标兼容键含尺寸/阴影/MSAA/布局，封面有独立revision；后处理键含AO尺寸/核/DOF/SMAA/实际附件身份。相机、焦点、主题数值和实体数量不得重建无关目标/PSO/SRB。buffer容量/上传边界只在rhi-postprocess.md维护。
- 隐藏停止视觉帧源；恢复合并ensurePolished/update并提交最新快照。GPU资源由render释放，错误跨renderer虚函数前捕获并显示errorText；已录命令引用的资源等待正常SG释放，不在错误回调提前销毁。QObject销毁取消回调，旧窗口结果核对epoch。

## 资源统计接线

- resourcePlan按纯值资源键变化计算分配前请求，核对真实QRhi尺寸/格式/采样；ResourceInventory开启遥测后从实际描述符复核，共享附件按身份去重。
- 纹理计完整mip/sample，UnknownFormat明确为0并按D32FS8的8bytes保守上界。记录estimatedCurrentBytes和resizeOldPlusNewUpperBoundBytes；后者仅资源替换对的估算，不是driver延迟回收后的驻留峰值。
- 计入Qt item输出；窗口swapchain/compositor、driver padding/PSO/SRB/buffer轮换与字体图集不可完全观测时逐项注明，measuredVramBytes=null。封面/atlas分阶段计数归covers/archive-pool/rhi-postprocess，不重复相加。

## 输入与采集接线

- SceneAdapter::recordInput记录单调时间/revision，SceneFrame携带真正已提交导航的关联revision；render报告submittedNs。input→状态→Qt命令提交与屏幕显示严格区分。
- cpuSubmitMs只含renderFrame到最后输出命令，synchronizationMs单列最近快照同步；静态复用不解释为每帧重复同步。遥测组装自身开销不混入提交数字，GPU timestamp不可用null。
- 遥测默认关闭；开启时mailbox最多128样本、一个排队GUI回调，溢出丢最旧并计coalescedSamples，丢样不能算统计完整。Review最多40000提交/呈现事件，溢出失败；frameSwapped按实际render事件时间独立记录。
- Review显式capture最多两批在途，纹理身份去重/共享值交付；request保留到对应提交。静态异步读回按MaxAsyncReadbackFrames+1有界续帧，普通产品无回调不增加工作。
- `yalahoo --verify-integration 新目录 --integration-seconds 12..120`按真实事件推进输入/尺寸/画质/重建，等待各步提交确认；FrameAnimation仍唯一帧源，无processEvents/msleep轮询。检查九尺寸/四画质/双主题/十次恢复，报告实际覆盖，短单盒输入不能冒充完整产品。
- `rhi-postprocess-audit --profile`是独立分通道GPU诊断，不能混入正常120秒性能样本；旧工具非像素迁移归R19D4B2。record-only模式只改性能退出门槛，functionalPassed/samplingComplete/gateEnforced/performanceAcceptance分开，用户历史例外与18.35/20.98ms待复现由performance契约和R34维护。
