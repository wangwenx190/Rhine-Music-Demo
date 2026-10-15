# SSAO与画质

本文负责SSAO算法、深度/法线输入和RenderQuality；阴影实现归[rhi-opaque.md](rhi-opaque.md)，后处理资源分配归[rhi-postprocess.md](rhi-postprocess.md)。冻结算法/资源hash在`resources/manifests/occlusion.json`。

## 算法与输入

- 有效链为scene构造→enableSelectionLighting→softenMusicContactShadows→setQuality后再次soften→resize/update。生产radius=.18、min/max=.001/.035（源orthographic depth单位），构造.38/.44与.09不是有效最终值。
- 原始/高32、极高64、性能0，另支持16核；kernel/noise用seed417的LCG与锁定SSAOPass/SimplexNoise分别重置生成，RGBA32F little-endian。
- 完整壳体/纸面（含contain留白）不透明几何覆盖，复用生产origin/矩阵/相机；normal只unpack，不额外normalize。独立view normal/RGB24线性距离，nearest深度/原滤波；RGB24白色精确背景哨兵。near/far恢复viewZ，不把主不透明depth当玻璃几何。
- 标准透视还原`viewZ=n*f/((f-n)*depth-f)`，`xy=ndcXY*tan(fov/2)*(aspect,1)*(-viewZ)`；framebuffer Y转换与hardware [0,1]深度分开。
- 保留原半球样本、scalar noise xyz复制、TBN、严格min<delta<max、原累加顺序、5×5 box与`mix(1,blur,.22)`。raw/blur当前R16F；采样通道唯一.r，诊断重建重复RGB/alpha，不降half精度。
- Quick3D Review的OcclusionPrepass显式将有符号world normal变view/pack，将可能量化的provider输出编码RGB24；不能把请求格式当底层实测。生产QRhi显式附件格式以资源契约为准。

## 画质边界

RenderQuality保留render-quality.ts四预设/归一化：scale50–200/步长5、DPR cap1/1.5/2/3、shadow0/1024/2048/4096、AO0/16/32/64及倍率.5/.75/1。全目标≤8294400像素且受当前设备尺寸限制，limited公开、不改用户偏好；stageScale按源窗口公式，独立Review明确用1。画质不改变相机/导航/播放或业务对象。

0 samples释放对应通道；AO按倍率，Bokeh完整深度独立，全分辨率兼容时才共用。诊断通道raw AO/blur/depth/normal/linearColor跳过输出转换。生产clip5/300/radius.18与明确压力输入.1/100/radius.38分别标记。

## 源公式检查

`rhi-ao-gpu`：27奇偶宽/三倍率/三核背景状态、243接触点、5×5/.22、R16F格式/缓存/十次SG重建。raw/blur≤1/255、.22合成≤2/255，背景恒1；完整生产回放另核对有效AO输入。

GL未对齐R16F显式读回按实际PACK_ALIGNMENT同格式复制到补齐存储宽后解码，产品尺寸不变。核仅创建时上传，动态前缀80bytes/通道，三个AO pass常态240bytes；无闲置kernel纹理。

入口：`ctest --preset qt-release-render -R '^rhi-ao-gpu$'`、`python3 tools/audit/audit-rhi-ao.py`、`python3 tools/audit/bake-occlusion-shaders.py`；选择实际测试分组见test-gates.md。旧verify-r09/occlusion-audit中的图像oracle不运行，非像素覆盖映射见source-fidelity-review.md的R19D4B2，目标欠验归R34。
