# 全项目 Qt 官方 skill 只读审查

尚有 **1 项 P3 规范建议**、**5 项待核实事项**。1076 条 lint 命中不等同于 1076 个缺陷。

审查对象为下述提交对应的干净源码工作树；审查未修改源码、契约或迁移计划。本文为用户要求保留的审查快照，不替代项目契约；结论和行号均以该提交为准。没有启动可见窗口、物理设备测试、性能采样或私人曲库扫描。

项目提交：`61fa3e5e8d95082076a17c70ff277c56b812b928`。Skill 提交：`6845f02d41171a6df8b6e6fe225b7f5d1a6f4adf`。日期：2026-10-23（Asia/Shanghai）。

使用 [qt-cpp-review](https://github.com/TheQtCompanyRnD/agent-skills/blob/6845f02d41171a6df8b6e6fe225b7f5d1a6f4adf/skills/qt-cpp-review/SKILL.md) v3.0 和 [qt-qml-review](https://github.com/TheQtCompanyRnD/agent-skills/blob/6845f02d41171a6df8b6e6fe225b7f5d1a6f4adf/skills/qt-qml-review/SKILL.md) v2.0，依次执行 lint、分片深审和合并。Skill 来源为 TheQtCompanyRnD/agent-skills；本报告由编码代理执行审查生成，不代表 Qt 公司出具的认证结论。

C++/头文件/Objective-C++ 358 文件、64384 行，26 分片；QML 93 文件、8300 行，6 分片，合计451文件/72684行。所有分片逐文件阅读，截断内容补读。跨片检查以具体符号追踪为主，不能据此承诺所有跨组件缺陷均已穷尽。构建、部署、shader、导出与验证脚本沿原生生产/检查闭包补充检查；冻结 Web 历史与第三方素材不作为原生 Qt 风格改写对象。

报告包含尚未解决的发现及下列附录；附录内发现编号沿各自 skill 报告局部编号，与正文全局 D 编号分别解释。

- [附录 A：C++ skill 完整报告](#cpp-skill-report)
- [附录 B：QML skill 完整报告](#qml-skill-report)
- [附录 C：逐文件覆盖清单](#file-coverage)

## 待核实事项

以下不计入已确认缺陷，也不以模拟事件冒充实际设备事实。

#### [I-001] 宿主场景坐标绑定可能过期

- **File**: [qml/Yalahoo/Music/components/ArchiveToolTip.qml:14](qml/Yalahoo/Music/components/ArchiveToolTip.qml#L14)
- **Category**: 深审待核实
- **Confidence**: 70/100
- **Finding / Unverified because**: mapToItem调用不显式跟踪祖先移动；Popup自动夹取/flip的补救使最终可见影响未确认。
- **How to verify**: 窗口右/底边显示tooltip并移动宿主/resize/展开overview，核对位置。


#### [I-002] 无输出期间InvalidMedia与errorOccurred处理不同

- **File**: [src/media/PlaybackController.cpp:329](src/media/PlaybackController.cpp#L329)
- **Category**: 深审待核实
- **Confidence**: 70/100
- **Finding / Unverified because**: status写错误并Paused，阻止restoreOutput自动恢复；但有效媒体由设备丢失触发InvalidMedia的生产可达条件未确认。Qt FFmpeg官方源码仅加载失败发此状态，不能把注入通知当设备事实。
- **How to verify**: 对应实际后端捕获无输出→InvalidMedia事件序列，区分既有格式错误与可恢复资源错误；确认可达再修。
- **Source**: [Qt 官方实现](https://raw.githubusercontent.com/qt/qtmultimedia/dev/src/plugins/multimedia/ffmpeg/qffmpegmediaplayer.cpp)


#### [I-003] 遮挡早退可能推迟未完成的Review捕获

- **File**: [src/render/rhi/RhiRenderer.cpp:175](src/render/rhi/RhiRenderer.cpp#L175)
- **Category**: 深审待核实
- **Confidence**: 72/100
- **Finding / Unverified because**: sceneOccluded清屏早退跳过renderFrame中的readbacks.retire/drain；捕获是否滞留取决于后端回调及其他帧；生产默认关闭捕获。
- **How to verify**: 现有Review捕获提交后立即切遮挡且停止其他更新，核对不恢复场景时是否交付；仅复核异步读回后端。


#### [I-004] 图层物理尺寸窄化前没有完整范围检查

- **File**: [src/render/rhi/SceneItem.cpp:872](src/render/rhi/SceneItem.cpp#L872)
- **Category**: 深审待核实
- **Confidence**: 65/100
- **Finding / Unverified because**: setOpeningLayerSize任意QSize乘DPR后直接转int，panelLayerSize类似；已确认正常生产尺寸来自QML布局，尚未证明正常窗口可达INT_MAX级输入。
- **How to verify**: 先明确内部属性输入域；若承担全QSize边界，用INT_MAX×DPR2最小隐藏测试/UBSan核对，窄化前验证。


#### [I-005] 激活ACK一次readAll假定读到完整行

- **File**: [src/services/SingleInstance.cpp:90](src/services/SingleInstance.cpp#L90)
- **Category**: 深审待核实
- **Confidence**: 72/100
- **Finding / Unverified because**: 客户端waitForReadyRead后一次readAll对比ok\n；服务端只一次write三字节，尚未确认真实Qt/OS路径可短读。
- **How to verify**: 隔离数据根/锁/端点，按协议分段发o与k\n且总时间未超时；确认客户端行为，按总截止时间累积完整行。



## 规范与工具结果

项目 `check-format` 通过（455 个C++/shader/JavaScript文件、92个QML文件）；两组官方 qmllint target 通过。93个QML审查范围还包含保留原貌的导入模型，因此与格式入口范围不同。Review专用Quick3D场景和迁移探针没有另行配置其官方type/lint目标，属于本轮静态覆盖。

为遵守 code-style.md，未使用 skill 的 `--qmllint` 直接调用模式，改用官方 CMake target；Python lint 仍按 `--summary` 保留完整结果。C++856条/32种rule-message、QML220条/39种rule-message。NOMINMAX、自绘边框/必要clip、异构var、有效多参数QString::arg及第三方模型等上下文须保留，不能机械按命中改写。

- P3 / 100/100：[src/render/opening/OpeningController.h:93](src/render/opening/OpeningController.h#L93) Phase枚举末项缺尾逗号。Qt review ENM-1的明确风格建议，无当前运行时影响；无需为此单独功能测试。


## 实际验证与边界

| 检查 | 本轮结果 |
|---|---|
| Yalahoo_qmllint、ui-tests_qmllint | Qt6.12.0官方target均通过 |
| check-format | 通过，无格式化写入 |
| 16项Release定向回归 | 最终全部通过；最初4项受沙箱屏幕/IPC限制失败，保持隐藏策略沙箱外定向复核后通过；两批原始结果均在本地保留 |

16项为 application-options、production-app-root、module-link-and-plugins、directory-scanner、library-data、media-metadata、playback、audio-ambience、online-metadata、single-instance、settings、json-file、qml-behavior、unix-deployment、linux-package-tools、launchers。使用已有Release产物，未重建全项目，未执行全量CTest或全量clang-tidy。通过结果不能覆盖本报告指出的遗漏分支。

Windows/Linux原生编译、部署/后端、Wayland HDR、实际音频/输入/DPR与可见画质事实未在本轮取得；仍按原R36门槛保留，不记通过。仅有可达性疑问的事件或极端内部尺寸留在调查项。




## 本地原始证据

原始 lint 输出、定向测试日志及最小探针保留于本地忽略目录 `verification/qt-official-review-20261023/`，遵守 [verification-artifacts.md](docs/contracts/verification-artifacts.md)，不随本报告提交。报告正文已保留验证方法、关键结果和局限；附录包含全部 lint 分组、定位和逐文件覆盖，阅读本报告不依赖本地附件。


<a id="cpp-skill-report"></a>

## 附录 A · Qt Code Review Report

**Scope**: files: native cpp 全量；冻结 Web 仅作来源参照。
**Files reviewed**: 358
**Issues found**: 857 (856 from lint, 1 from deep analysis)
**Agents**: 26 (按行数分片；跨片检查 best effort，具体发现均另追踪调用链)
**Framework mode**: off

计数保留工具原始命中；它不是已确认功能缺陷数量。项目契约优先于通用启发式规则。

---

### Lint findings

| Rule | Count | Locations | Mitigation |
|---|---:|---|---|
| DEP-1 QScopedPointer — use std::unique_ptr (const unique_ptr for scoped) | 1 | `tests/integration/AssetTest.cpp:80` | 按适用工具链与接口语义评估现代替代；不把仍受支持的类型自动列作运行缺陷。 |
| DEP-10 .count()/.length() — prefer .size() for std consistency | 50 | `src/app/MaterialReview.cpp:160  src/app/OverviewReview.cpp:237,239  src/core/MonotonicTime.h:14  src/library/DirectoryScanner.cpp:356,358  src/render/OverviewController.cpp:318  src/render/SceneAdapter.cpp:394,395,397,398,404  src/render/opening/OpeningInk.cpp:255  src/render/rhi/SceneItem.cpp:834  tests/render/ArchivePoolChecks.cpp:484,486,521,525  tests/render/RhiFlickerChecks.cpp:248  tests/render/RhiShellChecks.cpp:215  tests/unit/ArchivePoolTest.cpp:340  tests/unit/LightingTest.cpp:87,89,93,95  tests/unit/OpeningTest.cpp:151,152,248,251  tests/unit/OverviewTest.cpp:213,214,464,468,472  tests/unit/SceneTest.cpp:28,141,177,201,228  tests/unit/TitleTest.cpp:302,307,313,342,348,365,367  tools/assets/ExportBranding.cpp:330,376  tools/migration/TitleSourceProbe.cpp:395,396` | 按适用工具链与接口语义评估现代替代；不把仍受支持的类型自动列作运行缺陷。 |
| DEP-5 QPair — use std::pair (alias since Qt 6.0) | 4 | `tests/unit/ArchivePoolTest.cpp:364,366  tests/unit/TitleTest.cpp:119,216` | 按适用工具链与接口语义评估现代替代；不把仍受支持的类型自动列作运行缺陷。 |
| DEP-7 qMin/qMax/qBound — use (std::min)()/(std::max)()/std::clamp() | 36 | `src/core/MusicTypes.cpp:25,32  src/render/ArchivePool.cpp:63  src/render/SelectionController.cpp:787  src/render/rhi/AtlasAllocator.cpp:19,114,137,146  src/render/rhi/AtlasUpload.cpp:40,47  src/render/rhi/PixelStorage.h:59  src/render/rhi/ResourcePlan.h:37,152  src/render/rhi/RhiRenderer.cpp:637,638,639  src/render/rhi/SceneItem.cpp:900  src/render/rhi/TransferAudit.h:43,45  src/services/CoverCache.cpp:292,378,398,424,452,455,508,587  tests/render/StartupChecks.cpp:79,83,332,442,712  tests/unit/DirectoryScannerTest.cpp:334  tests/unit/LibraryDataTest.cpp:294  tests/unit/MediaMetadataTest.cpp:107  tools/migration/RulerSourceProbe.cpp:594` | 按适用工具链与接口语义评估现代替代；不把仍受支持的类型自动列作运行缺陷。 |
| ENM-2 Unscoped enum without explicit underlying type — add type to prevent BiC | 22 | `src/app/DeploymentProbe.cpp:31  src/app/DetailReview.cpp:58,64  src/app/MaterialReview.cpp:54  src/app/NavigationReview.cpp:39,47  src/app/OverviewReview.cpp:63,69  src/app/main.cpp:69  src/core/LibraryModels.h:51,106  src/media/MediaMetadataProbe.h:14  src/motion/MotionController.h:12  src/platform/macos/ApplicationMenu.mm:29  src/presentation/DetailModel.h:25  src/render/OpticalParameters.h:11  src/render/OverviewLabelsModel.h:30  src/render/rhi/DrawUniform.h:13  src/render/rhi/Postprocess.cpp:24  src/render/rhi/ReadbackImage.cpp:12  src/render/rhi/ReadbackQueue.h:13  src/render/rhi/RhiRenderer.cpp:37` | 明确枚举/布尔参数含义，按项目接口约定调整。 |
| ENM-9 Non-intuitive bool literal arguments — confusing at call site; use named enums | 69 | `src/app/DetailReview.cpp:371  src/presentation/PresentationController.cpp:48,50  src/presentation/TrackFocusController.cpp:77  src/render/rhi/ResourcePlan.h:55  src/render/text/TextLayoutAdapter.cpp:248,273,281,382  src/services/OnlineMetadataProvider.cpp:71  src/services/metadata/MetadataCoordinator.cpp:149  tests/render/LibraryChecks.cpp:84  tests/render/RhiOpaqueChecks.cpp:183,211,222  tests/render/RhiShadowChecks.cpp:444,446,448,450,452,456,458,460,461,463,465,467,469,470,472,474,475  tests/render/SmaaReference.h:119,120,130,131  tests/render/StartupChecks.cpp:475,1414  tests/unit/AudioAmbienceTest.cpp:257  tests/unit/DetailMotionTest.cpp:333,334  tests/unit/MediaMetadataTest.cpp:681  tests/unit/NavigationTest.cpp:71,99  tests/unit/PresentationTest.cpp:171,219,227  tests/unit/RealColumnsTest.cpp:111,252,253,254,275  tests/unit/RenderSupportTest.cpp:83,91,98  tests/unit/RhiMaterialTest.cpp:127,133  tests/unit/SceneTest.cpp:44,250  tests/unit/VerificationWindowTest.cpp:29,30,31,32,33,34,35,36  tools/migration/RulerSourceProbe.cpp:273,539` | 明确枚举/布尔参数含义，按项目接口约定调整。 |
| ENM-9 Two or more bool parameters — boolean trap; use named enums (f(x, LogRule::DontLog)) | 41 | `src/presentation/DetailModel.cpp:40,104  src/presentation/PresentationController.cpp:201  src/presentation/PresentationController.h:70  src/presentation/SurfaceTransition.cpp:41,54  src/presentation/SurfaceTransition.h:23,54  src/render/ArchiveField.cpp:45  src/render/ArchiveField.h:29  src/render/CaseClarity.cpp:71  src/render/CaseClarity.h:16  src/render/DetailMotion.cpp:57,70  src/render/DetailMotion.h:23,24  src/render/LayoutMetrics.cpp:56  src/render/PointerDragController.h:26  src/render/RealColumns.cpp:76  src/render/RealColumns.h:19  src/render/SelectionController.cpp:72,840  src/render/navigation/RulerController.cpp:409  src/render/navigation/RulerController.h:54,55  src/render/rhi/FrameUniforms.cpp:114  src/render/rhi/FrameUniforms.h:12,27  src/render/rhi/ReadbackImage.cpp:21  src/render/rhi/ReadbackImage.h:11  src/render/rhi/RenderTarget.cpp:27  src/render/rhi/RenderTarget.h:40  src/render/rhi/ResourcePlan.h:41  src/render/text/TextLayoutAdapter.cpp:358  src/services/PreferencesStore.cpp:268  src/services/PreferencesStore.h:29  tests/render/RhiOpaqueChecks.cpp:23  tests/render/SmaaReference.h:63  tests/render/StartupChecks.cpp:127,1470  tests/render/StartupChecks.h:13` | 明确枚举/布尔参数含义，按项目接口约定调整。 |
| ERR-1 QFile::open() return not checked — silent failure if file cannot open | 59 | `src/library/ArtworkStore.cpp:82,122  src/render/rhi/PreparedAssets.cpp:25  tests/integration/LibraryQmlTest.cpp:115  tests/render/ArchivePoolChecks.cpp:254,368  tests/render/RhiAoChecks.cpp:27,138,196  tests/render/RhiBokehChecks.cpp:182  tests/render/RhiFlickerChecks.cpp:74  tests/render/RhiFloorChecks.h:13  tests/render/RhiFogChecks.cpp:41  tests/render/RhiReviewInput.h:22  tests/render/RhiShadowChecks.cpp:489  tests/render/RhiShellChecks.cpp:72  tests/render/StartupChecks.cpp:838,848,1410  tests/unit/ArchivePoolTest.cpp:135,417  tests/unit/AudioAmbienceTest.cpp:44,161,188,226,266  tests/unit/CoverCacheTest.cpp:813  tests/unit/DetailMotionTest.cpp:116,169  tests/unit/DirectoryScannerTest.cpp:202  tests/unit/JsonFileTest.cpp:23,35  tests/unit/LibraryDataTest.cpp:434,438,765  tests/unit/LightingTest.cpp:100  tests/unit/LoggingTest.cpp:106,136  tests/unit/MediaMetadataTest.cpp:66,386  tests/unit/OnlineMetadataTest.cpp:327,351,997  tests/unit/OpeningTest.cpp:77  tests/unit/RealColumnsTest.cpp:119,246,256,268,278  tests/unit/RhiMaterialTest.cpp:160,210  tests/unit/SceneTest.cpp:90  tests/unit/SingleInstanceTest.cpp:99  tools/assets/ExportBranding.cpp:66,73,80,197  tools/assets/GeometryAudit.cpp:129,136` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-2 QJsonDocument::fromJson() not validated — check isNull()/isObject() | 41 | `src/app/MaterialReview.cpp:402  src/media/InterfaceSoundController.cpp:32  src/render/opening/OpeningInk.cpp:220  tests/integration/AssetTest.cpp:44,71,103  tests/integration/LibraryQmlTest.cpp:117  tests/render/ArchivePoolChecks.cpp:256  tests/render/RhiAoChecks.cpp:139,198  tests/render/RhiBokehChecks.cpp:183  tests/render/RhiFlickerChecks.cpp:75  tests/render/RhiFogChecks.cpp:42  tests/render/RhiShadowChecks.cpp:490  tests/unit/ArchivePoolTest.cpp:136,419  tests/unit/AudioAmbienceTest.cpp:163  tests/unit/DetailMotionTest.cpp:117,171  tests/unit/DirectoryScannerTest.cpp:358  tests/unit/JsonFileTest.cpp:24  tests/unit/LibraryDataTest.cpp:436,439,767  tests/unit/LightingTest.cpp:101  tests/unit/MediaMetadataTest.cpp:649,665  tests/unit/OpeningTest.cpp:78  tests/unit/OverviewTest.cpp:66  tests/unit/RealColumnsTest.cpp:120,257  tests/unit/RhiMaterialTest.cpp:161,211  tests/unit/SceneTest.cpp:91  tests/unit/SettingsTest.cpp:115,163  tools/assets/ExportBranding.cpp:558,796  tools/assets/PrepareAssets.cpp:86,94,231` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-3 QNetworkReply data read without prior error check | 1 | `src/services/metadata/MetadataHttp.cpp:193` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-4 Hardcoded http:// URL — use https:// | 1 | `tools/assets/PrepareAssets.cpp:351` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-6 QString::arg() has %2 but only 1 .arg() calls | 1 | `src/presentation/DetailModel.cpp:165` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-6 QString::arg() has %3 but only 1 .arg() calls | 2 | `src/render/text/TextLayoutAdapter.cpp:202,254` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-6 QString::arg() has %4 but only 1 .arg() calls | 1 | `tests/render/StartupChecks.cpp:1552` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-6 QString::arg() has %5 but only 1 .arg() calls | 1 | `tests/render/OpeningChecks.cpp:244` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| ERR-9 QNetworkAccessManager without sslErrors handling | 15 | `src/services/OnlineMetadataProvider.cpp:22  src/services/OnlineMetadataProvider.h:8,19  src/services/metadata/MetadataHttp.cpp:12,47,49,60,71  src/services/metadata/MetadataHttp.h:10,25  src/services/metadata/MetadataWorker.cpp:64,112  src/services/metadata/MetadataWorker.h:8,20  tests/unit/OnlineMetadataTest.cpp:200` | 核对真实错误检查、上下文与失败传播，再决定修改；原始命中完整保留。 |
| HDR-3 Unprotected numeric_limits::min/max — use (std::numeric_limits<T>::min)() | 16 | `src/core/MusicTypes.cpp:89  src/render/RenderQuality.cpp:41,42  src/render/rhi/AtlasAllocator.cpp:17,20,140  src/render/rhi/ReadbackImage.cpp:33  src/render/text/ReelInk.cpp:37  src/services/CoverCache.cpp:268  tests/unit/CoverCacheTest.cpp:88,349,394  tests/unit/LibraryDataTest.cpp:800,801  tools/assets/GeometryAudit.cpp:80,291` | 核对自包含及 Windows 宏边界；本项目已配置 NOMINMAX。 |
| HDR-3 Unprotected std::min/max — use (std::min)(a,b) for Windows macro safety | 242 | `src/app/DetailReview.cpp:149,151,548,550  src/app/MaterialReview.cpp:215,221,246,280  src/app/OverviewReview.cpp:145,536  src/core/LibraryOrder.cpp:36,91,126  src/core/Logging.cpp:257  src/core/SearchListModel.cpp:253  src/media/BackgroundMusicController.cpp:161  src/media/PlaybackController.cpp:557  src/motion/FrameClock.cpp:60  src/motion/MotionFunctions.cpp:114,146  src/motion/QuinticTrack.cpp:37,62  src/motion/SampledReelCurve.h:24,52  src/motion/ScalarSpring.h:18  src/platform/macos/DisplayMonitor.mm:40  src/platform/windows/DisplayMonitor.cpp:124  src/presentation/DetailModel.cpp:286,287,300,301  src/presentation/GenreRowsModel.cpp:85,117  src/presentation/OptionMetrics.cpp:61  src/presentation/PresentationController.cpp:116,127  src/presentation/SurfaceTransition.cpp:66,92  src/presentation/TrackFocusController.cpp:29,117,155,187  src/render/CaseClarity.cpp:89  src/render/LayoutMetrics.cpp:30,54,60,61  src/render/OverviewController.cpp:235,240,311,324,336  src/render/OverviewLabelsModel.cpp:214,234,328,329  src/render/RealColumns.cpp:39,41,50,92  src/render/RenderQuality.cpp:26,44,54  src/render/RowLightController.cpp:32,34  src/render/SceneAdapter.cpp:195,198,201,206,434,436  src/render/SceneMath.h:82,83  src/render/SelectionController.cpp:623,627,790  src/render/SelectionLighting.cpp:236  src/render/navigation/InputRouter.cpp:153,287,288  src/render/navigation/OrdinalMotion.cpp:39,78  src/render/navigation/RulerController.cpp:238,239,244,282  src/render/opening/OpeningController.cpp:75,93  src/render/opening/OpeningFrame.cpp:62,111,159,180,181  src/render/opening/OpeningInk.cpp:82,116,174  src/render/rhi/AtlasUpload.cpp:76  src/render/rhi/DiagnosticSamples.h:34,40  src/render/rhi/FrameUniforms.cpp:82  src/render/rhi/Postprocess.cpp:193  src/render/rhi/RhiItem.cpp:65  src/render/rhi/RhiRenderer.cpp:464,724,741,752  src/render/rhi/SceneCovers.cpp:159,162,167  src/render/rhi/SceneGeometryData.cpp:38,39  src/render/rhi/SceneItem.cpp:833,856  src/render/text/TextLayoutAdapter.cpp:98,130,134,178,205,208,209,389,392  src/render/text/TitleReel.cpp:36,38,45,72,164,169,171,243,247,259,260,262,273  src/render/text/TransportMotion.cpp:65  src/services/CoverCache.cpp:83,91,105,108,376,548  src/services/metadata/MetadataHttp.cpp:249  tests/render/ArchivePoolChecks.cpp:288,392,511  tests/render/CoverSamplingChecks.h:32,33,167  tests/render/ExperimentChecks.cpp:201  tests/render/LibraryChecks.cpp:232,594,630,696,814  tests/render/OpeningChecks.cpp:145  tests/render/PresentationChecks.cpp:385,386  tests/render/RhiAoChecks.cpp:91,106,113  tests/render/RhiBokehChecks.cpp:148  tests/render/RhiFloorChecks.h:77,116,155,205,206,227  tests/render/RhiFogChecks.cpp:86,89  tests/render/RhiOpaqueChecks.cpp:75,77,153,157,161,170,186,196,202  tests/render/RhiPostprocessChecks.cpp:298  tests/render/RhiShadowChecks.cpp:62,63,64,65,107,108,197  tests/render/RhiShellChecks.cpp:169,215,320,323  tests/render/RhiSmaaChecks.cpp:117  tests/render/SmaaReference.h:42,53,163  tests/support/OffscreenSurface.h:203  tests/unit/DetailMotionTest.cpp:141,195  tests/unit/LightingTest.cpp:108,135,142,144,147,151,155,167,169,179  tests/unit/MotionTest.cpp:165,166  tests/unit/OnlineMetadataTest.cpp:188  tests/unit/OpeningTest.cpp:144  tests/unit/OverviewTest.cpp:167,213,293  tests/unit/QmlBehaviorTest.cpp:237  tests/unit/RealColumnsTest.cpp:151  tests/unit/SceneTest.cpp:166,210,273  tools/assets/ExportBranding.cpp:127,128,129,130,379  tools/assets/GeometryAudit.cpp:84,85,299,310  tools/migration/RulerSourceProbe.cpp:73,82,513,516,526,631  tools/migration/TitleSourceProbe.cpp:60,73,131,188,317,740,820` | 核对自包含及 Windows 宏边界；本项目已配置 NOMINMAX。 |
| INC-2 C++ standard header before Qt header — reorder by descending specificity | 12 | `src/app/FixedTextureData.cpp:1  src/core/SearchListModel.cpp:3  src/library/LibraryStore.cpp:2  src/render/rhi/Postprocess.cpp:2  src/render/rhi/ReviewGlCapabilities.h:16  src/render/rhi/RhiRenderer.cpp:18  src/services/PreferencesStore.cpp:2  tests/render/RhiAoChecks.cpp:11  tests/unit/CoverCacheTest.cpp:9  tests/unit/RenderSupportTest.cpp:5  tests/unit/RhiMaterialTest.cpp:3  tools/assets/ExportBranding.cpp:19` | 遵守项目 include 顺序与自包含要求。 |
| LCY-1 QNetworkReply read without deleteLater() in this handler | 1 | `src/services/metadata/MetadataHttp.cpp:193` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-3 Q_ASSERT wraps side-effectful call — compiled out in release | 6 | `src/library/LibraryStore.cpp:256,283,310,319  src/render/StartupCoordinator.cpp:49  src/services/LibrarySession.cpp:73` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-5 m_connections grows without size cap — unbounded memory growth | 6 | `src/media/PlaybackController.cpp:206,211,219,226,240,251` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-5 m_entries grows without size cap — unbounded memory growth | 1 | `src/render/rhi/ResourceInventory.h:69` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-5 m_generation grows without size cap — unbounded memory growth | 1 | `src/library/LibraryStore.cpp:249` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-5 m_order grows without size cap — unbounded memory growth | 1 | `tests/render/PresentationChecks.cpp:51` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| LCY-5 m_path grows without size cap — unbounded memory growth | 1 | `src/library/LibraryStore.cpp:473` | 沿所有者、释放、调用者与队列上界复核；标量计数器不等于内存增长。 |
| PAT-11 QRegularExpression constructed inside loop — compile once before loop | 2 | `src/render/opening/BrandPaths.h:45  src/services/metadata/WikipediaSource.cpp:90` | 按真实读写、容器/正则生命周期及依赖决定是否调整。 |
| PAT-12 Non-const range-for reference — may trigger COW detach; use const auto& if read-only | 46 | `src/library/DirectoryScanner.cpp:156,439  src/media/InterfaceSoundController.cpp:43,66,74,95,105  src/media/MediaMetadataProbe.cpp:124,133,239,255,320,367  src/motion/MotionController.cpp:85  src/render/ArchiveField.cpp:40,55  src/render/OverviewLabelsModel.cpp:89,275,401  src/render/SelectionController.cpp:799,932  src/render/navigation/RulerController.cpp:321  src/render/rhi/AtlasAllocator.cpp:187,193,197  src/render/rhi/AtlasUpload.cpp:64  src/render/rhi/Postprocess.cpp:293  src/render/rhi/ReadbackQueue.cpp:63  src/render/rhi/RenderTarget.h:49,51  src/render/rhi/SceneCovers.cpp:172,174,235,299,448,474  src/render/rhi/SceneItem.cpp:519  src/render/text/TextLayoutAdapter.cpp:371  tests/render/OutputReference.h:26  tests/render/SmaaReference.h:35,36  tests/unit/AtlasUploadTest.cpp:118  tests/unit/AudioAmbienceTest.cpp:194  tests/unit/LibraryDataTest.cpp:102  tools/assets/GeometryAudit.cpp:255,317` | 按真实读写、容器/正则生命周期及依赖决定是否调整。 |
| PAT-2 std::optional default-constructed — use std::nullopt explicitly (GCC warning bug) | 29 | `src/app/ApplicationOptions.h:40  src/core/MusicTypes.h:25,54  src/presentation/PresentationController.cpp:52,143  src/presentation/SurfaceTransition.h:61  src/render/ArchiveField.h:21  src/render/CaseClarity.h:26  src/render/ReturningCasePool.h:16  src/render/RowLightController.h:54  src/render/SceneAdapter.cpp:108  src/render/SelectionController.cpp:90,131,132,977,978  src/render/SelectionLighting.h:204  src/render/navigation/InputRouter.h:70  src/render/opening/OpeningController.h:104  src/render/rhi/RhiRenderer.cpp:291  src/render/rhi/SceneItem.cpp:71,72,126,128  src/render/text/TitleReel.h:98  src/services/PreferencesStore.cpp:45  src/services/metadata/MetadataWorker.cpp:110,140  tests/render/CaptureMailbox.h:17` | 按真实读写、容器/正则生命周期及依赖决定是否调整。 |
| PAT-7 QMap usage — verify copying is needed; std::map saves ~1.7KiB text | 10 | `src/app/AssetCatalog.cpp:24,26,51  src/render/rhi/AtlasUpload.h:61  tests/render/StartupChecks.cpp:1233,1265,1279  tools/assets/ExportBranding.cpp:654  tools/assets/PrepareAssets.cpp:98,310` | 按真实读写、容器/正则生命周期及依赖决定是否调整。 |
| TMO-1 Integer timeout/interval parameter — use QDeadlineTimer or std::chrono | 3 | `src/motion/SampledReelCurve.h:10  tests/render/StartupChecks.cpp:408  tests/unit/MediaMetadataTest.cpp:39` | 区分采样/时长数值与 Qt 调度 API，适用时使用具名时长类型。 |
| VAR-3 Direct brace initialization — prefer copy-init (var = {...}) for safety | 134 | `src/app/DeploymentProbe.cpp:70,113,148  src/app/DetailReview.cpp:158,582,731  src/app/MaterialReview.cpp:294,435  src/app/NavigationReview.cpp:279,395  src/app/OverviewReview.cpp:150,540,668  src/app/VerificationWindow.h:70  src/core/LibraryOrder.cpp:92  src/core/MusicTypes.cpp:205  src/library/DirectoryScanner.cpp:98  src/media/PlaybackController.cpp:487  src/motion/MotionController.h:47,113  src/platform/windows/DisplayMonitor.cpp:63,86,94,116  src/presentation/DetailModel.cpp:386  src/presentation/PanelController.h:41  src/presentation/PresentationController.cpp:47,49  src/render/OverviewController.cpp:70  src/render/OverviewLabelsModel.cpp:75  src/render/RealColumns.h:38  src/render/RenderQuality.h:54  src/render/SceneAdapter.cpp:218,220  src/render/SelectionController.cpp:783  src/render/navigation/OrdinalMotion.h:21  src/render/opening/BrandPaths.h:83  src/render/opening/OpeningFrame.cpp:188  src/render/opening/OpeningInk.h:72  src/render/rhi/AtlasAllocator.cpp:118  src/render/rhi/FrameUniforms.cpp:117  src/render/rhi/PixelStorage.h:56  src/render/rhi/Postprocess.cpp:104,295  src/render/rhi/RhiRenderer.cpp:954,1027  src/render/rhi/SceneCovers.cpp:65,82  src/render/rhi/SceneGeometryData.cpp:32  src/render/rhi/SceneGeometryData.h:12  src/render/rhi/TransferAudit.h:22  src/render/text/TextLayoutAdapter.cpp:224  src/render/text/TitleReel.h:29,71  src/services/MetadataPresentation.cpp:110  tests/render/ArchivePoolChecks.cpp:345,601,624  tests/render/CoverChecks.cpp:216  tests/render/CoverSamplingChecks.h:29,62  tests/render/LibraryChecks.cpp:449,1059  tests/render/OpeningChecks.cpp:159,160,161,162,328  tests/render/OverviewChecks.cpp:63,276  tests/render/PresentationChecks.cpp:308,391,490  tests/render/RhiBokehChecks.cpp:53,130  tests/render/RhiPostprocessChecks.cpp:518,526  tests/render/RhiShadowChecks.cpp:55  tests/render/SmaaReference.h:79  tests/render/StartupChecks.cpp:554,557,563,564,565,566,567,568,569,571,572,573,574,575,576,811,1392,1476  tests/unit/DetailMotionTest.cpp:32,33,35,36  tests/unit/LibraryDataTest.cpp:79  tests/unit/MotionTest.cpp:87  tests/unit/NavigationTest.cpp:80  tests/unit/OpeningTest.cpp:27,28,29  tests/unit/OverviewTest.cpp:29,30,31,33,34,39,355  tests/unit/PlaybackTest.cpp:29,30  tests/unit/PresentationTest.cpp:51,71  tests/unit/RenderSupportTest.cpp:213  tests/unit/StartupTest.cpp:32,33,34,35  tests/unit/TrackFocusTest.cpp:24,25,27,28,29  tools/assets/ExportBranding.cpp:392,756  tools/assets/GeometryAudit.cpp:142,188,205,220` | 按项目初始化和类型安全约定评估，保持语义不变。 |

---

### Deep analysis findings

#### [D-020] [P3] Phase枚举末项缺尾逗号

- **File**: [src/render/opening/OpeningController.h:93](src/render/opening/OpeningController.h#L93)
- **Category**: A4. API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: 触发：Qt review ENM-1 风格检查。 影响：Qt review ENM-1的明确风格建议，无当前运行时影响；无需为此单独功能测试。
- **Trace**: Phase 枚举完整声明。
- **Mitigation**: 如采用该 skill 风格建议，添加末项尾逗号；没有当前运行影响。



---

### Investigation targets (human verification needed)

#### [I-001] 无输出期间InvalidMedia与errorOccurred处理不同

- **File**: [src/media/PlaybackController.cpp:329](src/media/PlaybackController.cpp#L329)
- **Category**: 深审待核实
- **Confidence**: 70/100
- **Finding / Unverified because**: status写错误并Paused，阻止restoreOutput自动恢复；但有效媒体由设备丢失触发InvalidMedia的生产可达条件未确认。Qt FFmpeg官方源码仅加载失败发此状态，不能把注入通知当设备事实。
- **How to verify**: 对应实际后端捕获无输出→InvalidMedia事件序列，区分既有格式错误与可恢复资源错误；确认可达再修。
- **Source**: [Qt 官方实现](https://raw.githubusercontent.com/qt/qtmultimedia/dev/src/plugins/multimedia/ffmpeg/qffmpegmediaplayer.cpp)


#### [I-002] 遮挡早退可能推迟未完成的Review捕获

- **File**: [src/render/rhi/RhiRenderer.cpp:175](src/render/rhi/RhiRenderer.cpp#L175)
- **Category**: 深审待核实
- **Confidence**: 72/100
- **Finding / Unverified because**: sceneOccluded清屏早退跳过renderFrame中的readbacks.retire/drain；捕获是否滞留取决于后端回调及其他帧；生产默认关闭捕获。
- **How to verify**: 现有Review捕获提交后立即切遮挡且停止其他更新，核对不恢复场景时是否交付；仅复核异步读回后端。


#### [I-003] 图层物理尺寸窄化前没有完整范围检查

- **File**: [src/render/rhi/SceneItem.cpp:872](src/render/rhi/SceneItem.cpp#L872)
- **Category**: 深审待核实
- **Confidence**: 65/100
- **Finding / Unverified because**: setOpeningLayerSize任意QSize乘DPR后直接转int，panelLayerSize类似；已确认正常生产尺寸来自QML布局，尚未证明正常窗口可达INT_MAX级输入。
- **How to verify**: 先明确内部属性输入域；若承担全QSize边界，用INT_MAX×DPR2最小隐藏测试/UBSan核对，窄化前验证。


#### [I-004] 激活ACK一次readAll假定读到完整行

- **File**: [src/services/SingleInstance.cpp:90](src/services/SingleInstance.cpp#L90)
- **Category**: 深审待核实
- **Confidence**: 72/100
- **Finding / Unverified because**: 客户端waitForReadyRead后一次readAll对比ok\n；服务端只一次write三字节，尚未确认真实Qt/OS路径可短读。
- **How to verify**: 隔离数据根/锁/端点，按协议分段发o与k\n且总时间未超时；确认客户端行为，按总截止时间累积完整行。



---

### Summary

| Category | Lint | Deep | Investigate | Total |
|---|---:|---:|---:|---:|
| DEP | 91 | 0 | 0 | 91 |
| ENM | 132 | 0 | 0 | 132 |
| ERR | 122 | 0 | 0 | 122 |
| HDR | 258 | 0 | 0 | 258 |
| INC | 12 | 0 | 0 | 12 |
| LCY | 17 | 0 | 0 | 17 |
| PAT | 87 | 0 | 0 | 87 |
| TMO | 3 | 0 | 0 | 3 |
| VAR | 134 | 0 | 0 | 134 |
| A3. Thread Safety | 0 | 1 | 0 | 1 |
| A4. API, Naming & C++ Correctness | 0 | 2 | 0 | 2 |
| A5. Error Handling & Validation | 0 | 6 | 0 | 6 |
| 待核实 | 0 | 0 | 4 | 4 |
| **Total** | **856** | **9** | **4** | **869** |

Findings below confidence 60 are suppressed.


<a id="qml-skill-report"></a>

## 附录 B · QML Code Review Report

**Scope**: files: native qml 全量；冻结 Web 仅作来源参照。
**Files reviewed**: 93
**Issues found**: 220 (220 from lint, 0 from deep analysis)
**Agents**: 6 (按行数分片；跨片检查 best effort，具体发现均另追踪调用链)
**qmllint**: ran — 官方 CMake Yalahoo_qmllint / ui-tests_qmllint 均通过。

计数保留工具原始命中；它不是已确认功能缺陷数量。项目契约优先于通用启发式规则。

---

### Lint findings

| Rule | Count | Locations | Mitigation |
|---|---:|---|---|
| BND-1 property var -- use a typed property (int, string, etc.) for qmlsc compilation and type safety | 25 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:14,75  qml/Yalahoo/Music/components/ArchiveEditMenu.qml:10,64  qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:7  qml/Yalahoo/Music/components/BackdropBlurPass.qml:14  qml/Yalahoo/Music/components/BrowseInfo.qml:13  qml/Yalahoo/Music/components/CreditCard.qml:10  qml/Yalahoo/Music/components/DetailAlbumInfo.qml:10,153  qml/Yalahoo/Music/components/DetailHeader.qml:10  qml/Yalahoo/Music/components/DetailIntroduction.qml:9  qml/Yalahoo/Music/components/DetailTabsBackdrop.qml:24,31  qml/Yalahoo/Music/components/DisplaySurface.qml:13  qml/Yalahoo/Music/pages/Browse.qml:19  qml/Yalahoo/Music/panels/LibraryPanel.qml:142  qml/Yalahoo/Music/panels/LightingModes.qml:38  qml/Yalahoo/Music/panels/LightingPanel.qml:112  qml/Yalahoo/Music/panels/SearchFilters.qml:62,71  qml/Yalahoo/Music/panels/SettingChoice.qml:10,11  qml/Yalahoo/Music/panels/SoundSettings.qml:10,22` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-2 Imperative '=' on 'activeTab' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/pages/Detail.qml:37` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-2 Imperative '=' on 'editMenu.x' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 2 | `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:37,45` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-2 Imperative '=' on 'editMenu.y' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 2 | `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:38,46` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-2 Imperative '=' on 'root.activeTab' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/pages/Detail.qml:78` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-2 Imperative '=' on 'roots.text' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/panels/LibraryPanel.qml:18` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| BND-5 list<> property has no granular change signals -- add/move/remove won't notify; consider a ListModel or notify manually | 1 | `qml/Yalahoo/Music/components/DesignTokens.qml:14` | 采用有意义的类型与持续绑定；异构记录/sampler 可保留 var，临时编辑需隔离草稿。 |
| IMG-1 Image without sourceSize -- decodes full resolution into GPU memory; set sourceSize to display dimensions | 1 | `tests/render/OverviewVisual.qml:19` | 按实际图片用途限制解码尺寸，并核对错误处理。 |
| IMP-4 Non-Qt import before Qt import -- order: Qt modules, third-party, local C++, QML folders | 1 | `qml/Yalahoo/Music/components/ArchiveButton.qml:4` | 遵守当前模块/导入契约，保持解析身份不变。 |
| ORD-1 attached property appears after child object | 4 | `qml/Yalahoo/Music/components/Header.qml:72  qml/Yalahoo/Music/components/TrackRow.qml:159  qml/Yalahoo/Music/panels/LibraryPanels.qml:128  qml/Yalahoo/Music/panels/MetadataSection.qml:76` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 attached property appears after signal handler | 5 | `qml/Yalahoo/Music/components/CreditCard.qml:54  qml/Yalahoo/Music/components/PlaybackControls.qml:64  qml/Yalahoo/Music/panels/LightingPanel.qml:153  qml/Yalahoo/Music/panels/SettingChoice.qml:39  qml/Yalahoo/Music/panels/SettingRange.qml:62` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 id appears after child object | 2 | `qml/Yalahoo/Music/components/DetailTabs.qml:25  qml/Yalahoo/Music/scene/CaseMaterials.qml:16` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 id appears after property assignment | 11 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:74  qml/Yalahoo/Music/components/ArchiveTextArea.qml:29  qml/Yalahoo/Music/components/ArchiveTextField.qml:28  qml/Yalahoo/Music/components/OrdinalReel.qml:57  qml/Yalahoo/Music/components/Ruler.qml:30  qml/Yalahoo/Music/components/ThemeSwitch.qml:35  qml/Yalahoo/Music/components/TitleReel.qml:70,107  qml/Yalahoo/Music/panels/LightingModes.qml:37  qml/Yalahoo/Music/panels/SearchFilters.qml:61,68` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property assignment appears after attached property | 5 | `qml/Yalahoo/Music/components/ColumnNavigation.qml:86  qml/Yalahoo/Music/components/NavigationFrame.qml:144,157  qml/Yalahoo/Music/components/TitleReel.qml:125  qml/Yalahoo/Music/panels/SearchFilters.qml:45` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property assignment appears after child object | 7 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:25  qml/Yalahoo/Music/components/DesignTokens.qml:56  qml/Yalahoo/Music/components/Header.qml:76  qml/Yalahoo/Music/components/OrdinalReel.qml:22  qml/Yalahoo/Music/pages/Overview.qml:55  qml/Yalahoo/Music/scene/CaseMaterials.qml:40  qml/Yalahoo/Music/scene/GlassMaterial.qml:17` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property assignment appears after signal handler | 7 | `qml/Yalahoo/Music/components/TrackList.qml:117  qml/Yalahoo/Music/pages/MusicStage.qml:70  qml/Yalahoo/Music/panels/MetadataSettings.qml:39,55,75,109,117` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property declaration appears after child object | 2 | `qml/Yalahoo/Music/panels/SoundSettings.qml:10,22` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property declaration appears after property assignment | 35 | `qml/Yalahoo/Music/components/ArchiveEditMenu.qml:9,64  qml/Yalahoo/Music/components/ArchiveSlider.qml:11  qml/Yalahoo/Music/components/ArchiveTextArea.qml:10  qml/Yalahoo/Music/components/ArchiveTextField.qml:10  qml/Yalahoo/Music/components/BackdropBlurPass.qml:14  qml/Yalahoo/Music/components/CreditCard.qml:17,76  qml/Yalahoo/Music/components/DetailAlbumInfo.qml:153  qml/Yalahoo/Music/components/DetailTabsBackdrop.qml:24,31  qml/Yalahoo/Music/components/DisplaySurface.qml:13  qml/Yalahoo/Music/components/OpeningBackdrop.qml:16,27  qml/Yalahoo/Music/components/OpeningFilm.qml:15  qml/Yalahoo/Music/components/OrdinalReel.qml:50  qml/Yalahoo/Music/components/TrackList.qml:17,183,190  qml/Yalahoo/Music/components/TrackRow.qml:22  qml/Yalahoo/Music/components/TransportTitle.qml:10  qml/Yalahoo/Music/panels/ExperimentPanels.qml:10  qml/Yalahoo/Music/panels/LibraryPanel.qml:142  qml/Yalahoo/Music/panels/LibraryPanels.qml:47,85  qml/Yalahoo/Music/panels/LightingPanel.qml:9,112  qml/Yalahoo/Music/panels/MetadataSettings.qml:9  qml/Yalahoo/Music/panels/MotionPanel.qml:9  qml/Yalahoo/Music/panels/PlaybackPanel.qml:9  qml/Yalahoo/Music/panels/SearchResult.qml:19  qml/Yalahoo/Music/panels/SettingsPanel.qml:12  qml/Yalahoo/Music/panels/ThemeCard.qml:11  qml/Yalahoo/Music/scene/CaseMaterials.qml:78  qml/Yalahoo/Music/scene/GeometryScene.qml:43` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 property declaration appears after signal declaration | 5 | `qml/Yalahoo/Music/components/OverviewLabel.qml:19  qml/Yalahoo/Music/panels/LibraryPanels.qml:23  qml/Yalahoo/Music/panels/PanelContents.qml:15  qml/Yalahoo/Music/panels/PanelHeading.qml:13  qml/Yalahoo/Music/panels/SearchFilters.qml:21` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 signal declaration appears after property assignment | 1 | `qml/Yalahoo/Music/pages/Detail.qml:68` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| ORD-1 signal handler appears after child object | 5 | `qml/Yalahoo/Music/components/DetailTabs.qml:127  qml/Yalahoo/Music/components/OverviewLabel.qml:135  qml/Yalahoo/Music/components/TrackList.qml:126  qml/Yalahoo/Music/panels/LibraryPanel.qml:179  qml/Yalahoo/Music/panels/SearchPanel.qml:125` | 结合项目保留声明顺序的要求评估；不机械改动行为。 |
| PRF-1 Rectangle with 'transparent' color -- use Item instead, or toggle visible on the Rectangle (creates geometry node even when transparent) | 17 | `qml/Yalahoo/Music/components/ArchiveButton.qml:48  qml/Yalahoo/Music/components/ArchiveCheckBox.qml:21,31  qml/Yalahoo/Music/components/ArchiveComboBox.qml:91  qml/Yalahoo/Music/components/ArchiveSlider.qml:44  qml/Yalahoo/Music/components/DetailAlbumInfo.qml:122  qml/Yalahoo/Music/components/OverviewLabel.qml:102,157  qml/Yalahoo/Music/components/Ruler.qml:48  qml/Yalahoo/Music/components/ThemeSwitch.qml:12,52,73  qml/Yalahoo/Music/pages/Opening.qml:104  qml/Yalahoo/Music/pages/Overview.qml:136,158  qml/Yalahoo/Music/panels/SearchFilters.qml:84  qml/Yalahoo/Music/panels/ThemeCard.qml:22` | 保留必要边框、裁切和源效果，按实际工作量判断；不为静态命中增加性能矩阵。 |
| PRF-1 Rectangle with 'transparent' in color expression -- use Item instead, or toggle visible (creates geometry node even when transparent) | 3 | `qml/Yalahoo/Music/components/ArchiveButton.qml:33  qml/Yalahoo/Music/panels/LightingModes.qml:52  qml/Yalahoo/Music/panels/SearchResult.qml:32` | 保留必要边框、裁切和源效果，按实际工作量判断；不为静态命中增加性能矩阵。 |
| PRF-3 clip: true disables scene graph batching -- verify this is needed (acceptable on ListView) | 20 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:123  qml/Yalahoo/Music/components/ArchiveScrollView.qml:13  qml/Yalahoo/Music/components/ArchiveTextArea.qml:17  qml/Yalahoo/Music/components/ArchiveTextField.qml:17  qml/Yalahoo/Music/components/BackdropBlur.qml:21  qml/Yalahoo/Music/components/BrowseInfo.qml:42  qml/Yalahoo/Music/components/CreditCard.qml:70  qml/Yalahoo/Music/components/OpeningFilm.qml:77  qml/Yalahoo/Music/components/OrdinalReel.qml:62  qml/Yalahoo/Music/components/OverviewLabel.qml:142  qml/Yalahoo/Music/components/Ruler.qml:13,25  qml/Yalahoo/Music/components/TitleReel.qml:24,115  qml/Yalahoo/Music/components/TrackList.qml:16,128  qml/Yalahoo/Music/components/TransportTitle.qml:23  qml/Yalahoo/Music/panels/ExperimentPanels.qml:21  qml/Yalahoo/Music/panels/SearchFilters.qml:55  qml/Yalahoo/Music/panels/SearchPanel.qml:49` | 保留必要边框、裁切和源效果，按实际工作量判断；不为静态命中增加性能矩阵。 |
| STY-1 Top-level component should have id: root (enables qualified lookup, future-proofs for QML 3) | 26 | `qml/Yalahoo/Music/App.qml:11  qml/Yalahoo/Music/components/ArchiveButton.qml:7  qml/Yalahoo/Music/components/ArchiveCheckBox.qml:7  qml/Yalahoo/Music/components/ArchiveComboBox.qml:10  qml/Yalahoo/Music/components/ArchiveEditMenu.qml:6  qml/Yalahoo/Music/components/ArchiveProgressBar.qml:6  qml/Yalahoo/Music/components/ArchiveScrollBar.qml:6  qml/Yalahoo/Music/components/ArchiveScrollView.qml:6  qml/Yalahoo/Music/components/ArchiveSlider.qml:6  qml/Yalahoo/Music/components/ArchiveTextArea.qml:6  qml/Yalahoo/Music/components/ArchiveTextField.qml:6  qml/Yalahoo/Music/components/ArchiveToolTip.qml:6  qml/Yalahoo/Music/components/ColumnNavigation.qml:6  qml/Yalahoo/Music/components/CreditCard.qml:7  qml/Yalahoo/Music/components/DetailTabs.qml:6  qml/Yalahoo/Music/components/HeaderTransport.qml:6  qml/Yalahoo/Music/components/OverviewLabel.qml:7  qml/Yalahoo/Music/components/TrackRow.qml:6  qml/Yalahoo/Music/pages/MusicStage.qml:11  qml/Yalahoo/Music/panels/SearchFilters.qml:8  qml/Yalahoo/Music/panels/SearchResult.qml:8  qml/Yalahoo/Music/scene/CaseMaterials.qml:6  qml/Yalahoo/Music/scene/GeometryScene.qml:9  qml/Yalahoo/Music/scene/GlassMaterial.qml:8  qml/Yalahoo/Music/scene/LightingMaterial.qml:5  tests/render/LibraryReview.qml:6` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 3 anchors.* properties using dot notation -- use group notation: anchors { ... } | 1 | `qml/Yalahoo/Music/panels/StagePanels.qml:114` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 3 font.* properties using dot notation -- use group notation: font { ... } | 4 | `qml/Yalahoo/Music/components/BrowseInfo.qml:163  qml/Yalahoo/Music/components/DetailAlbumInfo.qml:75,89  qml/Yalahoo/Music/components/NavigationFrame.qml:101` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 3 layer.* properties using dot notation -- use group notation: layer { ... } | 2 | `qml/Yalahoo/Music/components/DisplaySurface.qml:9  qml/Yalahoo/Music/components/TitleReel.qml:79` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 4 font.* properties using dot notation -- use group notation: font { ... } | 13 | `qml/Yalahoo/Music/components/ArchiveButton.qml:17  qml/Yalahoo/Music/components/ArchiveComboBox.qml:20  qml/Yalahoo/Music/components/NavigationFrame.qml:49,56,77  qml/Yalahoo/Music/components/OrdinalReel.qml:26  qml/Yalahoo/Music/pages/Opening.qml:69  qml/Yalahoo/Music/pages/Overview.qml:29,36,43,50  qml/Yalahoo/Music/panels/SearchFilters.qml:76  qml/Yalahoo/Music/panels/SearchPanel.qml:19` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 5 anchors.* properties using dot notation -- use group notation: anchors { ... } | 1 | `qml/Yalahoo/Music/panels/LibraryPanels.qml:108` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 5 font.* properties using dot notation -- use group notation: font { ... } | 1 | `qml/Yalahoo/Music/components/ArchiveText.qml:29` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-3 6 font.* properties using dot notation -- use group notation: font { ... } | 1 | `qml/Yalahoo/Music/components/ArchiveText.qml:62` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'frosted_Polymer_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:10` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'ivory_Edges_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:24` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'music_Case_V040__Frosted_Polymer' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:57` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'music_Case_V040__Ivory_Edges' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:65` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'music_Case_V040__Optical_Diffuser' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:73` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'optical_Diffuser_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:38` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |
| STY-6 id 'rhine_Music_Case_V040' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:54` | 依项目 QML 格式约定评估；导入模型资产保持原貌。 |

---

---

### Investigation targets (human verification needed)

#### [I-001] Tooltip 的宿主场景坐标可能不会随布局更新
- **File**: `qml/Yalahoo/Music/components/ArchiveToolTip.qml:14`
- **Category**: A1. Bindings & Properties
- **Confidence**: 70/100
- **Finding**: `parentPosition` 通过 `parent.mapToItem(null, 0, 0)` 计算，只显式依赖 `parent` 对象；宿主或其祖先移动时，该绑定可能不重新求值，导致第 16–21 行的水平边缘限制及上下方选择使用旧坐标。
- **Unverified because**: 调用链已确认 `OverviewLabel.qml:36–37,44` 的宿主会移动，`BrowseInfo.qml:27–37` 会响应视口尺寸改变；但 `T.Popup` 自身的 margins 和自动位置调整可能消除部分影响，静态审查不能确认最终可见偏差。
- **How to verify**: 将 overview 标签或专辑标题移到窗口右侧、底部，显示 tooltip 后调整窗口尺寸或展开 overview，核对 tooltip 是否继续靠近当前宿主、是否正确切换上下位置。若复现，应使用可跟踪场景坐标变化的定位方案，或在宿主几何变化及打开时更新坐标。


---

### Summary

| Category | Lint | Deep | Investigate | Total |
|---|---:|---:|---:|---:|
| BND | 33 | 0 | 0 | 33 |
| IMG | 1 | 0 | 0 | 1 |
| IMP | 1 | 0 | 0 | 1 |
| ORD | 89 | 0 | 0 | 89 |
| PRF | 40 | 0 | 0 | 40 |
| STY | 56 | 0 | 0 | 56 |
| 待核实 | 0 | 0 | 1 | 1 |
| **Total** | **220** | **0** | **1** | **221** |

Findings below confidence 60 are suppressed.


<a id="file-coverage"></a>

## 附录 C · 分片覆盖

以Git跟踪文件为准，排除冻结legacy-web及生成输出；全部分片完成后交付。

### cpp-1 · 2666 行 · 已完成

- [src/app/ApplicationIdentity.h](src/app/ApplicationIdentity.h)
- [src/app/ApplicationOptions.cpp](src/app/ApplicationOptions.cpp)
- [src/app/ApplicationOptions.h](src/app/ApplicationOptions.h)
- [src/app/AssetCatalog.cpp](src/app/AssetCatalog.cpp)
- [src/app/AssetCatalog.h](src/app/AssetCatalog.h)
- [src/app/DeploymentProbe.cpp](src/app/DeploymentProbe.cpp)
- [src/app/DeploymentProbe.h](src/app/DeploymentProbe.h)
- [src/app/DetailReview.cpp](src/app/DetailReview.cpp)
- [src/app/DetailReview.h](src/app/DetailReview.h)
- [src/app/FixedTextureData.cpp](src/app/FixedTextureData.cpp)
- [src/app/FixedTextureData.h](src/app/FixedTextureData.h)
- [src/app/GraphicsBackend.h](src/app/GraphicsBackend.h)
- [src/app/LibraryPersistence.cpp](src/app/LibraryPersistence.cpp)
- [src/app/LibraryPersistence.h](src/app/LibraryPersistence.h)
- [src/app/MaterialReview.cpp](src/app/MaterialReview.cpp)
- [src/app/MaterialReview.h](src/app/MaterialReview.h)
- [src/app/NavigationReview.cpp](src/app/NavigationReview.cpp)

### cpp-2 · 2539 行 · 已完成

- [src/app/NavigationReview.h](src/app/NavigationReview.h)
- [src/app/OverviewReview.cpp](src/app/OverviewReview.cpp)
- [src/app/OverviewReview.h](src/app/OverviewReview.h)
- [src/app/QmlTypes.h](src/app/QmlTypes.h)
- [src/app/ReviewApplication.h](src/app/ReviewApplication.h)
- [src/app/ReviewSupport.h](src/app/ReviewSupport.h)
- [src/app/VerificationWindow.h](src/app/VerificationWindow.h)
- [src/app/VerificationWindowMac.mm](src/app/VerificationWindowMac.mm)
- [src/app/main.cpp](src/app/main.cpp)
- [src/core/AlbumListModel.cpp](src/core/AlbumListModel.cpp)
- [src/core/AppIdentity.cpp](src/core/AppIdentity.cpp)
- [src/core/AppIdentity.h](src/core/AppIdentity.h)
- [src/core/ColorTransfer.h](src/core/ColorTransfer.h)
- [src/core/CoverPixels.h](src/core/CoverPixels.h)
- [src/core/JsonFile.h](src/core/JsonFile.h)
- [src/core/LibraryIndex.cpp](src/core/LibraryIndex.cpp)
- [src/core/LibraryIndex.h](src/core/LibraryIndex.h)
- [src/core/LibraryModels.h](src/core/LibraryModels.h)

### cpp-3 · 2478 行 · 已完成

- [src/core/LibraryOrder.cpp](src/core/LibraryOrder.cpp)
- [src/core/Localization.cpp](src/core/Localization.cpp)
- [src/core/Localization.h](src/core/Localization.h)
- [src/core/Logging.cpp](src/core/Logging.cpp)
- [src/core/Logging.h](src/core/Logging.h)
- [src/core/MetadataConfiguration.h](src/core/MetadataConfiguration.h)
- [src/core/MonotonicTime.h](src/core/MonotonicTime.h)
- [src/core/MusicTypes.cpp](src/core/MusicTypes.cpp)
- [src/core/MusicTypes.h](src/core/MusicTypes.h)
- [src/core/NumericValue.h](src/core/NumericValue.h)
- [src/core/ProductDefaults.h](src/core/ProductDefaults.h)
- [src/core/Require.h](src/core/Require.h)
- [src/core/SceneGeometry.h](src/core/SceneGeometry.h)
- [src/core/SearchListModel.cpp](src/core/SearchListModel.cpp)
- [src/core/Units.h](src/core/Units.h)
- [src/library/ArtworkStore.cpp](src/library/ArtworkStore.cpp)
- [src/library/ArtworkStore.h](src/library/ArtworkStore.h)
- [src/library/DirectoryScanner.cpp](src/library/DirectoryScanner.cpp)
- [src/library/DirectoryScanner.h](src/library/DirectoryScanner.h)
- [src/library/LegacyImporter.cpp](src/library/LegacyImporter.cpp)
- [src/library/LegacyImporter.h](src/library/LegacyImporter.h)

### cpp-4 · 2509 行 · 已完成

- [src/library/LibraryStore.cpp](src/library/LibraryStore.cpp)
- [src/library/LibraryStore.h](src/library/LibraryStore.h)
- [src/media/AudioOutputChanges.h](src/media/AudioOutputChanges.h)
- [src/media/AudioPreferencesBinding.cpp](src/media/AudioPreferencesBinding.cpp)
- [src/media/AudioPreferencesBinding.h](src/media/AudioPreferencesBinding.h)
- [src/media/BackgroundMusicController.cpp](src/media/BackgroundMusicController.cpp)
- [src/media/BackgroundMusicController.h](src/media/BackgroundMusicController.h)
- [src/media/BackgroundMusicController_p.h](src/media/BackgroundMusicController_p.h)
- [src/media/InterfaceSoundController.cpp](src/media/InterfaceSoundController.cpp)
- [src/media/InterfaceSoundController.h](src/media/InterfaceSoundController.h)
- [src/media/InterfaceSoundController_p.h](src/media/InterfaceSoundController_p.h)
- [src/media/MediaMetadataProbe.cpp](src/media/MediaMetadataProbe.cpp)
- [src/media/MediaMetadataProbe.h](src/media/MediaMetadataProbe.h)
- [src/media/PlaybackController.cpp](src/media/PlaybackController.cpp)
- [src/media/PlaybackController.h](src/media/PlaybackController.h)

### cpp-5 · 2500 行 · 已完成

- [src/media/PlaybackController_p.h](src/media/PlaybackController_p.h)
- [src/media/PlaybackQueue.cpp](src/media/PlaybackQueue.cpp)
- [src/media/PlaybackQueue.h](src/media/PlaybackQueue.h)
- [src/motion/EasingCurve.h](src/motion/EasingCurve.h)
- [src/motion/FrameClock.cpp](src/motion/FrameClock.cpp)
- [src/motion/FrameClock.h](src/motion/FrameClock.h)
- [src/motion/FrameConnection.h](src/motion/FrameConnection.h)
- [src/motion/MotionController.cpp](src/motion/MotionController.cpp)
- [src/motion/MotionController.h](src/motion/MotionController.h)
- [src/motion/MotionFunctions.cpp](src/motion/MotionFunctions.cpp)
- [src/motion/MotionFunctions.h](src/motion/MotionFunctions.h)
- [src/motion/OpeningTimeline.h](src/motion/OpeningTimeline.h)
- [src/motion/QuinticTrack.cpp](src/motion/QuinticTrack.cpp)
- [src/motion/QuinticTrack.h](src/motion/QuinticTrack.h)
- [src/motion/SampledReelCurve.h](src/motion/SampledReelCurve.h)
- [src/motion/ScalarSpring.cpp](src/motion/ScalarSpring.cpp)
- [src/motion/ScalarSpring.h](src/motion/ScalarSpring.h)
- [src/platform/linux/DisplayMonitor.cpp](src/platform/linux/DisplayMonitor.cpp)
- [src/platform/macos/ApplicationMenu.h](src/platform/macos/ApplicationMenu.h)
- [src/platform/macos/ApplicationMenu.mm](src/platform/macos/ApplicationMenu.mm)
- [src/platform/macos/DisplayMonitor.mm](src/platform/macos/DisplayMonitor.mm)
- [src/platform/windows/DisplayMonitor.cpp](src/platform/windows/DisplayMonitor.cpp)
- [src/presentation/DetailModel.cpp](src/presentation/DetailModel.cpp)
- [src/presentation/DetailModel.h](src/presentation/DetailModel.h)

### cpp-6 · 2491 行 · 已完成

- [src/presentation/GenreRowsModel.cpp](src/presentation/GenreRowsModel.cpp)
- [src/presentation/GenreRowsModel.h](src/presentation/GenreRowsModel.h)
- [src/presentation/OptionMetrics.cpp](src/presentation/OptionMetrics.cpp)
- [src/presentation/OptionMetrics.h](src/presentation/OptionMetrics.h)
- [src/presentation/PanelController.cpp](src/presentation/PanelController.cpp)
- [src/presentation/PanelController.h](src/presentation/PanelController.h)
- [src/presentation/PresentationController.cpp](src/presentation/PresentationController.cpp)
- [src/presentation/PresentationController.h](src/presentation/PresentationController.h)
- [src/presentation/SurfaceTransition.cpp](src/presentation/SurfaceTransition.cpp)
- [src/presentation/SurfaceTransition.h](src/presentation/SurfaceTransition.h)
- [src/presentation/TrackFocusController.cpp](src/presentation/TrackFocusController.cpp)
- [src/presentation/TrackFocusController.h](src/presentation/TrackFocusController.h)
- [src/render/ArchiveField.cpp](src/render/ArchiveField.cpp)
- [src/render/ArchiveField.h](src/render/ArchiveField.h)
- [src/render/ArchivePool.cpp](src/render/ArchivePool.cpp)
- [src/render/ArchivePool.h](src/render/ArchivePool.h)
- [src/render/CameraFraming.h](src/render/CameraFraming.h)
- [src/render/CanvasSupport.h](src/render/CanvasSupport.h)
- [src/render/CaseClarity.cpp](src/render/CaseClarity.cpp)
- [src/render/CaseClarity.h](src/render/CaseClarity.h)
- [src/render/ColumnMemory.cpp](src/render/ColumnMemory.cpp)
- [src/render/ColumnMemory.h](src/render/ColumnMemory.h)
- [src/render/DetailMotion.cpp](src/render/DetailMotion.cpp)
- [src/render/DetailMotion.h](src/render/DetailMotion.h)
- [src/render/DetailPorts.cpp](src/render/DetailPorts.cpp)
- [src/render/DetailPorts.h](src/render/DetailPorts.h)
- [src/render/LayoutMetrics.cpp](src/render/LayoutMetrics.cpp)
- [src/render/LayoutMetrics.h](src/render/LayoutMetrics.h)

### cpp-7 · 2451 行 · 已完成

- [src/render/OpticalParameters.h](src/render/OpticalParameters.h)
- [src/render/OverviewController.cpp](src/render/OverviewController.cpp)
- [src/render/OverviewController.h](src/render/OverviewController.h)
- [src/render/OverviewLabelsModel.cpp](src/render/OverviewLabelsModel.cpp)
- [src/render/OverviewLabelsModel.h](src/render/OverviewLabelsModel.h)
- [src/render/PointerDragController.cpp](src/render/PointerDragController.cpp)
- [src/render/PointerDragController.h](src/render/PointerDragController.h)
- [src/render/RealColumns.cpp](src/render/RealColumns.cpp)
- [src/render/RealColumns.h](src/render/RealColumns.h)
- [src/render/RenderBackend.h](src/render/RenderBackend.h)
- [src/render/RenderQuality.cpp](src/render/RenderQuality.cpp)
- [src/render/RenderQuality.h](src/render/RenderQuality.h)
- [src/render/ReturningCasePool.cpp](src/render/ReturningCasePool.cpp)
- [src/render/ReturningCasePool.h](src/render/ReturningCasePool.h)
- [src/render/RowLightController.cpp](src/render/RowLightController.cpp)
- [src/render/RowLightController.h](src/render/RowLightController.h)
- [src/render/SceneAdapter.cpp](src/render/SceneAdapter.cpp)
- [src/render/SceneAdapter.h](src/render/SceneAdapter.h)
- [src/render/SceneFrame.h](src/render/SceneFrame.h)

### cpp-8 · 2455 行 · 已完成

- [src/render/SceneGraphLifecycle.cpp](src/render/SceneGraphLifecycle.cpp)
- [src/render/SceneGraphLifecycle.h](src/render/SceneGraphLifecycle.h)
- [src/render/SceneMath.h](src/render/SceneMath.h)
- [src/render/SelectionController.cpp](src/render/SelectionController.cpp)
- [src/render/SelectionController.h](src/render/SelectionController.h)
- [src/render/SelectionLighting.cpp](src/render/SelectionLighting.cpp)
- [src/render/SelectionLighting.h](src/render/SelectionLighting.h)
- [src/render/SettingsController.cpp](src/render/SettingsController.cpp)
- [src/render/SettingsController.h](src/render/SettingsController.h)
- [src/render/SourceOptics.h](src/render/SourceOptics.h)
- [src/render/StartupCoordinator.cpp](src/render/StartupCoordinator.cpp)
- [src/render/StartupCoordinator.h](src/render/StartupCoordinator.h)

### cpp-9 · 2400 行 · 已完成

- [src/render/navigation/InputRouter.cpp](src/render/navigation/InputRouter.cpp)
- [src/render/navigation/InputRouter.h](src/render/navigation/InputRouter.h)
- [src/render/navigation/OrdinalMotion.cpp](src/render/navigation/OrdinalMotion.cpp)
- [src/render/navigation/OrdinalMotion.h](src/render/navigation/OrdinalMotion.h)
- [src/render/navigation/RulerController.cpp](src/render/navigation/RulerController.cpp)
- [src/render/navigation/RulerController.h](src/render/navigation/RulerController.h)
- [src/render/navigation/RulerInk.cpp](src/render/navigation/RulerInk.cpp)
- [src/render/navigation/RulerInk.h](src/render/navigation/RulerInk.h)
- [src/render/opening/BrandPaths.h](src/render/opening/BrandPaths.h)
- [src/render/opening/OpeningController.cpp](src/render/opening/OpeningController.cpp)
- [src/render/opening/OpeningController.h](src/render/opening/OpeningController.h)
- [src/render/opening/OpeningFrame.cpp](src/render/opening/OpeningFrame.cpp)
- [src/render/opening/OpeningFrame.h](src/render/opening/OpeningFrame.h)
- [src/render/opening/OpeningInk.cpp](src/render/opening/OpeningInk.cpp)
- [src/render/opening/OpeningInk.h](src/render/opening/OpeningInk.h)

### cpp-10 · 2466 行 · 已完成

- [src/render/rhi/AtlasAllocator.cpp](src/render/rhi/AtlasAllocator.cpp)
- [src/render/rhi/AtlasAllocator.h](src/render/rhi/AtlasAllocator.h)
- [src/render/rhi/AtlasUpload.cpp](src/render/rhi/AtlasUpload.cpp)
- [src/render/rhi/AtlasUpload.h](src/render/rhi/AtlasUpload.h)
- [src/render/rhi/BackendCapabilities.cpp](src/render/rhi/BackendCapabilities.cpp)
- [src/render/rhi/BackendCapabilities.h](src/render/rhi/BackendCapabilities.h)
- [src/render/rhi/DiagnosticSamples.h](src/render/rhi/DiagnosticSamples.h)
- [src/render/rhi/DisplayOutput.cpp](src/render/rhi/DisplayOutput.cpp)
- [src/render/rhi/DisplayOutput.h](src/render/rhi/DisplayOutput.h)
- [src/render/rhi/DisplayState.h](src/render/rhi/DisplayState.h)
- [src/render/rhi/DrawUniform.h](src/render/rhi/DrawUniform.h)
- [src/render/rhi/FrameUniforms.cpp](src/render/rhi/FrameUniforms.cpp)
- [src/render/rhi/FrameUniforms.h](src/render/rhi/FrameUniforms.h)
- [src/render/rhi/PixelStorage.h](src/render/rhi/PixelStorage.h)
- [src/render/rhi/Postprocess.cpp](src/render/rhi/Postprocess.cpp)
- [src/render/rhi/Postprocess.h](src/render/rhi/Postprocess.h)
- [src/render/rhi/PostprocessDefaults.h](src/render/rhi/PostprocessDefaults.h)
- [src/render/rhi/PreparedAssets.cpp](src/render/rhi/PreparedAssets.cpp)
- [src/render/rhi/PreparedAssets.h](src/render/rhi/PreparedAssets.h)
- [src/render/rhi/ReadbackImage.cpp](src/render/rhi/ReadbackImage.cpp)
- [src/render/rhi/ReadbackImage.h](src/render/rhi/ReadbackImage.h)
- [src/render/rhi/ReadbackQueue.cpp](src/render/rhi/ReadbackQueue.cpp)

### cpp-11 · 2946 行 · 已完成

- [src/render/rhi/ReadbackQueue.h](src/render/rhi/ReadbackQueue.h)
- [src/render/rhi/ReelTextNode.cpp](src/render/rhi/ReelTextNode.cpp)
- [src/render/rhi/ReelTextNode.h](src/render/rhi/ReelTextNode.h)
- [src/render/rhi/RenderData.h](src/render/rhi/RenderData.h)
- [src/render/rhi/RenderTarget.cpp](src/render/rhi/RenderTarget.cpp)
- [src/render/rhi/RenderTarget.h](src/render/rhi/RenderTarget.h)
- [src/render/rhi/ResourceInventory.h](src/render/rhi/ResourceInventory.h)
- [src/render/rhi/ResourcePlan.h](src/render/rhi/ResourcePlan.h)
- [src/render/rhi/ReviewGlCapabilities.h](src/render/rhi/ReviewGlCapabilities.h)
- [src/render/rhi/RhiApi.h](src/render/rhi/RhiApi.h)
- [src/render/rhi/RhiItem.cpp](src/render/rhi/RhiItem.cpp)
- [src/render/rhi/RhiItem.h](src/render/rhi/RhiItem.h)
- [src/render/rhi/RhiRenderer.cpp](src/render/rhi/RhiRenderer.cpp)

### cpp-12 · 2478 行 · 已完成

- [src/render/rhi/RhiRenderer.h](src/render/rhi/RhiRenderer.h)
- [src/render/rhi/SceneCovers.cpp](src/render/rhi/SceneCovers.cpp)
- [src/render/rhi/SceneCovers.h](src/render/rhi/SceneCovers.h)
- [src/render/rhi/SceneGeometryData.cpp](src/render/rhi/SceneGeometryData.cpp)
- [src/render/rhi/SceneGeometryData.h](src/render/rhi/SceneGeometryData.h)
- [src/render/rhi/SceneItem.cpp](src/render/rhi/SceneItem.cpp)
- [src/render/rhi/SceneItem.h](src/render/rhi/SceneItem.h)
- [src/render/rhi/ScenePipeline.cpp](src/render/rhi/ScenePipeline.cpp)
- [src/render/rhi/ScenePipeline.h](src/render/rhi/ScenePipeline.h)
- [src/render/rhi/ShellMaterial.cpp](src/render/rhi/ShellMaterial.cpp)
- [src/render/rhi/ShellMaterial.h](src/render/rhi/ShellMaterial.h)
- [src/render/rhi/TransferAudit.h](src/render/rhi/TransferAudit.h)
- [src/render/text/ReelInk.cpp](src/render/text/ReelInk.cpp)
- [src/render/text/ReelInk.h](src/render/text/ReelInk.h)
- [src/render/text/TextAreaFormat.cpp](src/render/text/TextAreaFormat.cpp)
- [src/render/text/TextAreaFormat.h](src/render/text/TextAreaFormat.h)

### cpp-13 · 2458 行 · 已完成

- [src/render/text/TextLayoutAdapter.cpp](src/render/text/TextLayoutAdapter.cpp)
- [src/render/text/TextLayoutAdapter.h](src/render/text/TextLayoutAdapter.h)
- [src/render/text/TitleReel.cpp](src/render/text/TitleReel.cpp)
- [src/render/text/TitleReel.h](src/render/text/TitleReel.h)
- [src/render/text/TransportMotion.cpp](src/render/text/TransportMotion.cpp)
- [src/render/text/TransportMotion.h](src/render/text/TransportMotion.h)
- [src/services/CoverCache.cpp](src/services/CoverCache.cpp)
- [src/services/CoverCache.h](src/services/CoverCache.h)
- [src/services/DemoLibrary.cpp](src/services/DemoLibrary.cpp)
- [src/services/DemoLibrary.h](src/services/DemoLibrary.h)

### cpp-14 · 2517 行 · 已完成

- [src/services/IMetadataProvider.cpp](src/services/IMetadataProvider.cpp)
- [src/services/IMetadataProvider.h](src/services/IMetadataProvider.h)
- [src/services/LibrarySession.cpp](src/services/LibrarySession.cpp)
- [src/services/LibrarySession.h](src/services/LibrarySession.h)
- [src/services/LibrarySession_p.h](src/services/LibrarySession_p.h)
- [src/services/MetadataPresentation.cpp](src/services/MetadataPresentation.cpp)
- [src/services/MetadataPresentation.h](src/services/MetadataPresentation.h)
- [src/services/OfflineMetadataProvider.cpp](src/services/OfflineMetadataProvider.cpp)
- [src/services/OfflineMetadataProvider.h](src/services/OfflineMetadataProvider.h)
- [src/services/OnlineMetadataProvider.cpp](src/services/OnlineMetadataProvider.cpp)
- [src/services/OnlineMetadataProvider.h](src/services/OnlineMetadataProvider.h)
- [src/services/PreferencesStore.cpp](src/services/PreferencesStore.cpp)
- [src/services/PreferencesStore.h](src/services/PreferencesStore.h)
- [src/services/SingleInstance.cpp](src/services/SingleInstance.cpp)
- [src/services/SingleInstance.h](src/services/SingleInstance.h)
- [src/services/metadata/MetadataCachePolicy.cpp](src/services/metadata/MetadataCachePolicy.cpp)
- [src/services/metadata/MetadataCachePolicy.h](src/services/metadata/MetadataCachePolicy.h)
- [src/services/metadata/MetadataCoordinator.cpp](src/services/metadata/MetadataCoordinator.cpp)
- [src/services/metadata/MetadataCoordinator.h](src/services/metadata/MetadataCoordinator.h)
- [src/services/metadata/MetadataHttp.cpp](src/services/metadata/MetadataHttp.cpp)

### cpp-15 · 2565 行 · 已完成

- [src/services/metadata/MetadataHttp.h](src/services/metadata/MetadataHttp.h)
- [src/services/metadata/MetadataMerge.cpp](src/services/metadata/MetadataMerge.cpp)
- [src/services/metadata/MetadataMerge.h](src/services/metadata/MetadataMerge.h)
- [src/services/metadata/MetadataWorker.cpp](src/services/metadata/MetadataWorker.cpp)
- [src/services/metadata/MetadataWorker.h](src/services/metadata/MetadataWorker.h)
- [src/services/metadata/MusicBrainzSource.cpp](src/services/metadata/MusicBrainzSource.cpp)
- [src/services/metadata/MusicBrainzSource.h](src/services/metadata/MusicBrainzSource.h)
- [src/services/metadata/WikipediaSource.cpp](src/services/metadata/WikipediaSource.cpp)
- [src/services/metadata/WikipediaSource.h](src/services/metadata/WikipediaSource.h)
- [tests/integration/AssetTest.cpp](tests/integration/AssetTest.cpp)
- [tests/integration/LibraryQmlTest.cpp](tests/integration/LibraryQmlTest.cpp)
- [tests/integration/MacApplicationMenuTest.mm](tests/integration/MacApplicationMenuTest.mm)
- [tests/render/ArchivePoolChecks.cpp](tests/render/ArchivePoolChecks.cpp)

### cpp-16 · 2568 行 · 已完成

- [tests/render/CaptureMailbox.h](tests/render/CaptureMailbox.h)
- [tests/render/ControlsChecks.cpp](tests/render/ControlsChecks.cpp)
- [tests/render/ControlsChecks.h](tests/render/ControlsChecks.h)
- [tests/render/CoverChecks.cpp](tests/render/CoverChecks.cpp)
- [tests/render/CoverSamplingChecks.h](tests/render/CoverSamplingChecks.h)
- [tests/render/ExperimentChecks.cpp](tests/render/ExperimentChecks.cpp)
- [tests/render/ExperimentChecks.h](tests/render/ExperimentChecks.h)
- [tests/render/ImageSampling.h](tests/render/ImageSampling.h)
- [tests/render/LibraryChecks.cpp](tests/render/LibraryChecks.cpp)
- [tests/render/LibraryChecks.h](tests/render/LibraryChecks.h)
- [tests/render/OffscreenRhi.h](tests/render/OffscreenRhi.h)
- [tests/render/OpeningChecks.cpp](tests/render/OpeningChecks.cpp)

### cpp-17 · 2509 行 · 已完成

- [tests/render/OpeningChecks.h](tests/render/OpeningChecks.h)
- [tests/render/OutputReference.h](tests/render/OutputReference.h)
- [tests/render/OverviewChecks.cpp](tests/render/OverviewChecks.cpp)
- [tests/render/OverviewChecks.h](tests/render/OverviewChecks.h)
- [tests/render/PostprocessChecks.h](tests/render/PostprocessChecks.h)
- [tests/render/PresentationChecks.cpp](tests/render/PresentationChecks.cpp)
- [tests/render/PresentationChecks.h](tests/render/PresentationChecks.h)
- [tests/render/RhiAoChecks.cpp](tests/render/RhiAoChecks.cpp)
- [tests/render/RhiBokehChecks.cpp](tests/render/RhiBokehChecks.cpp)
- [tests/render/RhiCaptureHarness.h](tests/render/RhiCaptureHarness.h)
- [tests/render/RhiFlickerChecks.cpp](tests/render/RhiFlickerChecks.cpp)
- [tests/render/RhiFloorChecks.h](tests/render/RhiFloorChecks.h)
- [tests/render/RhiFogChecks.cpp](tests/render/RhiFogChecks.cpp)

### cpp-18 · 2421 行 · 已完成

- [tests/render/RhiFogChecks.h](tests/render/RhiFogChecks.h)
- [tests/render/RhiOpaqueChecks.cpp](tests/render/RhiOpaqueChecks.cpp)
- [tests/render/RhiPostprocessChecks.cpp](tests/render/RhiPostprocessChecks.cpp)
- [tests/render/RhiProductionFixture.h](tests/render/RhiProductionFixture.h)
- [tests/render/RhiReviewInput.h](tests/render/RhiReviewInput.h)
- [tests/render/RhiScaleChecks.cpp](tests/render/RhiScaleChecks.cpp)
- [tests/render/RhiShadowChecks.cpp](tests/render/RhiShadowChecks.cpp)
- [tests/render/RhiShadowChecks.h](tests/render/RhiShadowChecks.h)
- [tests/render/RhiShellChecks.cpp](tests/render/RhiShellChecks.cpp)

### cpp-19 · 2481 行 · 已完成

- [tests/render/RhiSmaaChecks.cpp](tests/render/RhiSmaaChecks.cpp)
- [tests/render/SmaaReference.h](tests/render/SmaaReference.h)
- [tests/render/StartupChecks.cpp](tests/render/StartupChecks.cpp)
- [tests/render/StartupChecks.h](tests/render/StartupChecks.h)
- [tests/render/UiTests.cpp](tests/render/UiTests.cpp)
- [tests/support/AudioFixture.h](tests/support/AudioFixture.h)
- [tests/support/FileFixture.h](tests/support/FileFixture.h)
- [tests/support/LibraryFixture.h](tests/support/LibraryFixture.h)
- [tests/support/LibrarySearchOracle.h](tests/support/LibrarySearchOracle.h)

### cpp-20 · 2417 行 · 已完成

- [tests/support/NumericChecks.h](tests/support/NumericChecks.h)
- [tests/support/OffscreenSurface.h](tests/support/OffscreenSurface.h)
- [tests/support/SettingsFixture.h](tests/support/SettingsFixture.h)
- [tests/unit/ApplicationOptionsTest.cpp](tests/unit/ApplicationOptionsTest.cpp)
- [tests/unit/ArchivePoolTest.cpp](tests/unit/ArchivePoolTest.cpp)
- [tests/unit/AtlasUploadTest.cpp](tests/unit/AtlasUploadTest.cpp)
- [tests/unit/AudioAmbienceTest.cpp](tests/unit/AudioAmbienceTest.cpp)
- [tests/unit/CoverCacheTest.cpp](tests/unit/CoverCacheTest.cpp)

### cpp-21 · 3133 行 · 已完成

- [tests/unit/DetailMotionTest.cpp](tests/unit/DetailMotionTest.cpp)
- [tests/unit/DirectoryScannerTest.cpp](tests/unit/DirectoryScannerTest.cpp)
- [tests/unit/DisplayOutputTest.cpp](tests/unit/DisplayOutputTest.cpp)
- [tests/unit/DisplayOutputTypes.h](tests/unit/DisplayOutputTypes.h)
- [tests/unit/ExperimentTest.cpp](tests/unit/ExperimentTest.cpp)
- [tests/unit/GraphicsBackendTest.cpp](tests/unit/GraphicsBackendTest.cpp)
- [tests/unit/IdentityTest.cpp](tests/unit/IdentityTest.cpp)
- [tests/unit/JsonFileTest.cpp](tests/unit/JsonFileTest.cpp)
- [tests/unit/LibraryDataTest.cpp](tests/unit/LibraryDataTest.cpp)

### cpp-22 · 2007 行 · 已完成

- [tests/unit/LightingTest.cpp](tests/unit/LightingTest.cpp)
- [tests/unit/LocalizationTest.cpp](tests/unit/LocalizationTest.cpp)
- [tests/unit/LoggingTest.cpp](tests/unit/LoggingTest.cpp)
- [tests/unit/MediaMetadataTest.cpp](tests/unit/MediaMetadataTest.cpp)
- [tests/unit/ModuleProbe.cpp](tests/unit/ModuleProbe.cpp)
- [tests/unit/MotionTest.cpp](tests/unit/MotionTest.cpp)
- [tests/unit/NativeToolTest.cpp](tests/unit/NativeToolTest.cpp)
- [tests/unit/NavigationTest.cpp](tests/unit/NavigationTest.cpp)

### cpp-23 · 2510 行 · 已完成

- [tests/unit/OnlineMetadataTest.cpp](tests/unit/OnlineMetadataTest.cpp)
- [tests/unit/OpeningTest.cpp](tests/unit/OpeningTest.cpp)
- [tests/unit/OverviewTest.cpp](tests/unit/OverviewTest.cpp)
- [tests/unit/PlaybackTest.cpp](tests/unit/PlaybackTest.cpp)
- [tests/unit/PresentationTest.cpp](tests/unit/PresentationTest.cpp)

### cpp-24 · 2587 行 · 已完成

- [tests/unit/QmlBehaviorTest.cpp](tests/unit/QmlBehaviorTest.cpp)
- [tests/unit/RealColumnsTest.cpp](tests/unit/RealColumnsTest.cpp)
- [tests/unit/RenderQualityTest.cpp](tests/unit/RenderQualityTest.cpp)
- [tests/unit/RenderSupportTest.cpp](tests/unit/RenderSupportTest.cpp)
- [tests/unit/RhiMaterialTest.cpp](tests/unit/RhiMaterialTest.cpp)
- [tests/unit/SceneTest.cpp](tests/unit/SceneTest.cpp)
- [tests/unit/SettingsTest.cpp](tests/unit/SettingsTest.cpp)
- [tests/unit/SingleInstanceTest.cpp](tests/unit/SingleInstanceTest.cpp)
- [tests/unit/StartupTest.cpp](tests/unit/StartupTest.cpp)
- [tests/unit/TitleTest.cpp](tests/unit/TitleTest.cpp)

### cpp-25 · 2236 行 · 已完成

- [tests/unit/TrackFocusTest.cpp](tests/unit/TrackFocusTest.cpp)
- [tests/unit/VerificationWindowTest.cpp](tests/unit/VerificationWindowTest.cpp)
- [tools/assets/ExportBranding.cpp](tools/assets/ExportBranding.cpp)
- [tools/assets/GeometryAudit.cpp](tools/assets/GeometryAudit.cpp)
- [tools/assets/GeometryAudit.h](tools/assets/GeometryAudit.h)
- [tools/assets/PrepareAssets.cpp](tools/assets/PrepareAssets.cpp)
- [tools/migration/MetadataNetworkProbe.cpp](tools/migration/MetadataNetworkProbe.cpp)

### cpp-26 · 1596 行 · 已完成

- [tools/migration/RulerSourceProbe.cpp](tools/migration/RulerSourceProbe.cpp)
- [tools/migration/RulerSourceProbe.h](tools/migration/RulerSourceProbe.h)
- [tools/migration/SourceUiProbe.cpp](tools/migration/SourceUiProbe.cpp)
- [tools/migration/TitleSourceProbe.cpp](tools/migration/TitleSourceProbe.cpp)
- [tools/migration/TitleSourceProbe.h](tools/migration/TitleSourceProbe.h)

### qml-1 · 1371 行 · 已完成

- [qml/Yalahoo/Music/App.qml](qml/Yalahoo/Music/App.qml)
- [qml/Yalahoo/Music/components/ArchiveButton.qml](qml/Yalahoo/Music/components/ArchiveButton.qml)
- [qml/Yalahoo/Music/components/ArchiveCheckBox.qml](qml/Yalahoo/Music/components/ArchiveCheckBox.qml)
- [qml/Yalahoo/Music/components/ArchiveComboBox.qml](qml/Yalahoo/Music/components/ArchiveComboBox.qml)
- [qml/Yalahoo/Music/components/ArchiveEditMenu.qml](qml/Yalahoo/Music/components/ArchiveEditMenu.qml)
- [qml/Yalahoo/Music/components/ArchiveEditorSurface.qml](qml/Yalahoo/Music/components/ArchiveEditorSurface.qml)
- [qml/Yalahoo/Music/components/ArchiveProgressBar.qml](qml/Yalahoo/Music/components/ArchiveProgressBar.qml)
- [qml/Yalahoo/Music/components/ArchiveScrollBar.qml](qml/Yalahoo/Music/components/ArchiveScrollBar.qml)
- [qml/Yalahoo/Music/components/ArchiveScrollView.qml](qml/Yalahoo/Music/components/ArchiveScrollView.qml)
- [qml/Yalahoo/Music/components/ArchiveSlider.qml](qml/Yalahoo/Music/components/ArchiveSlider.qml)
- [qml/Yalahoo/Music/components/ArchiveText.qml](qml/Yalahoo/Music/components/ArchiveText.qml)
- [qml/Yalahoo/Music/components/ArchiveTextArea.qml](qml/Yalahoo/Music/components/ArchiveTextArea.qml)
- [qml/Yalahoo/Music/components/ArchiveTextField.qml](qml/Yalahoo/Music/components/ArchiveTextField.qml)
- [qml/Yalahoo/Music/components/ArchiveToolTip.qml](qml/Yalahoo/Music/components/ArchiveToolTip.qml)
- [qml/Yalahoo/Music/components/ArchiveVectorImage.qml](qml/Yalahoo/Music/components/ArchiveVectorImage.qml)
- [qml/Yalahoo/Music/components/BackdropBlur.qml](qml/Yalahoo/Music/components/BackdropBlur.qml)
- [qml/Yalahoo/Music/components/BackdropBlurPass.qml](qml/Yalahoo/Music/components/BackdropBlurPass.qml)
- [qml/Yalahoo/Music/components/BackdropTexture.qml](qml/Yalahoo/Music/components/BackdropTexture.qml)
- [qml/Yalahoo/Music/components/BrowseInfo.qml](qml/Yalahoo/Music/components/BrowseInfo.qml)
- [qml/Yalahoo/Music/components/ColumnNavigation.qml](qml/Yalahoo/Music/components/ColumnNavigation.qml)

### qml-2 · 1372 行 · 已完成

- [qml/Yalahoo/Music/components/CreditCard.qml](qml/Yalahoo/Music/components/CreditCard.qml)
- [qml/Yalahoo/Music/components/DesignTokens.qml](qml/Yalahoo/Music/components/DesignTokens.qml)
- [qml/Yalahoo/Music/components/DetailAlbumInfo.qml](qml/Yalahoo/Music/components/DetailAlbumInfo.qml)
- [qml/Yalahoo/Music/components/DetailHeader.qml](qml/Yalahoo/Music/components/DetailHeader.qml)
- [qml/Yalahoo/Music/components/DetailIntroduction.qml](qml/Yalahoo/Music/components/DetailIntroduction.qml)
- [qml/Yalahoo/Music/components/DetailTabs.qml](qml/Yalahoo/Music/components/DetailTabs.qml)
- [qml/Yalahoo/Music/components/DetailTabsBackdrop.qml](qml/Yalahoo/Music/components/DetailTabsBackdrop.qml)
- [qml/Yalahoo/Music/components/DisplaySurface.qml](qml/Yalahoo/Music/components/DisplaySurface.qml)
- [qml/Yalahoo/Music/components/Footer.qml](qml/Yalahoo/Music/components/Footer.qml)
- [qml/Yalahoo/Music/components/Header.qml](qml/Yalahoo/Music/components/Header.qml)
- [qml/Yalahoo/Music/components/HeaderTransport.qml](qml/Yalahoo/Music/components/HeaderTransport.qml)
- [qml/Yalahoo/Music/components/NavigationFrame.qml](qml/Yalahoo/Music/components/NavigationFrame.qml)
- [qml/Yalahoo/Music/components/OpeningBackdrop.qml](qml/Yalahoo/Music/components/OpeningBackdrop.qml)
- [qml/Yalahoo/Music/components/OpeningFilm.qml](qml/Yalahoo/Music/components/OpeningFilm.qml)

### qml-3 · 1445 行 · 已完成

- [qml/Yalahoo/Music/components/OrdinalReel.qml](qml/Yalahoo/Music/components/OrdinalReel.qml)
- [qml/Yalahoo/Music/components/OverviewBackdrop.qml](qml/Yalahoo/Music/components/OverviewBackdrop.qml)
- [qml/Yalahoo/Music/components/OverviewLabel.qml](qml/Yalahoo/Music/components/OverviewLabel.qml)
- [qml/Yalahoo/Music/components/PlaybackControls.qml](qml/Yalahoo/Music/components/PlaybackControls.qml)
- [qml/Yalahoo/Music/components/ReadingVeil.qml](qml/Yalahoo/Music/components/ReadingVeil.qml)
- [qml/Yalahoo/Music/components/Ruler.qml](qml/Yalahoo/Music/components/Ruler.qml)
- [qml/Yalahoo/Music/components/SceneGraphVisual.qml](qml/Yalahoo/Music/components/SceneGraphVisual.qml)
- [qml/Yalahoo/Music/components/StageFocus.qml](qml/Yalahoo/Music/components/StageFocus.qml)
- [qml/Yalahoo/Music/components/StageFrameDriver.qml](qml/Yalahoo/Music/components/StageFrameDriver.qml)
- [qml/Yalahoo/Music/components/ThemeSwitch.qml](qml/Yalahoo/Music/components/ThemeSwitch.qml)
- [qml/Yalahoo/Music/components/TitleReel.qml](qml/Yalahoo/Music/components/TitleReel.qml)
- [qml/Yalahoo/Music/components/TrackList.qml](qml/Yalahoo/Music/components/TrackList.qml)
- [qml/Yalahoo/Music/components/TrackListReveal.qml](qml/Yalahoo/Music/components/TrackListReveal.qml)
- [qml/Yalahoo/Music/components/TrackRow.qml](qml/Yalahoo/Music/components/TrackRow.qml)

### qml-4 · 1434 行 · 已完成

- [qml/Yalahoo/Music/components/TransportTitle.qml](qml/Yalahoo/Music/components/TransportTitle.qml)
- [qml/Yalahoo/Music/pages/Browse.qml](qml/Yalahoo/Music/pages/Browse.qml)
- [qml/Yalahoo/Music/pages/Detail.qml](qml/Yalahoo/Music/pages/Detail.qml)
- [qml/Yalahoo/Music/pages/MusicStage.qml](qml/Yalahoo/Music/pages/MusicStage.qml)
- [qml/Yalahoo/Music/pages/Opening.qml](qml/Yalahoo/Music/pages/Opening.qml)
- [qml/Yalahoo/Music/pages/Overview.qml](qml/Yalahoo/Music/pages/Overview.qml)
- [qml/Yalahoo/Music/panels/AppearanceSettings.qml](qml/Yalahoo/Music/panels/AppearanceSettings.qml)
- [qml/Yalahoo/Music/panels/ArrayQualitySettings.qml](qml/Yalahoo/Music/panels/ArrayQualitySettings.qml)
- [qml/Yalahoo/Music/panels/ExperimentOverlay.qml](qml/Yalahoo/Music/panels/ExperimentOverlay.qml)
- [qml/Yalahoo/Music/panels/ExperimentPanels.qml](qml/Yalahoo/Music/panels/ExperimentPanels.qml)
- [qml/Yalahoo/Music/panels/InterfaceSoundPreview.qml](qml/Yalahoo/Music/panels/InterfaceSoundPreview.qml)
- [qml/Yalahoo/Music/panels/LibraryPanel.qml](qml/Yalahoo/Music/panels/LibraryPanel.qml)

### qml-5 · 1397 行 · 已完成

- [qml/Yalahoo/Music/panels/LibraryPanels.qml](qml/Yalahoo/Music/panels/LibraryPanels.qml)
- [qml/Yalahoo/Music/panels/LightingModes.qml](qml/Yalahoo/Music/panels/LightingModes.qml)
- [qml/Yalahoo/Music/panels/LightingPanel.qml](qml/Yalahoo/Music/panels/LightingPanel.qml)
- [qml/Yalahoo/Music/panels/MetadataSection.qml](qml/Yalahoo/Music/panels/MetadataSection.qml)
- [qml/Yalahoo/Music/panels/MetadataSettings.qml](qml/Yalahoo/Music/panels/MetadataSettings.qml)
- [qml/Yalahoo/Music/panels/MotionPanel.qml](qml/Yalahoo/Music/panels/MotionPanel.qml)
- [qml/Yalahoo/Music/panels/MotionSpeedRange.qml](qml/Yalahoo/Music/panels/MotionSpeedRange.qml)
- [qml/Yalahoo/Music/panels/PanelContents.qml](qml/Yalahoo/Music/panels/PanelContents.qml)
- [qml/Yalahoo/Music/panels/PanelDescription.qml](qml/Yalahoo/Music/panels/PanelDescription.qml)
- [qml/Yalahoo/Music/panels/PanelHeading.qml](qml/Yalahoo/Music/panels/PanelHeading.qml)
- [qml/Yalahoo/Music/panels/PlaybackPanel.qml](qml/Yalahoo/Music/panels/PlaybackPanel.qml)
- [qml/Yalahoo/Music/panels/SearchFilters.qml](qml/Yalahoo/Music/panels/SearchFilters.qml)
- [qml/Yalahoo/Music/panels/SearchPanel.qml](qml/Yalahoo/Music/panels/SearchPanel.qml)
- [qml/Yalahoo/Music/panels/SearchResult.qml](qml/Yalahoo/Music/panels/SearchResult.qml)

### qml-6 · 1281 行 · 已完成

- [qml/Yalahoo/Music/panels/SettingChoice.qml](qml/Yalahoo/Music/panels/SettingChoice.qml)
- [qml/Yalahoo/Music/panels/SettingRange.qml](qml/Yalahoo/Music/panels/SettingRange.qml)
- [qml/Yalahoo/Music/panels/SettingsPanel.qml](qml/Yalahoo/Music/panels/SettingsPanel.qml)
- [qml/Yalahoo/Music/panels/SettingsSection.qml](qml/Yalahoo/Music/panels/SettingsSection.qml)
- [qml/Yalahoo/Music/panels/SoundSettings.qml](qml/Yalahoo/Music/panels/SoundSettings.qml)
- [qml/Yalahoo/Music/panels/StagePanels.qml](qml/Yalahoo/Music/panels/StagePanels.qml)
- [qml/Yalahoo/Music/panels/ThemeCard.qml](qml/Yalahoo/Music/panels/ThemeCard.qml)
- [qml/Yalahoo/Music/scene/CaseMaterials.qml](qml/Yalahoo/Music/scene/CaseMaterials.qml)
- [qml/Yalahoo/Music/scene/GeometryScene.qml](qml/Yalahoo/Music/scene/GeometryScene.qml)
- [qml/Yalahoo/Music/scene/GlassMaterial.qml](qml/Yalahoo/Music/scene/GlassMaterial.qml)
- [qml/Yalahoo/Music/scene/LightingMaterial.qml](qml/Yalahoo/Music/scene/LightingMaterial.qml)
- [resources/models/Music_case_v040.qml](resources/models/Music_case_v040.qml)
- [tests/render/ControlsReview.qml](tests/render/ControlsReview.qml)
- [tests/render/DetailReview.qml](tests/render/DetailReview.qml)
- [tests/render/LibraryReview.qml](tests/render/LibraryReview.qml)
- [tests/render/OverviewVisual.qml](tests/render/OverviewVisual.qml)
- [tests/unit/DisplayOutputProbe.qml](tests/unit/DisplayOutputProbe.qml)
- [tools/migration/RulerReview.qml](tools/migration/RulerReview.qml)
- [tools/migration/TitleReview.qml](tools/migration/TitleReview.qml)
