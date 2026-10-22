# SSAO与画质

本文负责SSAO算法、深度/法线输入和RenderQuality；阴影实现归[rhi-opaque.md](rhi-opaque.md)，后处理资源分配归[rhi-postprocess.md](rhi-postprocess.md)。冻结算法/资源hash在`resources/manifests/occlusion.json`。

## 算法与输入

- 有效链为scene构造→enableSelectionLighting→softenMusicContactShadows→setQuality后再次soften→resize/update。生产radius=.18、min/max=.001/.035（源orthographic depth单位），构造.38/.44与.09不是有效最终值。
- 生产固定使用64核；独立算法探针另覆盖16/32核。kernel/noise用seed417的LCG与锁定SSAOPass/SimplexNoise分别重置生成，RGBA32F little-endian。
- 完整壳体/纸面（含contain留白）不透明几何覆盖，复用生产origin/矩阵/相机；normal只unpack，不额外normalize。独立view normal/RGB24线性距离，nearest深度/原滤波；RGB24白色精确背景哨兵。near/far恢复viewZ，不把主不透明depth当玻璃几何。
- 标准透视还原`viewZ=n*f/((f-n)*depth-f)`，`xy=ndcXY*tan(fov/2)*(aspect,1)*(-viewZ)`；framebuffer Y转换与hardware [0,1]深度分开。
- 保留原半球样本、scalar noise xyz复制、TBN、严格min<delta<max、原累加顺序、5×5 box与`mix(1,blur,.22)`。raw/blur当前R16F；采样通道唯一.r，诊断重建重复RGB/alpha，不降half精度。
- Quick3D Review的OcclusionPrepass显式将有符号world normal变view/pack，将可能量化的provider输出编码RGB24；不能把请求格式当底层实测。生产QRhi显式附件格式以资源契约为准。

## 画质边界

所有屏幕空间颜色、几何、AO、景深、抗锯齿与二维合成目标按当前内容逻辑尺寸乘实际窗口有效DPR取整分配。禁止设置DPR上限、渲染倍率、固定参考画布或像素总量钳制后再缩放到窗口；跨屏、DPR与resize沿现有失效链更新。设备尺寸、格式或分配不支持时显式报错，不降低目标精度。素材纹理的透视采样、独立mip层和有意的景深/磨砂/背景模糊不改变这一尺寸规则。

RenderQuality只保存艺术参数depthOfField，非负有限值；它不改变相机、导航、播放或目标分辨率。生产始终启用4×几何MSAA、SMAA、64核全分辨率AO和完整透射捕获；阴影贴图按当前物理输出最长边的两倍向上取整为二次幂，以稳定区间内的缓存。硬件能力由分配前检查，不以软件固定档位代替。效果数学与当前质量参数分别由postprocess/rhi-opaque维护。

AO关闭只存在于独立算法探针，生产AO/normal/depth与颜色同尺寸；Bokeh完整深度可共用同尺寸AO几何。诊断通道跳过输出转换。生产clip5/300/radius.18与明确压力输入.1/100/radius.38分别标记。

## 源公式检查

`rhi-ao-gpu`：奇偶宽/三核全分辨率背景状态、243接触点、5×5/.22、R16F格式/缓存/一次SG重建。raw/blur≤1/255、.22合成≤2/255，背景恒1；完整生产回放另核对有效AO输入。

GL未对齐R16F显式读回按实际PACK_ALIGNMENT同格式复制到补齐存储宽后解码，产品尺寸不变。核仅创建时上传，动态前缀80bytes/通道，三个AO pass常态240bytes；无闲置kernel纹理。

入口：`ctest --preset qt-release-render -R '^rhi-ao-gpu$'`；实际测试分组见test-gates.md，目标欠验归R36。
