# 序章、渲染准备与交接

本文负责StartupCoordinator、OpeningController、Opening.qml和OpeningInk；品牌几何/弧长归branding.md，唯一帧源归motion.md，生产RHI资源归rhi-integration.md，窗口与后端执行范围归test-gates.md和迁移计划。

- `App.qml → MusicStage.qml`为唯一产品舞台，隐藏Review也实例化同一MusicStage。OpeningController消费FrameClock未缩放dt，按Preparing→Film→Reveal→Done驱动；准备、暂停和后台等待不消耗片长。重播先归档详情/总览，保留同一SceneItem、稳定选中ID与媒体会话。
- StartupCoordinator在GUI持有一个最新待应用库快照；隐藏合并通知，Preparing按事件循环批量应用一致快照。初始模型未就绪不接受scenePrepared；最后一次polish后到达的扫描快照须先应用并重发preparationToken，等待真实封面/上传/渲染重新确认。Film冻结库展示更新，handedOff后才恢复普通交付；协调器不拥有媒体或GPU资源。
- Preparing在底纸下暖真实RHI通道；SceneItem用本次preparationToken、当前submission、全部可见封面/选中高清租约和已排空上传队列确认就绪。旧回调失效，失败沿errorText显示；跳过准备阶段记住意图，待就绪后到达同一终态。
- Film逐式采样冻结musicOpeningFrame与musicBootSceneTime：底纸6.32s消退，6.40s从源时间21.92起片，11.60s到源27.12的静止浏览姿态；相机、重定位波浪、entryZ、选中lift及row-light intro使用同一shot。二维cubic smooth与三维quintic ease各沿对应源函数。Reveal使用源cubic-bezier(.22,1,.36,1)和既有错峰，720ms后一次解锁/恢复有效焦点；途中减少动态直接完成，速度不改片长。
- 完全遮挡期SceneItem停止preparePool/submit；RhiItem同步只收遮挡值，render仅清透明输出，跳过所有三维资源/上传/pass。恢复首次提交携带当前完整快照，即使遮挡期SG重建也重新准备资源；三维render与遮挡clear分别计数。准备暖帧不计作片中三维提交。
- 主页Header的品牌节点始终保留，其他主页组在Film期间不可见/不可交互，交接复用原节点。键盘Tab留在跳过按钮，Enter/Escape跳过，退出后才恢复浏览输入。序章和定位不发媒体命令；设置重播按钮关闭设置面板并调用现有replay接口。
- 品牌/圆环用C++ Canvas Painter：17条中心线及两条完整内纹从qrc SVG/map读取一次，中心线在共享前开启并预建弧长缓存，QPainterPath::trimmed按弧长揭示（[Qt长度缓存](https://doc.qt.io/qt-6/qpainterpath.html#setCachingEnabled)），完整路径按稳定组缓存。逻辑尺寸变化时重建有限路径组，shader只更新纸面颜色/透明度；圆环保留源dash/转角/缩放/静止中心十字。短点用原round-cap笔画的填充轮廓保留，避免模块将极短线当作重合顶点丢弃。
- 缓存描边不带state scale：先把路径和线宽映射到逻辑坐标，保持AA fringe按实际DPR生成。原SVG画布contain在原310×145子槽，mark含17px字幕间隔；响应式标语沿源CSS，三维portrait边界仍为1.05。
- 跳过按钮保留源padding/veil/border/focus及blur(10px)。背景按局部区域先合成同源径向底纸和SceneItem，再用归一化sigma10/radius30高斯两pass；纹理仅按钮加60px边距，随序章退出释放。Canvas RGBA8单采样色彩加depth/stencil保守上界、局部模糊附件计入ResourcePlan，实际driver/字体图集另列。

REFDB-02仅采用渲染入口遮挡早退的思路，未复制对方代码或素材；参考来源MIT、作者LBEILC，读取hash/符号保存在本地审计。

## 入口

- `node tools/migration/export-opening-golden.mjs`从映射后的冻结Git blobs执行序章函数和生产相机片段，导出最小数学测试向量；`--check`核对同源可复现性。
- `startup`/`startup-qml`的集成及按需诊断入口见[performance.md](performance.md)。
- `ctest --preset qt-release -R '^opening$'`核对逐参数/相机≤1e-6、20/30/60/120fps与0.25×/1×/3×、就绪/跳过/重播/后台/减少动态、品牌弧长与错峰终点。
- `ui-tests --opening 输出目录`/`opening-qml`实例化生产MusicStage；按test-gates.md分别覆盖九尺寸布局、双主题及有效DPR边界、实际Logo/圆环/标语、一次完整片头/交接、零遮挡三维提交、键盘焦点/跳过。性能/资源先按performance.md静态审查，仍有实际运行未决项才最小观测恢复或局部成本；组件回放不作为正常VSYNC/物理屏幕或完整产品性能证明。图像只为自身正常输出质量检查，原始报告/图像均留本地。
- 该入口的`--representative`保留日间DPR1九尺寸，其余主题/DPR仅选择1280×720和390×844；报告列出实际组合。在轮廓揭示与三维露出两个独立阶段各核对一次SG释放/恢复、暂停时间与首个恢复间隔；检查实际三维提交及绘制器释放。
- 目标系统、后端与物理输入的未验项唯一由迁移计划R36维护。
