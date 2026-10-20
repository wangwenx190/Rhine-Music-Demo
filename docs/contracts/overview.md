# 总览状态、投影与局部合成

本文负责OverviewController/标签与折叠；公用相机和帧源分别见scene.md/motion.md，欠验归迁移计划R19-P（R34/R35按后端分工）。

## 来源与状态

- 冻结来源标识为`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，通过`tools/review/frozen-git.mjs`及来源映射读取。算法来自`music-overview.ts`、`music-overview-edge.ts`、`music-overview-ui.ts`、`music-real-columns.ts`及`scene.ts`实际取景/投影路径；布局和时序来自生产`music.css`与对应UI调用。
- `OverviewController`必须在`SelectionController`之前接入唯一`FrameClock`，先推进rate=6.5的临界阻尼progress，再供真实列居中、实体折叠和相机读取；在Selection/SceneAdapter/Presentation本帧状态之后同步标签和界面。dt已按MotionController缩放，不能重复乘速度。
- 总览取景为span=`max(16.8,17.5/aspect)`、distance=172、yaw=28°、elevation=43°、aim=(0,-1.8,-1.5)。按原方向归一化混合、距离/span/aim插值，再进入既有rate=9相机平滑，保留pointer顺序。
- extent=`1-smoother(progress/.65)*(1-smoother(4.5-abs(lane-center)))`。只缩放已旋转盒体的local Y，用旋转后的底边0.175补偿保持底边固定；不缩放宽/深，不让折叠矩阵污染光照旋转。extent≤.12在逆矩阵/射线测试前排除。
- `projectColumns()`只枚举中心±4.5内的物理列并映射真实ID，不扫描全库；闭区间边界最多枚举十个整数，其中两端extent为零。标签准入最多九候选，选中/展开优先，保留.06/.12滞回和原边界/碰撞规则。
- 固定十八个模型槽容纳九个活动与九个退场标签。锚点直接跟随投影，不给X/Y加追赶动画；内部卡片使用460/240ms进出、420ms入口宽度与320ms透明度。退出卡片立即停止交互并继续跟随当前位置/extent，完全退场才释放。
- 卡片完全退出后复用槽位时清零该列入口宽度/透明度状态；退出未完的同列反转保留显示状态。卡片退出早于入口收拢，不能让新列继承旧入口残值。
- 总览入口只在收藏架交互状态显示；详情打开、切换与返回途中遵守源`data-mode="detail"`的visibility隐藏，不能与详情返回按钮重叠。
- 第一击只展开入口，第二击调用既有`enterColumn(lane,true)`进行引导定位并退出总览。V、Esc、返回按钮及点击专辑统一回近景；仅在progress<.02、archiveReady及场景settled后恢复浏览内容与有效控件焦点。任何路径都不打开详情或改变播放。
- 标签字号/留白/边界按源CSS；字体行框通过原生FontMetrics与原结构计算，不能用截图反推。窄屏断点≤600使用116px卡片，宽屏160px；碰撞函数自身的源码断点/缓冲独立保留。

## 资源与性能边界

- 相机/折叠只修改现有实例和frame，不重建完整渲染图、封面图集或不变材质。SceneAdapter发布一次view-projection乘积供所有标签复用，extent按物理列/center/progress缓存。
- 标签碰撞最多90次比较，与库大小无关。标签投影/模型更新在场景revision、输入和动画均不变时跳过；通知合并为帧末状态。标签文字由Qt按内容/字体/宽度变化失效，不每帧重新创建委托。
- 背景保留源设计的12逻辑px高斯模糊；使用可分离sigma12/radius36离散核，预计算权重及相邻线性采样配对，每轴37次fetch。采样现有三维输出的局部区域，文本独立保留输出DPR；不能为消除旧图模糊而取消有意背景模糊，也不能把整层低分辨率套到文字上。
- 每张卡片预留入口76px和两侧36px核边界；两个RGBA8逻辑网格纹理最大308×129，展开动画不改变目标尺寸，垂直pass直接合成。十八槽两张纹理载荷上界5,721,408 bytes；保守计每目标8-byte depth/stencil为17,164,224 bytes，resize旧/新同时存活按34,328,448 bytes计。此为描述符估算，Qt内部padding、字体图集、driver实际分配未知，不能称为显存实测。
- Loader随退场完成释放局部资源；库快照替换清空总览/身份。隐藏时共享时钟暂停且恢复不追赶旧dt；没有新增定时器、worker、无界输入历史、解码或帧内I/O。


## 检查入口

- `node tools/review/export-overview-golden.mjs`生成独立源码数学；`overview`覆盖690运动、390折叠、54碰撞、54生产投影、63相机插值及两步进入/底边/宽深/拾取/隐藏/减少动态/反转。轨迹≤1e-6，投影≤2逻辑px。
- `overview-qml`的源状态集合为九尺寸×双主题×收起/展开36状态；布局/主题/DPR执行范围按test-gates.md选择，必要入口调整按test-gates.md。核对锚点/卡片/文字框、两张局部纹理实际尺寸、退场释放和窗口策略，代表交互状态验证实际Qt鼠标第一击只展开、第二击引导进入；没有准入标签时记录空集合，不伪造点击。`--lifecycle`才独立做十次真实QML根与渲染资源重建，控制器存活。匿名高频背景为自身质量输入，不是源图评分。
- `node tools/review/export-browse-ui.mjs --overview --overview-native 原生report.json`从冻结CSS/生产DOM导出九尺寸三DPR双主题卡片结构，并直接执行冻结`visibleOverviewLabels`检查原生投影输入的准入集合。`python3 tools/review/compare-overview-structure.py 原生report.json verification/overview-source/report.json 输出.json`核对全部卡片框≤1逻辑px及源准入；独立运动/投影golden仍保留，不能以结构匹配代替正常输出质量。
- `python3 tools/build/verify-overview.py --backend <目标后端> --dpr 数值 --runs 1 --output 全新目录`运行产品；`--visual`检查UI，`--night`产品夜间，`--lifecycle`另验窗口surface压力，`--foreground`仅授权时使用。
- `audit-overview.py`接受重复`--report`与`--visual`报告，核对源hash/核归一化/真实事件、SceneItem提交、按钮/歌名/焦点和资源。正式采样与窗口政策统一由公共契约规定。
- `audit-detail-overview-source.py`维护R19G1冻结/实际历史与当前调用链、COMMON/PERF/三平台矩阵。`audit-overview.py`可仅传可靠隐藏`--visual`证据；这不产生缺失的原生产品、焦点或呈现证明。
- 标签投影保留半列边界十个候选直到优先级/extent筛选，准入≤9、碰撞≤90，不能因提前截断丢失选中ID。
