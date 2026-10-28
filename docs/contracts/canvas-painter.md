# Canvas Painter 绘制接口

本文负责生产C++ Canvas Painter的API、生命周期与资源语义；浏览静态层见[browse-ui.md](browse-ui.md)，滑尺见[navigation.md](navigation.md)，序章见[opening.md](opening.md)。

## API与状态

- 同一最新Kit链接Qt6::CanvasPainter；资源使用qrc别名。
- QCanvasPainterItem由createItemRenderer创建所属render-thread renderer；synchronizeData只传值快照。路径/字体在renderer初始化或真实依赖变化时更新，使用独立painter和有界path group；透明fill开启alphaBlending，sampleCount=1，保留模块几何AA。
- 绘制只消费既有控制器与唯一FrameClock，隐藏不发布无效状态；缓存按路径/样式/字体/尺寸/DPR失效。paint/同步不得IO、解码、等待、GPU finish或读回。
- 描边/端点/连接、曲线、渐变/遮罩/裁切、透明预乘/颜色域和基线遵守生产来源语义；需要Quick合成的效果沿现有视觉层级组合。

## 资源与检查

- Qt管理的RGBA8单采样离屏附件透明合成进Quick，无mip/resolve/业务封面上传。颜色4bytes+depth/stencil8bytes保守上界按实际物理尺寸计，resize旧/新重叠、字体图集/内部buffer/driver另列，生产使用纳入ResourcePlan。
- 显式验证从实际提交纹理读回并核对revision，单个在途请求按MaxAsyncReadbackFrames+1有界排空。Canvas的grabWindow可触发重绘/清空命令，不能用它证明原提交；不得为验证引入持续重绘。
- 关闭QCPAINTER_DEBUG_COLLECT，避免模块诊断计时器驱动无效绘制；GUI准备、同步、paint与Quick提交分别计量，统计解释及采样条件见[performance.md](performance.md)。
- 生产滑尺与序章分别通过navigation/opening契约的真实宿主检查结构、内容、静止零工作和必要恢复；Release分组和原生诊断边界见[test-gates.md](test-gates.md)。自身输出质量、状态/数学和资源事实分别验证。
