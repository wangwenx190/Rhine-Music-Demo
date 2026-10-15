# 总览：源码、状态与验证契约

所有外观以迁移计划置顶规则为准：冻结源码及其生产调用是绝对基准，目标是不含渲染瑕疵的理论效果。旧实机图仅作大致参考，禁止逐像素比较、SSIM/RGB/边缘带追分或据图调参。总览仍须在紧接R19的历史源码复查中逐项复审，不能因已有测试通过豁免。

## 来源与状态

- 冻结来源标识为`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，通过`tools/review/frozen-git.mjs`及来源映射读取。算法来自`music-overview.ts`、`music-overview-edge.ts`、`music-overview-ui.ts`、`music-real-columns.ts`及`scene.ts`实际取景/投影路径；布局和时序来自生产`music.css`与对应UI调用。
- `OverviewController`必须在`SelectionController`之前接入唯一`FrameClock`，先推进rate=6.5的临界阻尼progress，再供真实列居中、实体折叠和相机读取；在Selection/SceneAdapter/Presentation本帧状态之后同步标签和界面。dt已按MotionController缩放，不能重复乘速度。
- 总览取景为span=`max(16.8,17.5/aspect)`、distance=172、yaw=28°、elevation=43°、aim=(0,-1.8,-1.5)。按原方向归一化混合、距离/span/aim插值，再进入既有rate=9相机平滑，保留pointer顺序。
- extent=`1-smoother(progress/.65)*(1-smoother(4.5-abs(lane-center)))`。只缩放已旋转盒体的local Y，用旋转后的底边0.175补偿保持底边固定；不缩放宽/深，不让折叠矩阵污染光照旋转。extent≤.12在逆矩阵/射线测试前排除。
- `projectColumns()`只枚举中心±4.5内的物理列并映射真实ID，不扫描全库；闭区间边界最多枚举十个整数，其中两端extent为零。标签准入最多九候选，选中/展开优先，保留.06/.12滞回和原边界/碰撞规则。
- 固定十八个模型槽容纳九个活动与九个退场标签。锚点直接跟随投影，不给X/Y加追赶动画；内部卡片使用460/240ms进出、420ms入口宽度与320ms透明度。退出卡片立即停止交互并继续跟随当前位置/extent，完全退场才释放。
- 第一击只展开入口，第二击调用既有`enterColumn(lane,true)`进行引导定位并退出总览。V、Esc、返回按钮及点击专辑统一回近景；仅在progress<.02、archiveReady及场景settled后恢复浏览内容与有效控件焦点。任何路径都不打开详情或改变播放。
- 标签字号/留白/边界按源CSS；字体行框通过原生FontMetrics与原结构计算，不能用截图反推。窄屏断点≤600使用116px卡片，宽屏160px；碰撞函数自身的源码断点/缓冲独立保留。

## 资源与性能边界

- 相机/折叠只修改现有实例和frame，不重建完整渲染图、封面图集或不变材质。SceneAdapter发布一次view-projection乘积供所有标签复用，extent按物理列/center/progress缓存。
- 标签碰撞最多90次比较，与库大小无关。标签投影/模型更新在场景revision、输入和动画均不变时跳过；通知合并为帧末状态。标签文字由Qt按内容/字体/宽度变化失效，不每帧重新创建委托。
- 背景保留源设计的12逻辑px高斯模糊；使用可分离sigma12/radius36离散核，预计算权重及相邻线性采样配对，每轴37次fetch。采样现有三维输出的局部区域，文本独立保留输出DPR；不能为消除旧图模糊而取消有意背景模糊，也不能把整层低分辨率套到文字上。
- 每张卡片预留入口76px和两侧36px核边界；两个RGBA8逻辑网格纹理最大308×129，展开动画不改变目标尺寸，垂直pass直接合成。十八槽两张纹理载荷上界5,721,408 bytes；保守计每目标8-byte depth/stencil为17,164,224 bytes，resize旧/新同时存活按34,328,448 bytes计。此为描述符估算，Qt内部padding、字体图集、driver实际分配未知，不能称为显存实测。
- Loader随退场完成释放局部资源；库快照替换清空总览/身份。隐藏时共享时钟暂停且恢复不追赶旧dt；没有新增定时器、worker、无界输入历史、解码或帧内I/O。

## 可复现检查

Release自动发现最新原生Kit，不在公共配置固定Qt版本/路径。正常构建后执行：

```sh
cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint
ctest --preset qt-release --output-on-failure
ctest --preset qt-release-render -R overview-qml --output-on-failure
```

`overview`直接核对冻结源码执行生成的数学输入：690个运动x/v/a样本、390个折叠输入、54组碰撞、54组生产投影和63组真实相机插值；另检验两步进入、折叠底边/宽深、拾取、隐藏、减少动态及反转。数值门槛为轨迹≤1e-6、投影≤2逻辑px，非截图位置测量。

生成数学输入需要旧项目已有开发依赖，仅为维护工具，产品不依赖Node：

```sh
node tools/review/export-overview-golden.mjs
```

`overview-qml`在当前macOS使用真实隐藏RenderControl、九尺寸×双主题×收起/展开共36个状态，检查结构、碰撞、资源和warnings。匿名高频背景在本地生成，作为模糊/合成质量观察输入，无原项目截图或图像评分；仅在显式`--lifecycle`时十次实际销毁/重建QML根和渲染资源，控制器继续存活；普通组件回放不重复压力序列。输出保持忽略。该完整组件回放属于显式隐藏渲染组或`qt-release-full`，日常快速门禁不重复执行；其他平台当前入口需原生窗口，按实际分组和R34验证。

实际产品入口为`yalahoo --verify-overview 全新目录`。包装脚本核对真实进程退出、报告、实际后端/DPR；普通功能使用`--runs 1`，三次独立运行只在正式成本/性能或具体不稳定问题复核时选择。报告包含Qt事件、SceneItem真实提交、上传、固定按钮/歌名和焦点；显式`--lifecycle`才执行十次窗口surface释放/重建，默认只显示初始窗口。该压力序列只在相关生命周期边界执行，其他已有效验证上下文可复用。授权轮次可加`--foreground`，否则遵循统一置底/拒绝焦点策略，不自行提升窗口：

```sh
python3 tools/build/verify-overview.py --backend metal --dpr 1 --runs 1 --output verification/macos/overview/example-product
python3 tools/build/verify-overview.py --visual --backend metal --dpr 1 --runs 1 --output verification/macos/overview/example-ui
```

根据真实目标环境选择metal/opengl/d3d11及DPR1/1.5/2；夜间产品用`--night`，UI入口本身覆盖双主题；`--lifecycle`是独立的压力开关，不能用普通回放的零重建结果宣称压力通过。目录必须全新，不覆盖失败样本。可见授权只在该轮有效；结束恢复`YALAHOO_MACOS_VISIBLE_TESTS=OFF`，并真实回归默认`verification-window-policy`。

审计入口`tools/audit/audit-overview.py`用重复`--report 产品report.json`和`--visual UIreport.json`读取本地证据，验证冻结源码hash/核归一化，并逐项输出COMMON-01–05、PERF-01–07与三平台真实调用链/资源边界。审计结论不等同于其他平台实机通过。

## 验收范围与后置任务

本机Release/arm64总览验证区分Metal/threaded与macOS OpenGL/basic、真实产品窗口与隐藏RenderControl。自身窗口读回不表示屏幕扫描或input-to-photon；自注入Qt鼠键事件不表示物理设备输入。短序列记录的提交/同步/GPU字段只解释其实际范围，完整120秒产品性能及预算仍属于R28和后置专项。

Windows/D3D11、Windows/OpenGL以及Linux/xcb/Wayland当前没有原生Kit/窗口管理器/GPU运行环境，不能从共享C++/qsb或macOS运行推断目标通过。对应源码/平台静态审计及准确复建要求移交R34的R19-P；目标质量、物理输入/焦点、DPR/恢复与平台集成须真实执行。保留源码/数学/功能/质量/资源门槛，不引入任何原图逐像素验收。


## 本机R19最小验证结论

- Release/arm64、自动发现的Qt 6.12.0：源码运动最大误差1.5099e-14，相机2.46139e-14，生产投影0.000631929逻辑px；折叠、碰撞、两步进入及拾取回归通过。此为数学/代码验证，没有原图比较。
- 最终隐藏UI在Metal/OpenGL的实际DPR1/1.5/2六上下文各运行一次，均覆盖36状态、零QML warnings；Metal/DPR1显式完成十次实际QML根/RenderControl资源重建，其余上下文不重复压力序列。
- 普通单窗口模式实际通过OpenGL/DPR2总览及Metal/DPR1详情：总览cycles=0，详情fullWindowRebuilds=0且lifecycleStressRequested=false。改为显式压力开关前的同一产品逻辑已在Metal三DPR、OpenGL/DPR1及DPR1.5完成有效前台生命周期回放；停止的重复队列不作为完整批次通过，按具体有效报告复用，不重复显示窗口。模式改动只改变验证驱动，未再改相机/布局/折叠/标签/材质实现。
- `qt-release-full`的28项全部通过，实际46.91秒；同次运行25项fast累计24.49秒、三项真正隐藏集成累计22.40秒。不是单独重复测量的快速preset耗时，也不用于承诺其他设备性能。六项原生窗口测试被明确排除，没有将未选择项记作通过。
- 完整回归后另补半列边界回归：保留十个投影候选直到按优先级/extent筛选，避免环绕时丢失排序末尾的选中ID；标签准入仍≤9、碰撞≤90次。受影响overview/source-format及隐藏overview-qml单独重跑，不重复其他矩阵或显示窗口。
- check-format、source-format及两组官方qmllint通过；COMMON/PERF与三平台静态审计通过，目标系统实机仍按R34/R19-P。最终Review、可见测试和性能记录开关均OFF，默认隐藏窗口策略真实通过；没有提交任何运行输出。
