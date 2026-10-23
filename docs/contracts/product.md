# 产品范围与工程归属

本文唯一维护产品定位、支持范围、技术身份、顶层组合及目录职责。使用入口见根README，技术算法见迁移计划的专项索引，未验收目标仅在迁移计划R36维护。

## 产品范围

面向Windows、macOS、Linux桌面的本地音乐收藏播放器，提供收藏架浏览、真实列/填充、列记忆、排序/搜索、详情抽取/拖动/归位、总览、开场、设置与普通歌曲播放。浏览、搜索、总览及歌曲定位均独立于播放意图。支持暖昼/深夜、减少动态、响应式布局和动效速度控制；速度与时钟语义归[motion.md](motion.md)。

歌曲、氛围音乐和界面音效由[playback.md](playback.md)定义，普通播放只用QtMultimedia公开API。联网资料显式授权后由[library-data.md](library-data.md)的Provider接入MusicBrainz及Wikipedia/Wikidata，来源、缓存和查询状态可离线查看。原项目中不合理的行为按[source-defect-exceptions.md](source-defect-exceptions.md)确定正常预期。

不实现移动端、流媒体播放、无缝/交叉混音、手动PCM/重采样或自建音频引擎。产品运行基于Qt和系统公开能力，不依赖Node、浏览器、WebEngine、本地HTTP服务、Swift音频桥接或外部FFmpeg进程；Qt随附媒体依赖由部署工具处理，不自行链接其内部FFmpeg。不新增第三方运行库；来源审计只使用冻结锁定开发依赖。

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

`src/app/main.cpp`在GUI线程构造长期业务控制器、Provider、媒体和QML引擎，显式注入`App.qml → MusicStage.qml`。ApplicationOptions在持久化对象创建前解析并验证互斥启动模式与绝对输入路径；LibraryPersistence组合库持久化接线，AudioPreferencesBinding独占音频设置同步。控制器各自拥有状态，通过窄接口协作；QML视觉重载不销毁库/偏好/媒体。模型通知、异步对象与渲染快照边界分别遵守Qt/QML及RHI契约。

| 路径 | 归属 |
| --- | --- |
| 根CMake、`cmake/` | 构建、Kit/能力探测、模块/资源清单与部署接线 |
| `src/app/`、`src/core/` | 程序组合/注册、身份、值模型、日志与共用协议 |
| `src/motion/`、`src/presentation/` | 唯一时钟/运动数学、展示/焦点/面板状态 |
| `src/library/`、`src/services/`、`src/media/` | 扫描/存储、Provider/偏好/封面、Qt媒体控制器 |
| `src/render/`、`src/platform/` | 场景/光场/输入/文字/RHI及真实平台差异 |
| `qml/Yalahoo/Music/` | 产品视觉树；`scene/`中的Quick3D仅Review |
| `resources/`、`tests/fixtures/` | 产品资产、来源manifest与独立测试输入 |
| `tests/{unit,integration,render,tools,runners}` | 行为回归、宿主、工具与已构建产物驱动 |
| `tools/{build,assets,deploy,launchers,audit,migration,format}` | 开发工具；具体入口归所属契约 |
| `legacy-web/` | 冻结来源及其锁定开发依赖，读取规则归[web-baseline.md](web-baseline.md) |

生产唯一场景为RHI SceneItem，后端语义归[render-backend.md](render-backend.md)；Quick3D仅作隔离Review。C++23/最新原生Kit、QML别名/qrc与输出位置归[native-bootstrap.md](native-bootstrap.md)。本地生成目录与Git边界归[verification-artifacts.md](verification-artifacts.md)。
