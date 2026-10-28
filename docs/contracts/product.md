# 产品范围与工程归属

本文唯一维护产品定位、支持范围、技术身份、顶层组合及目录职责。使用入口见根README，技术算法见迁移计划的专项索引，未验收目标仅在迁移计划R36维护。

## 产品范围

面向Windows、macOS、Linux桌面的本地音乐收藏播放器，提供收藏架浏览、真实列/填充、列记忆、排序/搜索、详情抽取/拖动/归位、总览、开场、设置与普通歌曲播放。浏览、搜索、总览及歌曲定位均独立于播放意图。支持暖昼/深夜、减少动态、响应式布局和动效速度控制；速度与时钟语义归[motion.md](motion.md)。

歌曲、氛围音乐和界面音效由[playback.md](playback.md)定义，普通播放只用QtMultimedia公开API。联网资料显式授权后由[library-data.md](library-data.md)的Provider接入MusicBrainz及Wikipedia/Wikidata，来源、缓存和查询状态可离线查看。原项目中不合理的行为按[source-defect-exceptions.md](source-defect-exceptions.md)确定正常预期。

不实现移动端、流媒体播放、无缝/交叉混音、手动PCM/重采样或自建音频引擎。产品运行基于Qt和系统公开能力，不依赖Node、浏览器、WebEngine、本地HTTP服务、Swift音频桥接或外部FFmpeg进程；Qt随附媒体依赖由部署工具处理，不自行链接其内部FFmpeg。不新增第三方运行库；构建和维护工具不读取旧工程或其锁定开发依赖。

## 技术身份

| 项目 | 唯一值 |
| --- | --- |
| 展示名、窗口标题 | `yalahOO!!!` |
| organizationName / applicationName / CMake project | `yalahoo` |
| 主程序 / 核心目标 | `Yalahoo` / `YalahooCore` |
| 输出文件名 | `yalahoo`（平台扩展由构建系统添加） |
| QML模块与入口 | `Yalahoo.Music` / `loadFromModule("Yalahoo.Music", "App")` |
| macOS bundle ID | `yalahoo.music` |
| Linux desktop / 图标名 | `yalahoo` |

`initializeIdentity()`在QGuiApplication及任何QStandardPaths查询之前设置技术/展示身份；展示名变化不迁移数据根。原创图形与平台衍生资产由[branding.md](branding.md)维护。

## 顶层组合与目录

`src/app/ApplicationRuntime.cpp`在GUI线程构造长期业务控制器、Provider、媒体和QML引擎，显式注入`App.qml → MusicStage.qml`。ApplicationOptions在持久化对象创建前解析并验证互斥启动模式与绝对输入路径；LibraryPersistence组合库持久化接线，AudioPreferencesBinding独占音频设置同步。MusicStage组合场景；StageFrameDriver交付唯一帧源，StageFocus恢复焦点，StagePanels承载弹层，HeaderTransport、ColumnNavigation和OverviewLabel分别绘制对应区域。控制器各自拥有状态，通过窄接口协作；QML视觉重载不销毁库/偏好/媒体。模型通知、异步对象与渲染快照边界分别遵守Qt/QML及RHI契约。

| 路径 | 归属 |
| --- | --- |
| 根CMake、`cmake/` | 构建、Kit/能力探测、模块/资源清单与部署接线 |
| `src/app/`、`src/core/` | 程序组合/注册、身份、值模型、日志与共用协议 |
| `src/motion/`、`src/presentation/` | 唯一时钟/运动数学、展示/焦点/面板状态 |
| `src/library/`、`src/services/`、`src/media/` | 扫描/存储、Provider/偏好/封面、Qt媒体控制器 |
| `src/render/`、`src/platform/` | 场景/光场/输入/文字/RHI及真实平台差异 |
| `qml/Yalahoo/Music/` | 产品视觉树，只保留实际产品QML |
| `resources/`、`tests/fixtures/` | 产品资产、来源manifest与独立测试输入 |
| `tests/{application,unit,integration,render,support,tools,runners}` | 应用验证入口/Quick3D Review、行为回归、捕获观察器、测试支持及已构建产物驱动 |
| `audit/{source,assets,rhi}` | 源码策略/编译器检查、独立几何检查及原生渲染审计 |
| `tools/{build,assets,deploy,launchers,diagnostics,format,support}` | 构建/资产/部署/启动/联网诊断工具及共用验证窗口支持 |

生产、审计、测试与工具实现按上表分属不同目录，不在生产文件内嵌审计/断言/回放实现。产品组合通过`ApplicationLaunch`生命周期观察接口接入`tests/application`的命令宿主，保留既有`--verify-*`协议与独立会话；生产选项由ApplicationOptions解析，验证选项仅由VerificationOptions扩展。渲染器的私有状态声明保留于`src/render/rhi/*_p.h`，验证遥测、资源报告、读回实现与上传字节观察定义在`tests/render`，原生附件/着色器替换审计定义在`audit/rhi`；条件编译和Review专用Qt适配也由审计目录维护。产品实验面板的有界运行诊断属于生产功能。GLB解码由`src/core/GeometryData`维护，Qt mesh对照检查由`audit/assets`维护，文件读写由`tools/assets/AssetFiles`维护。

生产唯一场景为RHI SceneItem，后端语义归[render-backend.md](render-backend.md)；Quick3D仅作隔离Review。C++23/最新原生Kit、QML别名/qrc与输出位置归[native-bootstrap.md](native-bootstrap.md)。本地生成目录与Git边界归[verification-artifacts.md](verification-artifacts.md)。
