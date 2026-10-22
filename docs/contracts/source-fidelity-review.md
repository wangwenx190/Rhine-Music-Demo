# 冻结来源与原生适配判定

本文只维护冻结模块到原生实现的适配理由及历史来源索引；完整逐模块hash/导入/当前路径由[source-map.json](source-map.json)维护。具体算法、数值、接口和组件检查归下列专项契约，运行成绩留本地，未验目标唯一归迁移计划R36。表中轮次仅供Git来源定位，不表示当前目标已运行通过。

## 来源索引

| 来源轮次 / 引入提交 | 当前主题的唯一规范 |
| --- | --- |
| R01 `a48211c`、R02 `24b125a` | [冻结来源](web-baseline.md)、[Kit与工程](native-bootstrap.md) |
| R03 `83f933c`、R03B `598c4a8` | [导入资产](assets.md)、[原创品牌](branding.md) |
| R04 `739ee97` | [数据与Provider](library-data.md) |
| R05 `ec0af92`、R06 `5102cb0` | [帧源与运动](motion.md)、[相机/布局](scene.md) |
| R07 `1556a55`、R08 `178ca45` | [材质](materials.md)、[灯光](lighting.md) |
| R09 `3fc6a39`、R09B `522a3f3` | [SSAO/质量](occlusion.md)、[后处理](postprocess.md) |
| R10 `d06e5f3`、R10A `52f1278`、R10B `5625d73`、R10C `1f2352a`、R10D `75775b3` | [后端](render-backend.md)、[不透明/阴影](rhi-opaque.md)、[透射](rhi-shell.md)、[资源图](rhi-postprocess.md)、[生产接线](rhi-integration.md) |
| R11 `f7e423a`、R12 `b7f73d4`、R13 `0d06b7b` | [封面](covers.md)、[填充池](archive-pool.md)、[真实列](real-columns.md) |
| R14 `1a082f2`、R15 `2ee2294` | [浏览静态层](browse-ui.md)、[导航/数字](navigation.md)、[Canvas API](canvas-painter.md) |
| R16 `2abb4f6`、R16B1 `6636ddb`、R16B2 `509afe8`、R16B2V `07514a5`、R16M1 `c7e9b61`、R16M2 `b9a5ea7`、R16M2P `9470126`、R16M3 `2840110` | [字体与标题](title-reels.md) |
| R17 `33e8260`、R18 `ed5aaef`、R19 `1c9b8c6` | [展示](presentation.md)、[详情运动](detail-motion.md)、[总览](overview.md) |

R19来源复查提交：A `8e6c5a3`、B `ae60ff8`、C1 `48a9495`、C2A `75b5543`、C2B1 `3c90d2d`、C2B2 `ebfb320`、C2C `2d3c9b1`、D1 `4ab1e73`、D2 `2f50812`、D3A `f45024d`、D3B `f4e1610`、D4A `704a99d`、D4B1 `5e333ec`、D4B2 `4e86136`、D4C `5454fb2`、E `cfce597`、F `a04db48`、G1 `3b1f89b`、G2 `dd5f959`。父任务由实际子轮次覆盖，不另产生验收任务。跨组件的`d03527e`涉及索引/可见集/缓存，`e98c798`涉及Cocoa/GL/RenderControl生命周期；其当前行为均由对应契约维护。

## 资产、模型与数据适配

GLB属性及绕序保留；Qt mesh的V补坐标属于格式转换。字体从WOFF2无损解包并按实际字形表选择，平台报告的命名字重不能替代原OS/2表。功能图标来自实际svg调用；原创品牌是用户指定差异。来源保护和资产规格归[assets.md](assets.md)。

原生QUrl、路径稳定身份、const快照、GUI模型通知和有界异步Provider是本地桌面接口适配。QCollator与Web ICU可有排序差异，固定匿名来源列顺序仍由独立fixture提供。缓存/取消/generation只改变调度和所有权，不改变搜索字段或播放意图；实现规范归[library-data.md](library-data.md)。

## R19B 运动、相机与阵列

冻结motion/archive/real-columns及scene音乐相机块是数学来源。解析求值、惰性列取样、公共场提升和相对坐标求位移用于控制工作量及double舍入；Qt矩阵/QRhi裁剪转换用于交付。它们不能增加取景偏移、改变导数或用池槽号替代物理身份。详情同cell回收和有限副本属于有界所有权适配；规范归motion/scene/archive-pool/real-columns契约。

## R19C 材质、光场与透射

材质取music-model的configure/theme/clarity与scene最终override；灯光取音乐入口的有效世界光场。跨API沿用源辐射单位与同一公式，没有图像标定系数。隔离Quick3D Review的直接材质输出与生产compositor的颜色路径分别维护，不能混用；规范归[materials.md](materials.md)、[lighting.md](lighting.md)。

生产RHI提供源全局表面排序、独立不透明捕获、深度与resolve控制。壳体导数使用view-space法线；LTC只在数学奇点选择有限基。visibility与Equal颜色pass需要相同位置运算及shader不变性，不能添加depth epsilon遮掩漏画。规范归[rhi-shell.md](rhi-shell.md)。

## R19D 算法与资源图

| 源调用/适配 | 判定依据与唯一规范 |
| --- | --- |
| scene最终AO覆盖、锁定SSAOPass/Shader | 完整几何、源核/累加、RGB24距离域与标量精度保留；GL奇数宽读回的行跨度是捕获适配，不能将错误解码当作产品shader值。[occlusion.md](occlusion.md) |
| MeshPhysical/Lambert fog、EffectComposer/OutputPass | fog在线性着色后、捕获前；最终颜色只转换一次。隔离材质ACES不进入逐实体生产路径。[postprocess.md](postprocess.md) |
| shadowmap_vertex / shadow filter、源地面 | normalBias在顶点阶段、只衰减key分量；产品地面不是诊断receiver，采用源Standard着色并进入AO/DOF。[rhi-opaque.md](rhi-opaque.md) |
| 锁定BokehShader / SMAAShader / lookup | 保留真实核、标量构造、颜色/深度过滤及lookup行序；独立解析期望不来自浏览器GPU。[postprocess.md](postprocess.md) |
| QRhi附件、实例、图集与读回 | 同格式/采样renderbuffer替代缺能力MSAA纹理是API适配；不降低生产质量。别名纹理按身份去重，共享值和有界回调只改变采集成本。[rhi-postprocess.md](rhi-postprocess.md) |

## R19D4C 目标尺度与封面

冻结音乐宿主最终CSS为实际窗口viewport；另一入口的参考舞台缩放不适用于此路径。SceneItem与最终合成只消费实际逻辑尺寸/DPR，不叠乘参考比例。源景深/磨砂不作为整体模糊补偿；质量归[occlusion.md](occlusion.md)。

投影需求清晰层、独立线性光mip及足迹过滤属于用户指定的封面质量实现；保持contain、透明留白、图集区域隔离和副本所有权。精确采样与缓存规则归[covers.md](covers.md)。

## R19E 浏览静态层、滑尺与数字

实际music.css最终callout、theme-switch竖屏覆盖及transport-title cap决定几何，不能采用早期声明。CSS行框/inline strut映射到实际字体度量，分数字号用局部矢量比例；这些是盒模型适配。[browse-ui.md](browse-ui.md)维护静态规范。

Canvas Painter的选择基于源矩形/路径表达、固定快照同步及renderer所有权。Qt滚轮单位/Y方向、焦点祖先、固定槽池与单个最新目标适配原生输入；数字保留锁定rolling-number的轨迹和重定向语义。[navigation.md](navigation.md)维护数学与行为，[canvas-painter.md](canvas-painter.md)维护绘制API。

## R19F 标题与字体

整行shaping保留连字/组合字，普通glyph与彩色Quick Text分工；GUI传字体描述，render取得所属线程QRawFont。自然宽度、尾空白、混合ascent及macOS半行距有字体框依据；不以截图偏移校准。源歌名逐字flex与整行kerning可有差异，原生保留整行字形并以真实自然宽度省略。[title-reels.md](title-reels.md)维护当前实现及来源探针。

## R19G 详情与总览

Presentation的layoutRevision是Qt异步布局交接；类型稳定ListView、虚拟委托、有界署名展开及实际origin修正是视图适配，不能改变展示/播放身份。总览固定槽池替代动态DOM，完整退出才清除入口残值；局部Gaussian保留源backdrop语义，文字仍使用实际输出DPR。规范分别归[presentation.md](presentation.md)、[detail-motion.md](detail-motion.md)、[overview.md](overview.md)。

## 原生产品功能来源

| 主题 | 来源、适配理由与唯一规范 |
| --- | --- |
| 序章/启动 | music-opening/boot-timing与scene实际musicIntro。StartupCoordinator协调原生异步快照与准备token，Film展示冻结不改变源时间数学；[opening.md](opening.md) |
| 本地扫描/媒体/导入 | music-library的walkAlbums/readAlbum/resolveGenre/原子保存。Qt公开无声元数据、内容寻址封面和显式只读导入属于原生替代；[library-data.md](library-data.md) |
| 普通播放/三路声音 | music-app播放接线、music-player命令和audio事件。普通Qt播放及匿名模式静音为用户固定范围；[playback.md](playback.md) |
| 设置/实验 | renderSettingsPanel、quality-settings、motion-settings及lighting/motion-lab。偏好只保存选择，视觉重载保留长期控制器；[settings.md](settings.md)、[lighting.md](lighting.md)、[motion.md](motion.md) |
| 库/搜索/歌曲定位 | renderLibraryPanel/renderSearchPanel/revealAlbum、music-search/track-focus及ui-transitions。原生模型虚拟化和Qt焦点不改变匹配/定位语义；[library-data.md](library-data.md) |
| 共用控件 | style/music/lighting-lab最终CSS；Templates仅提供行为，平台原生内部装饰使用明确的项目几何；[controls.md](controls.md) |
| 整场/部署/系统菜单 | 使用同一生产MusicStage及控制器，平台原生菜单/包内闭包是桌面适配；[rhi-integration.md](rhi-integration.md)及平台操作文档 |
| 联网资料 | album-introductions的保守身份准入和music-library的发行匹配；Qt Provider增加独立网络授权、实际曲目核对、响应上界与缓存事务；[library-data.md](library-data.md) |

冻结源自身的例外仅在[source-defect-exceptions.md](source-defect-exceptions.md)记录，不与原生实现缺陷或Qt适配混写。外部Qt参考的采用范围、作者及许可唯一见[参考NOTICE](../../resources/licenses/qt-playback-reference-NOTICE.md)。
