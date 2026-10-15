# Canvas Painter 共用绘制接口

本文只负责C++ Canvas Painter与QML Canvas2D的API、生命周期和比较方法；浏览静态层见[browse-ui.md](browse-ui.md)，生产滑尺见[navigation.md](navigation.md)，标题见[title-reels.md](title-reels.md)。

## API与状态

- 同一最新Kit链接Qt6::CanvasPainter；Canvas2D导入QtCanvas2D，模块依赖QtQuick/QtCanvas2D，资源用qrc别名。
- QCanvasPainterItem由createItemRenderer创建所属render-thread renderer；synchronizeData只传值快照。路径/字体在renderer初始化或真实依赖变化时更新，使用独立painter和有界path group；透明fill开启alphaBlending，sampleCount=1，保留模块几何AA。
- Canvas2D使用`property path2d`/`addPath(svgString)`，不是浏览器Path2D。矩形裁切用setClipRect；绘制前reset/resetClipping，restore后显式resetClipping。通用路径clip不能以矩形能力冒充。
- Canvas2D表面替换时外层保留业务状态/计数，内层Loader重建唯一绘制上下文，path在新上下文初始化一次；主题与静止帧不重建。JS不使用共享painter的GPU path group；失效用一次Qt.callLater合并。
- 两种API均只消费既有控制器与唯一FrameClock，隐藏不发布无效状态；缓存按路径/样式/字体/尺寸/DPR失效。paint/同步不得IO、解码、等待、GPU finish或读回。

## 选择与资源

- 同组件同内容/字体/路径/主题/尺寸/DPR/时刻/AA比较源码语义、结构、实际质量、成本及所有权；许可类别/技术预览标签不作为排除理由，不抄官方benchmark或用QPainter成绩替代实际GPU路径。
- 检查描边/端点/连接、曲线、渐变/遮罩/裁切、透明预乘/颜色域、基线/shaping/emoji；不支持的效果组合Quick合成或保留更合适路径，不能擅自近似。
- Qt管理的RGBA8单采样离屏附件再透明合成进Quick；无mip/resolve/业务封面上传。颜色4bytes+depth/stencil8bytes保守上界按实际物理尺寸计，resize旧/新重叠、字体图集/内部buffer/driver另列。生产使用纳入ResourcePlan；比较探针不计为产品资源。
- 静态与恢复验证从实际提交纹理读回并核对revision，单个在途请求按MaxAsyncReadbackFrames+1有界排空。Canvas的grabWindow可触发重绘/清空命令，不能用它证明原提交；不得为验证引入持续重绘。
- GUI准备、同步、paint、Quick提交与Canvas2D脚本时间分开；Date.now低分辨率的0不是零工作。关闭QCPAINTER_DEBUG_COLLECT，避免模块诊断计时器驱动无效绘制；正式统计解释见[performance.md](performance.md)。

## 检查入口

`ui-audit 输出目录 [--representative]`使用360×160固定内容，覆盖两API和Quick的路径/文字、透明合成、静态零重绘、420×200恢复、十次Loader卸载/renderer释放。保留实际DPR、revision、资源/计数与QML/shader错误检查；跨API和恢复图像评分已移除；自身纹理用于内容及实际质量检查，结构/数值、功能和资源分别断言。R20的实际序章接入另有退出条件，不能用探针替代产品交付。
