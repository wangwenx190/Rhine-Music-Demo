# Qt Quick / QML 原生迁移执行计划

## 项目文档优先级

- 本计划是项目内最高优先级的实施契约；`AGENTS.md`、`DESIGN.md`、README、贡献指南及其他计划、版本文档与其冲突时，以本计划为准。未冲突的约定继续适用。
- 新的用户明确要求优先于已有项目文档；长期范围、技术路线或验收要求发生变化时，同步修订本计划及相关约定。
- 历史 Web 实施和验收文档提供设计、参数与验证参照，其中的旧任务、旧授权、音频路线和发布检查不自动适用于本次原生迁移。

## 文件维护规则

**【已经完成的轮次直接移除，不允许输出过程性文字到这个文件中，除非是需要跨轮次传播的信息或者执行的任务】**

- 本文件是剩余任务清单与跨轮次实施契约，不是开发日志。禁止加入“本轮完成了”“正在处理”“检查通过”、完成勾选、流水账、聊天记录和历史进度表。
- 一个轮次的交付物及退出条件全部满足后，删除该轮次整个小节。编号保持不变，不重新编号；依赖编号指向代码、测试和验收资产，不要求被依赖的小节仍存在。
- 完成轮次涉及的接口、参数、资产路径、构建命令及固定限制，合并到本文件对应的长期契约，或保存到代码和 `docs/qt/contracts/`；不得用保留已完成轮次代替交接。
- 未通过的验收项保留为具体任务：写清输入、修复对象、预期结果、验收方法和依赖。禁止用“继续优化”“自行研究”“视情况调整”作为任务。
- 测试输出、截图、性能样本和差异报告分别放到 `verification/qt/`、`docs/media/qt/`，只在本文件保存必要引用。不能将私人曲库、个人配置或绝对音乐路径放入版本控制。
- 依次执行剩余且依赖已满足的轮次。一次执行以一个轮次为边界；不得为赶进度跳过视觉门槛或把跨平台未运行的检查写成通过。

## 目标、范围与固定取舍

1. GUI、布局、动画、相机、材质、灯光和特效尽最大可能逼近当前 Web 音乐入口；静态相似和动画连续性分别验收，不能只凭截图宣称复刻。
2. 原生应用使用 C++20、Qt 6.12、Qt Quick/QML、Qt Quick Controls 和 Qt Quick 3D；CMake 构建。QML 承担界面，C++ 承担模型、状态机、运动算法、文件扫描与控制器。
3. 最终运行不依赖 Node、浏览器、WebEngine、本地 HTTP 服务、Swift 桥接、外部 FFmpeg 命令或其他第三方库。Qt 自带的插件及其内部依赖由 Qt 部署工具部署，不自行链接其内部 FFmpeg 库。
4. 音乐定位为普通播放器：播放、暂停、停止、上一首、下一首、专辑队列、进度、音量、输出设备和错误提示。全部使用 QtMultimedia 公开 API。
5. **不实现无缝播放、交叉混音、手动 PCM、重采样、音频回调、音频环形缓冲、自建音频引擎或专业输出。** 不引入 `QAudioSink`、`QAudioDecoder`、`QAudioBufferOutput`、AVAudioEngine/CoreAudio 自建路线。
6. 歌曲衔接改为普通顺序换曲；删除原三档衔接和浏览器/CoreAudio 后端选择。保留原播放按钮位置、歌曲标题和播放定位行为。音频简化不降低 GUI 复刻标准。
7. 联网能力本阶段不执行，但保留异步接口、来源模型、状态占位、配置扩展和可替换 Provider。禁止删掉以后接入联网功能所需的数据字段；不实现外部服务协议，不发送请求。
8. 暖昼/深夜、减少动态效果、0.25×–3× 动效速度、横竖屏/短屏/超宽屏、填充与真实列、列记忆、搜索不改变播放等行为保留。
9. 不迁移旧档案终端 `?original=1`、PWA、浏览器专用菜单限制、在线播放器代理和旧拆解研究入口到产品主界面。原文件保留作为历史及参照；原生产品内的详情拖动查看、画质和光效调节保留。
10. Windows 为本机开发与首轮验收平台；macOS、Linux 为必须支持的平台，分别构建和验收。移动端不属于本计划。
11. 本阶段只编写与执行本地改造，不自动发布、上传私人数据、改写发行标签或删除 Web 基线。

## 跨轮次基线与工具链

### 基线与参数优先级

- Web 基线提交：`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，产品版本 V0.4.1。后续工作树中的新改动不能自动改变基线。
- 产品语义首先以本计划的固定取舍为准；AGENTS.md、DESIGN.md 顶部和 V0.4.1 专项文档仅补充未冲突的约定。渲染与时序数值取冻结提交实际执行路径的最终值。
- 依据文件：`DESIGN.md`、`docs/MODEL-V0.4.0.md`、`docs/MOTION-V0.4.0.md`、`docs/MOTION-REVIEW-2026-10-07.md`、`docs/REAL-COLUMNS-V0.4.1.md`、`docs/COLUMN-POSITION-V0.4.1.md`、`docs/OVERVIEW-V0.4.0.md`、`docs/STARTUP-V0.4.0.md`、`docs/TITLE-CLIP-V0.4.1.md`。
- 不能用历史截图代替当前运行基线。文档与运行值冲突时，保留文档设计意图，导出最终运行参数作数值依据。例如当前 `music-model.ts` 中侧脊基础/暖昼粗糙度为 0.25/0.30、厚度为 0.06/0.08，不直接照搬模型日志中的旧表格。

### 工具链契约

- Windows Qt Kit：`C:/Qt/6.12.0/msvc2022_64`，qmake 报告 6.12.0；本机 VS C++ 工具路径为 `C:/Program Files/Microsoft Visual Studio/18/Community`。以 Kit 配套 ABI 和实际编译探测为准，不因为目录名 msvc2022 就切换到不存在的 VS 安装。
- 本机 CMake/Ninja：`C:/Program Files/CMake/bin/cmake.exe`、同目录 `ninja.exe`。使用 `vswhere.exe` 定位带 VC.Tools.x86.x64 的工具链，构建脚本导入 VsDevCmd 环境，不修改全局 PATH。
- Qt 模块：Core、Gui、Qml、Quick、QuickControls2、QuickDialogs2、Quick3D、Multimedia、Concurrent、Svg、ShaderTools；测试使用 Test、QuickTest。上述关键 Kit 模块存在；实际构建仍需 R02 的链接及运行探测。
- `CMakeLists.txt` 在根目录；应用源放 `qt/`，构建目录 `build/qt/`，部署目录 `out/qt/`。本机路径只写入被忽略的 `CMakeUserPresets.json` 或环境变量，不写死到跨平台 CMake。
- 创建统一 preset 名称：`qt-debug`、`qt-release`，不同系统用各自 UserPresets 提供 Qt 前缀和编译器环境。统一命令：`cmake --preset qt-debug`、`cmake --build --preset qt-debug`、`ctest --preset qt-debug --output-on-failure`。
- Web 参照可以使用仓库已有且锁定的开发依赖；不得为 Qt 应用新增第三方运行库、下载新浏览器或引入新的 npm/Python 图形比较库。

## 架构与数据契约

### 目录及职责

```text
qt/
  app/          main、命令行、应用生命周期
  core/         LibrarySnapshot、Album、Track、排序/搜索、偏好、选择控制
  motion/       FrameClock、阻尼、五次轨迹、相机与阵列运动
  render/       SceneAdapter、实例表、模型、材质、图集、后处理
  media/        PlaybackController、BackgroundMusicController、InterfaceSoundController
  library/      扫描、Qt 元数据探测、缓存、旧数据导入
  services/     Provider 接口、OfflineProvider、资料缓存模型
  qml/          App、Header、Browse、Detail、Overview、Opening、面板和基础控件
  assets/       转换后的字体、模型、图标、音频和 shader
  tests/        QtTest、QuickTest、GUI 场景重放与图像比较
tools/qt/       构建、资产导出、验证、部署脚本
docs/qt/contracts/  参数、接口、源模块映射和固定平台差异
verification/qt/    自动化输出及机器可读对照结果
docs/media/qt/      非私人视觉基线与对照图
```

### 数据流与线程边界

- `AppController` 组合 `LibraryStore`、`SelectionController`、`PresentationController`、`MotionController`、`PlaybackController`、`PreferencesStore` 与资料 Provider；不建立替代 `music-app.ts` 的单个巨型类。
- 导航状态与播放状态独立：选择专辑/搜索定位/总览定位只改变导航。只有明确播放命令和当前曲目结束事件改变播放队列。
- 所有公开操作按稳定 `albumId`/`trackId`，不把列内序号、ALBUM 编号、代理模型行号当成持久 ID。
- `AlbumListModel`、`TrackListModel`、`SearchResultsModel`、`OverviewLabelsModel` 使用 QAbstractListModel；所有 begin/end 更新及 QML 属性通知在 GUI 线程。
- 扫描目录、QImage 解码、缓存读写使用受限工作线程；QMediaPlayer 探测对象放在有事件循环的专用线程，跨线程传回值对象，不在普通线程池任务中创建后等待异步媒体对象。
- GUI 线程向渲染层交付不可变 `SceneFrame` 快照：revision、camera、theme、quality、instances、selected、returning、rowLight。渲染线程不直接访问 QML/LibraryStore，不做磁盘或网络 IO。
- 每个异步任务包含 `{requestId, generation, entityId}`；发布结果前核对代次、当前实体及销毁状态。取消、扫描替换、重建、关闭必须使旧结果失效。
- 使用 Qt RAII、QObject 父子关系、QPointer 与可取消任务。GPU 资源在所属渲染线程释放；已销毁对象的晚到结果只能丢弃。

### 资料与联网扩展接口

- 保留原 `MusicAlbum` 的 description/descriptionSource、introduction、producers、creditsLookup、online、localNote；保留原查询配置字段，未知 JSON 字段读入后写回。
- `IMetadataProvider` 固定异步方法：`requestIntroduction(AlbumQuery, RequestToken)`、`requestCredits(AlbumQuery, RequestToken)`、`requestEnrichment(AlbumQuery, RequestToken)`、`cancel(RequestToken)`。以回调/信号交付 `ProviderResult`，不得让 GUI 等待同步返回。
- `ProviderResult` 固定包含 requestId、generation、albumId、kind、status、payload、source、checkedAt、retryAt、error。状态包括 Deferred、Matched、Partial、NotFound、Uncertain、Error、Cancelled。
- 本阶段唯一实现是 `OfflineMetadataProvider`：从本地缓存读取已有内容；无缓存返回 Deferred，并显示“联网资料功能暂未启用”。这不等同于 NotFound，不写入失败重试时间。
- Provider 通过构造注入或注册工厂接入，不通过 QML 判断平台/服务名称。设置保留“专辑介绍”“制作信息”“MusicBrainz”入口及状态占位；联网执行按钮禁用，来源链接显示文本且本阶段不自动打开。
- 未来 Provider 能接收联系人、开关、批量 albumIds、force 和取消令牌；预留 ProviderCapabilities，但本阶段 capabilities 统一 networkEnabled=false。全应用不创建联网请求，离线测试断言零请求。

### 存储与本地资料

- 默认数据根为 `QStandardPaths::AppLocalDataLocation`；`--data-dir` 可显式覆盖。配置、索引、封面、偏好各自分文件；偏好存 `preferences.json`，列位置仅存在会话内存。
- 原 `../music-data-v3` 只作为显式导入来源，不自动与 Web 共写。导入配置/索引/封面/本地备注/来源信息到新的数据根，原目录只读；不导入旧 PCM 缓存。
- JSON 使用 QJsonDocument、QSaveFile 原子提交；异步保存串行化。损坏文件保持原件并显示错误，不默默覆盖。源歌曲永不写回标签。
- ID 保留原规则：专辑 `album-` + SHA256(原生绝对目录或直属单曲路径 UTF-8) 前 24 位；曲目 `track-` + SHA256(原生绝对文件路径 UTF-8) 前 24 位。同机导入保留原 ID；跨系统路径不能自动猜测映射。
- 排序使用 Qt QCollator，zh_CN、numericMode=true、CaseInsensitive；NFKC、trim、合并空白，等价时按规范化字符串和 ID 稳定排序。不同系统的排序器可能与浏览器 ICU 有差别；固定视觉 fixture 使用冻结的列顺序，不引入 ICU/OpenCC/TagLib 弥补差异。
- 新元数据读取限于 QtMultimedia 的 QMediaMetaData；缺失信息保留 unknown/空值与现有占位，不猜测无损、位深、采样率或制作角色。已有本地索引中的丰富字段继续显示。

## 视觉、运动与渲染实施契约

### 布局与资源

- 使用真实窗口内容尺寸与 Qt 逻辑像素；不把整个应用缩放为固定 1920×1080 画布。电影式开场的参考坐标独立处理。
- 宽高比 `<1.05` 为竖屏，等于 1.05 为横屏；相机与 QML 共用一个 LayoutMetrics 对象。保留 1100/700/480 等样式密度分界，并从基线导出实际规则。
- 横屏专辑信息 x=68.633333% 屏宽、右边距=max(24px,2.2% 屏宽)；短屏内容自行滚动。竖屏品牌与控件两行，返回按钮固定品牌下方。
- 横屏主页相机下移由 16:9 的 8% 连续过渡到 21:9 的 10%；向左偏移由 0 到 3.5%，更宽保持终值；详情和竖屏不套用此偏移。
- 字体先用 Qt 加载现有 MiSans WOFF2 并逐个字重验证；若字体后端不支持，固定使用 Windows 系统 DirectWrite 的 `IDWriteFactory5::UnpackFontFile` 解包仓库四个 WOFF2 为原始 SFNT，再由 Qt 加载同一组可跨平台使用的字体。资产工具仅链接系统 Dwrite.lib，读取返回的 IDWriteFontFileStream，以 Qt QFile/QSaveFile 保存；根据 SFNT 签名选择 .ttf/.otf，保留字形/字重/字距数据及输入hash。转换只在准备资产时执行，不进入产品运行路径；禁止新增字体转换库、下载替代字体或用相近系统字体冒充。
- 图标/品牌路径提取为 SVG 或 Qt Quick Shape，保留 viewBox、描边宽度、端点与颜色；不替换成系统主题图标。
- GLB 保留原资产，用 Kit 自带 balsam 导入或通过 Qt 自带导入器读取；若某 glTF 特性导入丢失，写窄用途 C++/Qt 解析器读取本项目 GLB 的 JSON/BIN、POSITION/NORMAL/TEXCOORD/indices，不引入 Assimp 等库。

### 连续运动

- 时间单位统一秒，核心计算 double。唯一 FrameClock 接收 QML FrameAnimation 的帧间隔：`dt=min(max(dt,0),0.05)`；普通动效 `motionDt=dt*speed`，速度只缩放一次。片头和音频不受 speed 影响。
- 临界阻尼原样移植：`d=x-target`，`j=v+r*d`，`e=exp(-r*dt)`；`x=target+(d+j*dt)*e`，`v=(v-r*j*dt)*e`。普通真实列 rate=3.7，详情换片 rate=9，总览 rate=6.5。
- 搜索/跨列远记忆/归中按现有五次多项式接续 x/v/a，终点 v=a=0；时长 `1.4+clamp((hypot(lanes,rows/8)-1)/7,0,1)` 秒。系数直接移植 `music-archive-motion.ts`、`music-real-columns.ts`，不能用 Qt SpringAnimation 默认参数替换。
- 同目标不重启轨迹；坐标 rebase 只平移位置与多项式常数项，时间、速度、加速度不变。减少动态效果直接完成目标并处理回调/焦点，不能只把 duration 设成 1ms。
- UI 曲线采用原 cubic-bezier，通过求解横坐标获取 progress，不把 CSS 控制点直接解释为均匀参数 t。改速从当前进度继续，不重新设置起点。
- 表面进入/退出采样当前 opacity/transform；被打断时基础时长 `max(72ms,baseDuration*sqrt(opacityDistance))`。右侧进入 24px/退出 32px，上下进入 10px/退出 6px；有独立图层的面板分别过渡。
- PresentationPhase 保留 archive/opening/detail/switch-hiding/switching/hiding/returning/selecting；退浏览文字后才能抽取，盒体和相机落稳后才能进详情字。退场中只保留最后目标，旧完成事件按 revision 丢弃。
- 字轮基础 460ms，按当前显示位置和速度重定向；按 Unicode grapheme 分段，不能按 UTF-16 单元拆 emoji/组合字。QTextLayout 的整行 shaping 位置用于文字测量，不能逐字符独立测宽破坏字偶距。

### 几何、材质、封面与光场

- 盒体 4.45×3.35×0.14，中心 (0,1.85,0)；封面纸面 3.06×3.06，中心 (0.17,1.85,0.027)。后托前面 -0.032，前盖背面 0.052、正面 0.070；不能共面，不能扁平化为无厚度卡片。
- Three 与 Qt Quick 3D 都按右手、Y 向上处理；用显式矩阵和 camera aim 对齐，不全局翻转 Z。导入节点变换只应用一次，先以已知顶点验证坐标再加入动画。
- 模型分 Frosted_Polymer、Ivory_Edges、Optical_Diffuser 三壳体批次，封面单独批次。复制项目已有几何，不重新凭视觉建模。
- 主盖透射基础/暖昼 0.96/0.94，粗糙度 0.34/0.40，厚度 0.018/0.022，IOR 1.46，clearcoat 0.08；详情粗糙度趋向 0.27/0.34。其他最终参数由 R01 导出的 scene-parameters.json 固定。
- 纸面局部距离 `d=max(abs(localXY-(0.17,1.85))-(1.53,1.53))`；`p=1-smoothstep(0.015,0.15,d)`；透射 LOD 因子 `mix(1.0,0.16,p)`。它随实例/抽取盒移动，不能在屏幕固定区域模糊，不能增加噪点掩盖材质误差。
- 纸面为独立漫反射，参与既有光场且不自发光；最终颜色上限限制在原图颜色范围，不能用加白增亮改变封面。透明留白、1/64 内缩、contain 与采样钳制在阵列/选中/归位/查看中共用。
- 光带移植 `music-row-lighting.ts` 和 `music-lighting.ts` 的世界空间函数：rowSpacing=0.62、laneSpacing=5.2、laneRadius=27；横排高斯半高宽按 ln(2)，列衰减按六次幂指数。默认 guided、strength=1.3、width=1.35、offset=0.3、fill=1、transitionDuration=1.1。
- light follow rate=`4.743864518390578/duration`，同向速度限幅后接续；跨列换算旧世界原点，真实列使用独立列原点。详情把行光按原轨迹收退，不新增不受主题控制的白色高光。
- 环境取基线 RoomEnvironment 的方向性与颜色，R01 从现有 Web 渲染路径导出无私人资源的环境输入/样本。Qt 内置 PBR 的强度单位不与 Three 直接等同，按固定环境和 ROI 进行限定标定。

### 首选管线与确定的补救路径

- SceneAdapter 的产品接口固定，不因渲染实现变化修改 QML/导航。首选 Qt Quick 3D View3D + 自定义实例表 + CustomMaterial + Effect；不依赖 Qt 内部渲染类。
- 正常管线：阴影目标 → 不透明背景/纸面/结构与供透射采样的捕获 → 磨砂壳体合成 → SSAO合成 → 景深 → 按画质SMAA → 色调映射/输出；以冻结基线的实际通道顺序和例外为准。QML文字与阅读遮罩在三维合成之后，不参与景深。
- Qt 的 SCREEN_TEXTURE/SCREEN_MIP_TEXTURE 不包含透明材质和使用该屏幕采样的物体；不得假设它会递归捕获其他玻璃。先采用与当前 Web 一致的屏幕空间近似，不要求真实光线追踪。
- CustomMaterial 显式实现局部纸面 LOD、侧脊密度/高光、印刷光场和按主题混合；不只填 PrincipledMaterial 参数就宣布完成。Effect 移植基线 SSAO/Bokeh/SMAA 的有效算法及参数，避免 Qt 默认后处理改变画面。
- 线性渲染与 sRGB 输出分开，盒体跟随原 ACES 曝光；纸面 `toneMapped=false` 的路径单独核对。避免 View3D 与自定义 Effect 重复色调映射。
- 材质标定固定用五个ROI：盖内纸面、盖外留白、侧脊、外框边缘、盒体后方背景；暖昼/深夜、正视/低角度同时计分，不能只优化单张图。损失为所有ROI等权的RGB平均绝对误差，加上轮廓平均距离/255；先通过几何门槛再开始标定。
- 先校准光强单位：每类Qt灯相对导出强度尝试10^-4至10^4的十倍间隔倍率，按固定ROI损失选单位换算因子，暖昼/深夜共用因子，不能分别作弊。随后采用确定性坐标搜索：按曝光/环境强度/灯强度/透射/粗糙度/厚度/衰减距离顺序，依次试当前值、±10%、±5%、±2%、±1%，选跨场景损失最低且不破坏硬门槛的值，相同损失取距基线最近值；完整扫描最多3遍。透射/粗糙度范围0–1、厚度为正，IOR固定1.46、几何/封面纹理/主题颜色不参与搜索。三遍后仍有结构误差按R10触发补救；只剩参数残差时新增具体ROI补修轮次，不无限随机调参。
- R10 若出现无法通过公开 Qt Quick 3D 接口修复的透射捕获、逐实例排序、图集局部上传或通道控制问题，执行 R10A–R10D。不能直接删效果、自动降低默认画质或换成 WebEngine。
- 补救实现是 QQuickRhiItem + Qt 自带 QRhi/qsb：仍为 Qt Quick 原生应用。QRhi 使用 GuiPrivate，版本固定 6.12；不引入第三方渲染引擎，不改 QML 业务接口。只隔离在 `qt/render/rhi/`，不让私有类型泄漏到 core/media。
- 如果首选管线达到目标且无上述结构限制，删除条件轮次 R10A–R10D；不是把它们标记为完成。由实际对照决定是否触发，不由 agent 个人偏好决定。

### 纹理与实例预算

- 填充可见池 9×48=432，COLUMN_SPACING=5.2、ROW_SPACING=0.62；真实列按有效单元显示，数量大于可见池时按当前位置虚拟化，保持唯一逻辑身份。
- QML 不创建每张专辑一套三维节点；共享几何、材质和 C++ 实例表。选中与归位副本拥有独立纹理租约，不能在换片时换错封面。
- 封面后台尺寸最长边 256、选中 1024；同时解码最多 3 张，已完成 CPU 图像 LRU 16MiB。图集使用固定容量分页面，单边不超过 GPU 上限；每格保留采样边界和透明留白。
- 图集背景禁止跨格 mipmap/各向异性，选中纹理保留最多 8×；常态每帧最多上传 8×256² RGBA=2MiB，初始化每批最多 16MiB且让出一帧；稳定帧无重复上传。
- Qt Quick 3D TextureData 更新可能导致整页上传；必须实测。若超过上述预算，触发 R10A–R10D 的局部上传实现，不把 setTextureData 次数当成实际 GPU 上传证据。
- 全分辨率中间目标限制 8,294,400 像素及硬件尺寸；画质配置沿用 render-quality.ts 的范围和预设。限制发生时明确显示，不偷偷保存更低的用户设置。

## 验收契约

### 固定测试数据与可重放输入

- fixture 仅使用内置演示封面与合成/匿名标题：空库、单专辑、两张偶数列、三张奇数列、短列/长列、多于12张列、超过432张库；方形/竖版/横版封面；长中文、英文下伸字符、括号、emoji 和组合字。
- Web 与 Qt 使用同一 JSON 场景输入：viewport、DPR、theme、quality、arrayMode、albumIds、selection、motionTime、输入事件序列。fixture 不读取用户真实 music-data-v3。
- Qt `--review-fixture`、`--review-time`、`--review-script` 仅在 Review 构建启用；冻结时钟可按精确时间采样，不使用睡眠截图代替确定状态。
- 每份视觉产物携带基线提交、fixture hash、Qt/浏览器版本、图形后端、内容尺寸、DPR、画质、时间和 ROI；不能比较不同封面或不同相机状态的截图。

### 尺寸、图像和运动门槛

- 必测内容尺寸：1920×1080、1280×720、995×858、960×900、930×900、390×844、320×720、2560×1080、3440×1440；DPR 1/1.5/2 在代表性尺寸覆盖。平台窗口装饰不进入比较。
- 先验收结构：主锚点误差≤1逻辑像素、三维轮廓/投影锚点误差≤2逻辑像素；布局分界、文本断行、裁切、控件可点击范围必须一致。
- 在同尺寸同输入的 sRGB PNG 上，UI ROI SSIM目标≥0.98；三维关键 ROI SSIM目标≥0.97，平均 RGB 绝对误差目标≤6/255；文字边缘仅允许固定 1px 抗锯齿带单列统计，不能大面积遮掉差异。
- SSIM用 Qt/C++实现11×11高斯窗口σ=1.5，C1=(0.01×255)²、C2=(0.03×255)²；另生成绝对差分图、边缘距离和分区误差。禁止新增 OpenCV/Pillow/scikit-image。
- 数值门槛是验收目标，不是预先保证。图形算法仍有差异时，先按几何→取景→颜色空间→灯光→材质→后处理顺序修复；只允许记录具体、可复现、无法进一步缩小的残余差异，不用一句“肉眼差不多”放行。
- 运动数学对照：double轨迹误差≤1e-6（位置/速度/加速度分别记录）；投影误差≤2逻辑像素；时序事件差≤1个60Hz帧。连续曲线按20/30/60/120fps与长帧输入测试。
- 必测打断序列：打开后立即返回、退字中连续换目标、详情连续三次换片及反向、搜索替换/取消、总览中切列、改速、动画中开启减少动态、扫描替换与窗口跨1.05分界。
- 动画验证除关键帧外，每段以1/120秒采样状态；用同输入生成Web/Qt成对序列，禁止“完成态一致”替代过程一致。

### 性能与平台边界

- Windows参考机60Hz下，原始画质/1920×1080内容窗口，预热后120秒混合交互目标p95帧间隔≤20ms；无资源准备时不得重复出现>50ms卡顿。高刷新率额外记录，不将VSYNC限制误报为性能不足。
- 记录冷启动、稳定、重载、恢复各自分布；CPU提交时间不得称作GPU耗时。资源计数、上传字节、解码并发与帧率分别采集。
- 连续10次界面重建和30次详情切换后，资源回落且无单调增长；隐藏/最小化暂停视觉更新，歌曲继续播放。
- 没有macOS/Linux执行环境时，相关轮次保留；CI配置成功不等于真实图形、字体和音频验收通过。

## 待执行轮次

以下量级为熟悉C++/Qt的开发者有效开发日估算，不等同于聊天轮次时长。每轮只处理一个主要子系统和一个明确集成边界；达不到退出条件时拆出具体补修轮次，不把额外系统一起塞入本轮。

### R01 · 冻结可执行Web基线与视觉/行为数据（1–2日）

- 依赖：无。交付 `tools/qt/export-web-baseline.mjs`、`docs/qt/contracts/source-map.json`、`scene-parameters.json`、`motion-golden.json`、匿名fixture和基线截图清单。
- 读取冻结提交的实际music入口；建立所有生产模块到Qt职责的映射，明确哪些旧档案模块仅为参照。导出最终相机、灯光、材质、CSS变量/计算样式、可见性和时序，不只导出构造默认值。渲染参数必须等待music主题/光照初始化及目标状态落稳后读取。
- 场景对照至少导出位置/方向/FOV/near/far/投影矩阵、各灯世界变换/颜色/强度、环境纹理及曝光、每个表面最终材质、AO/DOF/AA有效参数、实例矩阵和纸面UV；UI导出字体/字重/字号/lineHeight/letterSpacing、边距、宽高、颜色及全部cubic-bezier。缺字段使导出失败，不让后续agent补猜默认值。
- 为阻尼、五次轨迹、光带、阵列rebase、总览、字轮和片头生成带输入事件的数值golden；用已有检查脚本验证导出器调用真实函数。
- 以既有Web环境生成各主题的浏览、详情、总览、片头关键节点及换片序列；缺少浏览器时保留基线采集任务，不用历史图填充。整理RoomEnvironment输入和可复现环境样本。
- 退出：产物能按提交+fixture+命令重建，所有匿名数据来源明确；联网与音频简化例外列在contract，不混入GUI差异。

### R02 · CMake工程、Kit探测与最小原生运行（0.5–1日）

- 依赖：R01。交付根CMake、presets、`qt/app/main.cpp`、最小App.qml、QtTest目标和构建脚本。
- `qt_add_executable`/`qt_add_qml_module`注册`Rhine.Music`模块；加载Qt Quick Controls Basic样式再创建引擎，避免平台主题改变自定义控件。
- 独立探测Qt Quick 3D、ShaderTools、Multimedia链接及插件加载；为Debug/Release配置一致的模块路径，Qt前缀可覆盖。
- 首个窗口验证D3D11渲染、中文文本、QML模块和QMediaPlayer创建；运行失败保留实际诊断，不能仅靠CMake configure成功过关。
- 退出：三个统一构建/测试命令运行；启动不创建浏览器、Node服务或联网请求。现有Web构建不被破坏。

### R03 · 模型、字体、图标与音频资源接入（1–1.5日）

- 依赖：R02。交付 `qt/assets/`、资源清单、`tools/qt/prepare-assets`和资产测试。
- 导入music-case-v040.glb，保留三壳体批次、顶点/法线/UV和纸面间隙；Qt导入输出与原几何包围盒/三角数对照。生成资产保留输入hash，禁止对模型做非均匀缩放。
- 验证四个MiSans字重的Qt加载、glyph覆盖与文字宽度；按布局契约处理不支持的字体格式。品牌/按钮图标逐路径提取，不换图案。
- 复用atmosphere/motif/pulse等现有完整音频资源；界面音效仅接入可直接播放的既有短文件。没有独立短资产的合成音效保留事件映射并静音，不手工生成/裁切PCM，不播放整段typing-preview作为每次按键声。
- 退出：资源均通过qrc加载，应用从任意工作目录启动仍可用；没有新第三方依赖及私人资源。

### R04 · C++数据模型、排序、搜索与离线Provider契约（1–1.5日）

- 依赖：R02。交付 `qt/core/`值类型和列表模型、`qt/services/`接口/离线实现。
- 按music-types.ts保留Album/Track/来源/查询状态；路径作为本地QUrl，媒体可播放性采用Unknown/Supported/Unsupported，不沿用browserPlayable的扩展名猜测。
- 实现独立导航/播放ID、QCollator稳定排序、原搜索字段与空查询规则；用冻结fixture证明排序列分组、搜索结果与模型通知正确。
- 实现OfflineMetadataProvider，异步读取缓存或返回Deferred；缓存命中不会触发重查，无缓存不会写成not-found。
- 退出：QML可读取匿名库，模型角色稳定；搜索/资料请求不调用播放，Provider替换无需修改QML组件。

### R05 · 单时钟、阻尼与五次运动内核（1–1.5日）

- 依赖：R01、R04。交付FrameClock、ScalarSpring、QuinticTrack、MotionController及数值对照测试。
- 逐式移植motion.ts/music-archive-motion.ts/music-real-columns.ts；实现observe/retarget/translate/finish，保留x/v/a，同目标不重启。
- 独立维护开场时钟、普通视觉时钟和媒体状态；改速不重算起点，隐藏恢复丢弃后台间隔，减少动态直接执行完成语义。
- 用20/30/60/120fps、反向/改速/长帧/rebase对照golden，不用近似Euler积分。
- 退出：满足数学门槛，只有一个生产视觉tick源，没有音频时钟或PCM代码。

### R06 · 几何场景、相机投影与LayoutMetrics（1–1.5日）

- 依赖：R03、R05。交付SceneAdapter、SceneFrame、LayoutMetrics、单盒View3D与相机控制。
- 移植viewport-layout.ts/music-camera.ts和scene.ts的有效相机路线；camera position、aim、span与长焦透视一致。Qt矩阵投影与Web已知顶点投影逐点对照。
- 建立标准浏览/详情/竖屏/超宽屏目标，动态过1.05分界；先用无光调试材质排除光效干扰，盒体物理比例不变。
- SceneAdapter输出projectWorldPoint、hitTest、frameRevision；以后总览标签和拾取共用这些函数。
- 退出：所有尺寸下关键投影≤2px，UI与相机同时切换布局；QML不维护第二套相机常数。

### R07 · 单盒主盖、侧脊与纸面材质（1.5–2日）

- 依赖：R06。交付CaseMaterials.qml、自定义材质shader、纸面材质和材质ROI对照。
- 先导入基础PBR参数与固定环境，再实现局部纸面LOD因子、主题插值、详情柔化、单面透射和侧脊独立密度；使用SCREEN_MIP_TEXTURE时按实际mip级数钳制LOD。
- 封面色彩按线性采样/输出转换核对；纸面与盖之间保持真实空间，漫反射不超过源图颜色。提供主盖/侧脊/后托分离调试视图，仅Review构建显示。
- 参数标定只按颜色空间→环境→透射→粗糙度→厚度顺序，每项与基线ROI比较；禁止改模型、封面亮度或加噪点掩盖问题。
- 退出：暖昼/深夜单盒及方/横/竖封面通过结构门槛，残差报告能定位到单层材质。

### R08 · 灯光、世界空间光带与主题连续过渡（1–1.5日）

- 依赖：R07。交付SelectionLighting、RowLightController、主题参数轨迹和光场shader。
- 移植music-lighting.ts/music-row-lighting.ts最终运行值，包含环境、半球/方向光、选中光场与guided默认；Qt光强按R01固定样本标定，不照抄不同单位的数值。
- 光场以实例世界原点计算，抽取和归位纸面使用同一函数；跨列/rebase恢复旧原点后再跟随，普通真实列纵向导航不带动整阵列相机俯仰。
- 昼夜主题650ms从当前颜色/数值继续；阅读层与三维参数共享目标和普通视觉时钟。
- 退出：光带golden通过，重复输入/跨长短列不瞬移，封面交接不闪白，改速只影响一次。

### R09 · 阴影与环境遮蔽通道（1–1.5日）

- 依赖：R08。交付阴影配置、SSAO Effect、RenderQuality模型与分通道截图。
- 依照R01导出路径移植有效SSAO算法；基线kernelRadius=0.38（refined另取0.44）、minDistance=0.001、maxDistance=0.09，最终活动值以导出为准。阴影优先用Qt公开配置；核大小/偏置/投影无法对齐时把具体差异交R10判定。
- 提供通道开关和原始depth/normal/linearColor探针；修复深度范围和FRAMEBUFFER_Y_UP差异，不用调色补偿倒置或非线性depth。基线随机核固定seed，Web与Qt使用相同采样方向。
- 接入原始/性能/高/极高预设和8.3MP上限；此轮不加入景深/SMAA，隔离AO与软阴影误差。
- 退出：单盒与少量邻盒的阴影/AO对照完成，纹理尺寸/格式/深度重建可重放；结构限制形成具体R10输入。

### R09B · 景深、抗锯齿与色彩输出（1.5–2日）

- 依赖：R09。交付Bokeh/SMAA/Output Effect、最终合成顺序和画质开关。
- 移植冻结基线Bokeh采样模式与焦距响应，初始focus=25/aperture=0.0018/maxblur=0.011，最终值用实际轨迹；前景/背景模糊、盒体锐边和纸面文字分别比较。
- 高/极高开启SMAA，lookup纹理由项目已有Three开发依赖导出为资产，Qt运行不加载Three；原始画质保留源路径的几何抗锯齿行为，不把无SMAA解释为无任何抗锯齿。
- 将盒体ACES/纸面toneMapped=false/最终sRGB合成路径与R01逐项对照；有内置色调映射时关闭重复自定义转换。QML文字和阅读遮罩最后叠加。
- 退出：景深/边缘/颜色ROI达到目标或形成明确R10限制；四预设切换不重建业务状态。

### R10 · 渲染可行性门槛与固定实现选择（0.5–1日）

- 依赖：R07–R09B。交付Qt/C++图像比较器、基线对照场景和RenderBackend合同字段。
- 在同一场景检查单盒、前后重叠盒、低角度侧脊、抽取中间态和三种封面；增加小型实例池探测排序及图集更新，不能只测孤立盒。
- 先按已规定参数顺序修正可调误差。若通道控制/透射捕获/实例排序/上传预算存在结构限制，固定选择RHI并保留R10A–R10D；若没有结构限制且达到门槛，固定Quick3D并移除条件轮次。
- 将后端选择、输入接口及不能改变的参数写入contract；后续轮次不再次讨论框架，不用简化玻璃效果绕过本门槛。
- 退出：主渲染路径唯一，具备可重放对照和真实GPU证据。无法达到门槛时留下具体修复任务，不能提前实施后续全部UI。

### R10A · 条件轮次：Qt RHI骨架与不透明通道（1–1.5日）

- 触发：R10固定RHI；依赖R06及R10。交付QQuickRhiItem/Renderer、qsb构建、阴影/不透明目标及shader接口。
- 使用Qt GuiPrivate的QRhi，Windows D3D11；synchronize阶段拷贝SceneFrame，render阶段无GUI访问。标准uniform block、显式binding/location和QSB跨后端编译。
- RenderGraph固定shadow→opaqueColor/normal/depth→capture/mips→shell→AO/DOF/AA→output；不透明封面沿用R07的UV/光场，目标按RenderQuality限制。设备clip-space通过QRhi修正矩阵处理，QSB不依赖OpenGL默认深度范围。
- 退出：已知顶点、深度、法线、单盒不透明投影与Quick3D基线一致，动态resize无资源竞态。

### R10B · 条件轮次：RHI磨砂捕获、材质和排序（1.5–2日）

- 依赖R10A。把已验证的局部LOD/纸面/侧脊算法迁入显式shader；不使用Three运行时，移植项目对应算法及既有shader数学。
- 不透明捕获和壳体输出分离，按原屏幕空间限制处理透射；使用ping-pong目标，禁止采样正在写入的纹理。复制Three相应颜色空间/BRDF/ACES计算时保留源映射与参数测试。
- 实例按固定相机深度排序，在相同距离下用稳定实例身份；壳体前后面的depthWrite/blend规则明确配置，不靠提交顺序随机补偿。
- 退出：R10全部材质ROI通过或残差进一步缩小；不存在穿透、整块白化、黑边或错误遮挡。

### R10C · 条件轮次：RHI图集局部上传与后处理（1.5–2日）

- 依赖R10B。用QRhiResourceUpdateBatch更新具体tile区域，上传队列按2MiB常态/16MiB初始化预算；纹理销毁后generation失效。
- 对齐R09/R09B的阴影/AO/景深/SMAA/输出，各pass独立buffer，复用非重叠生命周期目标；检查D3D11、OpenGL shader方向及MSAA resolve。
- 退出：432实例下排序/透射一致，稳定帧无上传，图像与性能门槛有实际证据；没有增加原生第三方库。

### R10D · 条件轮次：渲染替换集成与跨后端探测（0.5–1日）

- 依赖R10C。SceneAdapter接入RHI，保留projectWorldPoint/hitTest/SceneFrame合同，产品只打包选定后端。
- 用同一fixture重新跑R06–R10（含R09B）检查；至少本机D3D11与可用OpenGL探测，Metal/Linux实际检查留给各平台轮次。
- 退出：QML/导航无后端判断，图形私有接口集中隔离；原Quick3D研究代码不成为第二套生产状态。

### R11 · 封面解码、缓存、图集与所有权（1–1.5日）

- 依赖R10或触发时R10D。交付CoverCache、AtlasAllocator、SelectedCoverLease与异步请求测试。
- QImageReader优先在解码时缩放，固定3并发/16MiB完成缓存；按contain+1/64留边绘制，tile UV钳制到最外像素中心，透明留白保留。
- 选中与归位各持独立租约；先复用已解码图，再补清晰度。新槽未上传不得显示旧专辑封面；重建后晚到结果不能发布。
- 按选定后端落实上传预算；Quick3D整页上传若仍超限，补执行R10A–R10D，不允许临时全图重传留下长期卡顿。
- 退出：横/竖/方封面、连续换目标、缺图和关闭竞态通过；像素载荷与真实提交统计分开。

### R12 · 填充实例池、波浪、拾取与坐标rebase（1–1.5日）

- 依赖R05、R11。交付ArchivePool、实例表、SelectionController填充路径和ray hit测试。
- 移植archive-loop.ts的9列48行池、POOL_LANES顺序、nearestOccurrence和方向语义；行/列循环保留物理方向，实例复用只在边界之外。
- 逐式移植archiveWave/idleWave/musicSelectionWave/extraction/returnStep，并将实例、封面、投影/拾取和归位使用同一显示坐标。
- 抽取对象从实例池接管时原实例隐藏；归位后恢复准确身份，不能同时绘制双份选中盒。rebase更新所有原点和光带。
- 退出：大库只绘制固定可见池；连续首尾浏览、反向及拾取与golden一致，池边不瞬移。

### R13 · 真实列、首尾限制与会话记忆（1–1.5日）

- 依赖R12。交付RealColumns、ColumnMemory、真实单元过滤和导航可用性。
- 合法行12至12+count-1，中点12+(count-1)/2；未选列整列居中，当前列独立移动，离开列按五次轨迹归中；其他列不重启波浪相位。
- 列记忆默认开；只持久化开关，不持久化具体行位。关记忆时按目标列实际显示深度选择最近合法行，切开关不改当前图像/播放。
- 键盘/滚轮/滑尺/触控/详情统一调用NavigationAvailability；真实列首尾禁用，填充仍循环，左右列仍循环。
- 退出：0/1/2/3张、短长列跳转、未完成归中再回列通过；没有重复逻辑专辑。

### R14 · 二维基础控件、页眉与浏览静态布局（1–1.5日）

- 依赖R03、R04、R06、R13。交付DesignTokens、Header、BrowseInfo、Footer与自定义Basic控件。
- 按基线计算样式实现字体、字距、线宽、颜色、留白和图标；品牌与返回锚点、主题双标签、当前歌名槽和播放按钮固定位置。
- LayoutMetrics决定横/竖布局，窄屏保留主题文字和播放按钮；短屏信息可滚动，超宽屏相机/文字协同。
- 阅读渐变与夜间遮罩分层，竖屏侧向遮罩650ms淡出；不以全屏半透明层错误影响顶栏。
- 退出：九个尺寸静态截图通过结构/文本门槛，没有传统Qt默认外观混入产品。

### R15 · 滑尺、数字字轮与导航输入（1–1.5日）

- 依赖R05、R14。交付RulerController/Ruler.qml、OrdinalReel、InputRouter。
- 滑尺12可见+两侧各2预留，anchor=5、弹簧rate=18、ripple=560ms；桌面3px/10/30/22/8间距，窄屏2px/8/21/16/6间距，边缘mask渐隐。
- 刻度自身伸长；复用不可见槽，隐藏槽无焦点/点击。循环点击传可见occurrence相对行差，真实列传有限索引。
- wheel：pixelDelta优先；angleDelta每120对应48px；48px一张、单事件最多3张、200ms手势分段、反向/切列清余量。输入框、面板、歌单和滑块保留滚动所有权。
- 退出：键盘、触摸滑动（36px/1.3方向比/1400ms）与滑尺共用边界，快速反向连续，输入框不触发全局导航。

### R16 · 多行标题字轮、歌名字轮与文字测量（1.5–2日）

- 依赖R05、R14。交付TextLayoutAdapter、TitleReel、TransportTitle与字形测试。
- 移植music-title-reels.ts的460ms/归一rate10解析轨迹、速度限幅和统一时间原点；采样当前glyph位置/速度重定向，不重建整个文本面板。
- QTextLayout做整行断行/shaping，QTextBoundaryFinder做grapheme；glyph单元复用位置和裁切，高度覆盖下伸部及中英文括号；匿名长标题与emoji覆盖。
- 播放歌名槽640ms展开、400ms淡入、内层18px归位，图标180ms透明度/缩放；宽度以实际测量省略，完整名称保留Tooltip/Accessible。
- 退出：换歌/换专辑/窗口resize/中途改速无残影、错字、括号裁切；歌名定位不调用播放。

### R17 · Presentation状态机与详情静态界面（1–1.5日）

- 依赖R14、R16。交付PresentationController、SurfaceTransition、Detail.qml/TrackList.qml。
- 逐状态移植music-presentation.ts；open先退browse再enterCamera，detail就绪才进文字，close先退文字再归位；新请求覆盖pendingSelection，revision隔离完成信号。
- 详情保留编号、曲目、时长、介绍/本地备注、制作人员与来源字段；歌单用ListView虚拟化，视图滚动不影响专辑架导航。
- 异步准备布局完成后再开始文字动效，关闭浮层恢复焦点；隐藏层disabled，视觉退出期间防止底层误操作。
- 退出：退字期间连选/取消只执行最后目标，快速open/back不误关闭新详情，减少动态完成路径一致。

### R18 · 详情抽取、直接换片、归位与拖动查看（1.5–2日）

- 依赖R12、R17。交付DetailMotion、ReturningCasePool、PointerDragController。
- 相机和抽取按基线同步轨道，不能改成先抽完再侧移。详情换片：退旧字→同构图内旧盒归位/新盒抽出→新字进入，不绕主页。
- 归位副本维持自身封面与状态，朝向落稳后才下降；快速换片管理有限副本池，回收只能发生在归位完成/明确失效。
- 拖动查看保留轴向、角度限幅、惯性/回正，与PointerHandler手势区分点击；按实体transform更新拾取，不用二维假旋转。
- 退出：连续三次换片、反向、途中返回/搜索、拖动中切片和减少动态均无跳变/错封面。

### R19 · 总览取景、边缘折叠与列名两步进入（1–1.5日）

- 依赖R13、R16、R18。交付OverviewController、OverviewLabelsModel/Overview.qml。
- 总览progress rate=6.5；span=max(16.8,17.5/aspect)、distance=172、yaw=28、elevation=43，沿当前相机状态连续拉远；真实列居中与相机共用progress。
- 移植music-overview-edge.ts：外侧列在±4.5处收叠，overview/0.65完成折叠；extent≤0.12不可拾取。
- 标签按projectWorldPoint跟随整列，选中/展开优先，保留.06/.12滞回、边界缓冲与碰撞规则。第一击只展开进入按钮，第二击定位并返回标准视图。
- 退出：V/Esc/点专辑返回一致，播放歌名保留/按钮固定，总览换列标签不抖动，窄屏不重叠。

### R20 · 二维序章、三维开场和交互交接（1–1.5日）

- 依赖R08、R12、R14、R17。交付OpeningController、Opening.qml、品牌轮廓/圆环路径动画。
- 按musicOpeningFrame逐参数采样，不重写近似时间表：轮廓0.24–1.64s、标语2.72–3.58s进入/3.58–5.70s静读/5.70–6.32s退出，标语“一张一张，慢慢听”。
- 二维底纸6.32s开始消退，三维6.40s从源时间21.92开始，11.60s落稳；主页错峰720ms完成后解锁。主品牌节点复用，不通过两份品牌交叉淡入。
- View3D准备完成后才起片头；0–6.32s完全遮住时不提交三维帧，必要时显式暂停View3D/RHI更新而非只设opacity=0。
- 退出：跳过/重播/后台恢复/中途减少动态有明确终态、正确焦点与一次交接；20–120fps时序golden通过。

### R21 · 本地扫描、身份规则与缓存写入（1–1.5日）

- 依赖R04。交付DirectoryScanner、LibraryStore、原子JSON缓存和扫描取消。
- 保留原扩展名集合、根目录直属文件各自单曲、子目录含曲即专辑、多CD子目录分别识别；不跟随目录符号链接造成重复/循环，隐藏目录沿原规则忽略。
- 原生绝对路径UTF-8哈希与原fingerprint规则测试；根目录最多64，嵌套根去重，离线根保留旧索引。扫描只读文件，不上传内容。
- worker形成不可变快照，GUI一次应用增量/重置；保存串行，扫描中切根使旧结果失效；不重载整个窗口。
- 退出：合成目录测试覆盖重扫/重命名/删除/离线/取消，UI保持响应，源文件hash不变。

### R22 · Qt元数据探测、封面优先级与旧数据导入（1.5–2日）

- 依赖R11、R21。交付MediaMetadataProbe、LegacyImporter及兼容性测试。
- 最多2个探测QMediaPlayer，每个任务独立generation、无QAudioOutput、不调用play；等待LoadedMedia/metadata/duration事件，8秒超时或InvalidMedia结束。不可探测保留文件名/未知艺术家，不删曲目。
- 映射QMediaMetaData中的Title/AlbumTitle/AlbumArtist/ContributingArtist/TrackNumber/Date/Genre/Duration及CoverArtImage；不使用Qt6.12已弃用的ThumbnailImage。按实际公开字段保留，不访问Qt内部解码器。
- 目录封面cover/folder/front优先；直属单曲先嵌入封面再同名图；图片大小上限30MiB。现有字段/制作信息可从旧索引保留，Qt缺失字段不能覆盖为假值。
- 导入只读music-data-v3到新dataDir，复制必要封面，保持本地备注/来源/未知JSON字段；浏览器偏好无法自动读时提供显式JSON导入入口，不读取用户浏览器数据库。
- 退出：无声探测、损坏文件、超时、并发取消、索引v3/v4导入和unicode路径通过；播放能力与可索引能力分别显示。

### R23 · 音乐库/搜索面板与歌曲定位（1–1.5日）

- 依赖R16–R19、R22。交付LibraryPanel、SearchPanel、TrackFocusController。
- 复刻音乐库目录/扫描状态、搜索框/结果/空状态/流派过滤；结果按稳定ID，匹配规则沿music-search.ts，不添加网络搜索。
- 搜索目标路径：必要时退详情→归架→五次定位→打开→滚到曲目，目标行高亮按music-track-focus.ts；当前专辑直接滚动，不重抽盒。
- 点击当前播放歌名走同一定位接口；任何定位不调用play/pause/setSource。搜索替换只提交最后目标。
- 退出：搜索中连续输入/取消、切原歌手列、当前曲目定位和无结果状态无跳变，焦点/快捷键正确。

### R24 · 普通歌曲播放、队列、进度与输出设备（1–1.5日）

- 依赖R04、R17、R22。交付PlaybackController和QtMultimedia集成测试。
- 单个歌曲QMediaPlayer+QAudioOutput；本地文件QUrl::fromLocalFile，上一/下一首由C++队列维护，EndOfMedia只处理一次。无曲时Space播放当前专辑；暂停/停止行为沿原产品。
- 重建source操作使用递增revision；来源替换先断开旧操作连接，延迟事件核对当前source/待播放意图，暂停/停止不能被晚到LoadedMedia恢复播放。
- position/duration/seekable来自公开信号，用户拖动时暂时保持拖动值，提交setPosition后恢复跟随；不可seek禁用进度拖动。playbackRate固定1，动效速度不修改歌曲。
- QMediaDevices列设备，QAudioOutput::setDevice切换；设备消失回系统默认并提示。错误只显示Qt原因和可操作的换曲/重试，不增加外部解码后备。
- 退出：支持格式的本地样本实际播/暂停/seek/停止/换曲/设备变化通过；不支持格式明确提示，没有任何PCM、无缝或专业输出代码。

### R25 · 氛围音乐、界面音效与音频设置简化（0.5–1日）

- 依赖R03、R24。交付BackgroundMusicController、InterfaceSoundController、音频设置UI。
- BGM用独立QMediaPlayer/QAudioOutput循环播放现有资源，独立音量/开关；歌曲播放前220ms淡出BGM，暂停安静，停止/队列结束650ms按BGM自身音量恢复。淡变只调公开volume，不处理样本。
- UI音效用QSoundEffect及固定小池，音量独立；可直接播放的短资源缺失时该事件静音。保留事件接口供后续加资源，不借用第三方合成器或手写PCM。
- 输出设置改为系统默认/Qt设备列表和普通格式错误信息；去掉无缝三档/外部FFmpeg/CoreAudio探测。旧songTransition/songFade只作为被忽略的旧字段保留，绝不迁回无缝行为。
- 退出：歌曲/BGM/音效音量互不影响，暂停不恢复BGM，浏览定位不影响声音，最小化后歌曲继续。

### R26 · 完整设置、离线资料占位与未来联网插槽（1–1.5日）

- 依赖R04、R09B、R13、R23、R25。交付PreferencesStore、SettingsPanel和Provider注入集成。
- 复刻设置排版/控件/折叠/画质/主题/音量/排列/列记忆/减少动态/速度/重播；偏好schemaVersion固定迁移，未知字段保留，新JSON原子提交。
- 资料区域保留介绍/制作信息/MusicBrainz的标题、来源、查询状态与配置模型；已有缓存内容正常展示，执行按钮禁用并提示暂未启用。不返回伪查询成功/无结果。
- 接上IMetadataProvider异步结果路由、requestId取消、批量进度模型与ProviderCapabilities；用测试Provider注入Matched/Uncertain/Error证明UI无需改动即可接入未来实现，产品只注册OfflineProvider。
- 原设置重载只重建视觉树，不重建正在播放的媒体对象，不清除用户画质或数据；偏好与会话列记忆仍分开。
- 退出：持久化/导入/坏文件处理、设置反向打开、测试Provider过期结果和零网络请求断言通过。

### R27 · 光照/速度调节面板与演示入口（1–1.5日）

- 依赖R08、R19、R20、R26。交付LightingPanel、MotionPanel、演示/研究启动参数。
- 按music-lighting-lab/music-motion-lab的既有范围接入baseline/area/guided/hybrid及强度/宽度/偏移/过渡时间；只改变已有参数，不重新创建材质或实例。
- 全局倍率改动保持当前轨迹，片头/歌曲不变；恢复默认回guided固定预设。调试信息显示正确frame分位与上传/解码计数，不泄露私人路径。
- 普通音乐入口在根目录；演示和光照研究入口在tools/launchers/，对应`--demo`/`--lighting-lab`。演示不自动读真实库或启动播放。
- 退出：实验面板滚动不导航专辑、各倍率往返连续、默认复位一致，启动参数功能分离。

### R28 · 冷启动、异步生命周期与性能收敛（1–1.5日）

- 依赖R11–R27。交付StartupCoordinator、资源统计和生命周期/性能检查。
- 加载提示期间准备模型、可见封面与真实渲染通道；提交两帧初始化目标后才启动开场时钟。扫描完成发生在准备中时，只应用一个一致快照，交接后再允许普通更新。
- 隐藏暂停视觉tick/封面上传/非必要扫描通知，播放继续；恢复清统计时间而不清状态。重载/退出取消旧generation，资源释放幂等，QObject/GPU销毁顺序明确。
- 用96张合成大封面和超432匿名库验证预算；不以关景深/降低默认画质解决冷启动，必须消除重复解码、全图上传和重复实例创建。
- 退出：性能契约、10次重建/30次换片资源回落通过；初始化耗时与稳定帧数据各自记录。

### R29 · 全GUI成对重放与视觉差异修复（1.5–2日）

- 依赖R28。交付覆盖九个尺寸/双主题/两阵列模式的成对截图与动画数值报告。
- 执行所有验收序列，按区域比较头部/文字/滑尺/盒体/光带/背景/总览；分开报告字体边缘、三维着色与音频简化导致的允许UI变化。
- 修复顺序固定为几何、布局/相机、字体、颜色、光场、材质、后处理、时序/打断；每次修复仅重跑受影响场景和关联行为检查。
- 不把扫描元数据缺失、Qt支持格式或音频简化当作GUI缺陷；不把真实玻璃差异归因于音频范围变化。若超过两日仍有差异，拆具体ROI/事件补修轮次再继续。
- 退出：结构/运动硬门槛满足，图像目标最大程度达到，残余差异有成对证据和不能进一步缩小的具体原因；不得使用笼统“已完全复刻”。

### R30 · Windows部署、普通启动器与干净环境验证（1–1.5日）

- 依赖R29。交付Release目录、部署脚本、根目录Windows普通启动器及Qt使用说明。
- windeployqt指定qml目录部署Qt Quick 3D、Multimedia及Qt随附媒体依赖；打包qrc资产与MSVC运行库要求，不复制整个Qt安装目录。
- 用无Qt开发PATH、无Node/FFmpeg路径的临时环境运行；验证离线启动、demo、unicode音乐路径、设备列表、可支持样本和字体加载。
- 普通启动器只找合法安装/构建产物并启动Qt程序，不后台启动Web服务；演示/光照启动器继续在tools/launchers/。
- 退出：Release脱离开发环境能运行，数据路径符合QStandardPaths，部署包不含私人库/缓存/调试基线。

### R31 · macOS构建、Metal与平台集成（1–1.5日）

- 依赖R30。交付macOS构建/部署脚本和平台视觉/播放验收产物；需要实际macOS执行环境。
- 同一Qt6.12源码构建，macdeployqt部署QML/媒体插件；处理应用bundle资源路径、Retina逻辑像素、菜单/快捷键和关闭行为，运行时不依赖Swift桥接。
- 使用Metal执行代表性浏览/详情/总览/片头/玻璃ROI；修复shader坐标和字体加载差异，不以Windows截图替代。已有.command普通入口切换到Qt产物，Web参照入口移到tools/launchers/。
- 验证本地播放、暂停、seek和默认设备/设备变化；Qt不支持的编码按公开错误处理。
- 退出：实际构建、bundle运行、Metal GUI和QtMultimedia设备测试完成；缺设备时保留此轮，不发布跨平台已完成声明。

### R32 · Linux构建、图形后端与平台集成（1–1.5日）

- 依赖R30。交付Linux构建/部署脚本、普通.sh启动器和平台验收产物；需要实际Linux执行环境。
- Qt6.12+CMake/Ninja，使用Qt支持的系统图形/媒体依赖；QML/插件部署路径明确，优先OpenGL并对可用Vulkan探测，不要求系统安装第三方应用引擎。
- X11/Wayland至少可用环境实际验证，固定DPR/内容尺寸对照；检查FONT加载、触摸板pixelDelta、文件路径大小写、默认音频设备和权限错误。
- 若需要单独系统包，在平台说明写具体Qt部署依赖；不让安装脚本静默修改系统音频/驱动配置。
- 退出：干净环境启动、代表性GUI/音频实际检查完成；缺环境保留本轮。

### R33 · 原生主入口收口与最终文档（0.5–1日）

- 依赖R30–R32及所有视觉补修任务。交付根启动器、README/开发说明、源模块映射和最终离线功能清单。
- 默认产品入口指向Qt；保留Web源码/基线/研究资产可供回归，但普通用户流程不再要求Node、浏览器、Swift或外部FFmpeg。
- 文档明确普通播放、Qt支持格式边界、联网接口暂未启用、导入旧数据方式、Qt构建/平台部署命令和仍可复现的视觉残差；不恢复本计划已排除的能力。
- 对Qt原生构建、资源、GUI重放、离线Provider、普通播放和三平台结果做最终核对；未运行检查继续保留任务，不能仅凭CI文件存在或自动测试模拟通过收口。
- 退出：所有必需轮次有真实交付与验收资产，主入口可用；本文件删除已完成轮次，只保留未来联网接入合同和必要的长期约束。

## Qt官方实施参考

以下仅作为公开接口语义依据；执行时优先读取本机Qt6.12随附文档，不因查阅文档而在产品中启用联网功能。

- [QML与C++集成](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html)
- [Qt Quick帧动画](https://doc.qt.io/qt-6/qml-qtquick-frameanimation.html)
- [Qt Quick 3D自定义材质及屏幕纹理限制](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)
- [Qt Quick 3D后处理Effect](https://doc.qt.io/qt-6/qml-qtquick3d-effect.html)
- [Qt Quick 3D实例表](https://doc.qt.io/qt-6/qml-qtquick3d-instancing.html)
- [QQuickRhiItem及QRhi版本兼容边界](https://doc.qt.io/qt-6/qquickrhiitem.html)
- [Windows系统DirectWrite字体容器解包](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefactory5-unpackfontfile)
- [QMediaPlayer](https://doc.qt.io/qt-6/qmediaplayer.html)、[QAudioOutput](https://doc.qt.io/qt-6/qaudiooutput.html)、[QSoundEffect](https://doc.qt.io/qt-6/qsoundeffect.html)、[QMediaMetaData](https://doc.qt.io/qt-6/qmediametadata.html)
