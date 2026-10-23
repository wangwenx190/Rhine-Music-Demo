# 全项目 Qt 严格审查报告

审查日期：2026-10-23（Asia/Shanghai）。源码基准：`15908cfd709e7f91345ce53d37b325d387c98791`。本报告是该版本的只读审查快照；按本次用户明确要求保留于根目录并提交，实现未修改。本报告不替代专项契约，也不将发现的问题标为已修复或已转交 R36。

使用用户指定 `agent-skills` 仓库提交 `6845f02d41171a6df8b6e6fe225b7f5d1a6f4adf` 的 `skills/qt-cpp-review/SKILL.md`（v3.0）与 `skills/qt-qml-review/SKILL.md`（v2.0），以及该仓库的专用 review-agent 定义和 A1–A6/A1–A7 规则。技能标注 Qt Company 版权与 LicenseRef-Qt-Commercial OR BSD-3-Clause。没有把技能的静态启发式输出宣称为 Qt 官方对本项目的认证。

范围为 Git 跟踪的全部 369 个 C++/Objective-C++ 头与实现（66,262 行）和 93 个 QML 文件（8,329 行），包括生产、平台分支、研究入口、测试和原生工具。按技能行数阈值拆为 27 个 C++、6 个 QML 分片，由 33 次独立分片审查执行，最多三个审查 agent 并行；主审补查构建/部署/生成器、共享着色器、验证脚本、来源及有关文档。分片精确文件清单见附录。

`legacy-web/`是冻结来源：本轮检查来源映射、88 个源模块、47 项资源、24 项冻结资产输入和 36 项品牌输入的一致性，不把冻结代码当作现行 Qt 产品实现，也不修改第三方/导入资源。二进制资产按来源和完整性检查，未宣称对字体、GLB、音频等执行新的设备/画质验收。构建、部署输出、`.git` 和未跟踪机器产物不纳入源码覆盖统计。

判定口径：自动 lint 命中完整保留为规则诊断，不能等同于已确认功能缺陷；深度 findings 要求置信度至少 80/100 和符号链证据，60–79 的事项单列调查，低于 60 不列。跨分片引用进行了定向追踪，跨分片全局所有权/模型关系仍为 best effort。结论限于静态审查与下列实际执行，未作全程序正确性证明。

## 执行与证据边界

| 检查 | 实际结果 | 能证明的范围 |
| --- | --- | --- |
| Qt C++ skill Python linter，全 369 文件，`--summary` | 878 条诊断，退出 1 | 规则命中；退出 1 是发现诊断，不是工具使用错误 |
| QML skill Python linter，全 93 文件，`--summary --qmllint` | Python 规则 222 条；独立 qmllint 1,303 条；合计 1,525 条 | 全量启发式检查；独立调用缺少 CMake 生成的类型、导入和资源上下文，类型诊断需结合项目 target 解释 |
| `cmake --build --preset qt-release --target Yalahoo_qmllint ui-tests_qmllint` | 退出 0，无诊断 | 当前配置中这两个目标的官方 Qt 6.12.0 类型/导入/资源检查；不外推未编入该配置的研究 QML |
| `cmake -DSOURCE_ROOT=… -P tools/audit/CheckSourcePolicy.cmake` | 退出 0 | 当前源码的 ASCII/Qt 显式字符串策略 |
| `node tools/migration/check-web-baseline.mjs` | 退出 0，来源及资产一致性通过 | 冻结来源/数值 fixture/资源身份；不是运行或画质通过 |
| 空 Qt 许可目录的匿名临时目录复现 | 实际接受空目录，见 S-001 | Unix 许可复制函数的边界行为 |
| 后处理生成器的匿名隔离副本复现 | 退出 0，输出 SMAA 0.1/8，见 S-003 | 生成脚本和已提交生产 0.05/32 参数不一致 |

本轮没有启动可见窗口、播放声音、读取用户曲库、联网查询用户资料、发布或上传；没有执行全量构建/CTest、性能采样、Windows/Linux 原生构建或跨设备/HDR/输入/听感验收。未执行的检查不记为通过；现有 R36 未完成项（包括 MAC-VISIBLE-3D 已知失败）仍以迁移计划为准。提交只涉及本报告，故不触发无关功能回归或源码格式重写。原始日志与机器路径保留在本地临时目录，不提交。

## 确认事项索引

独立深度审查确认 35 项，构建/部署/生成链补查 3 项，另保留 3 项调查。以下分类只帮助区分影响；置信度表示证据充分程度，100/100 的代码规范项不比 95/100 的行为缺陷更严重。自动 lint 的 2,403 条原始诊断另列，不混入这组确认事项。

| ID | 类型 | 源码位置 | 事项 |
| --- | --- | --- | --- |
| [D-001] | 行为/错误路径 | `src/app/MaterialReview.cpp:140` | Material review accepts invalid render dimensions until a fatal GPU-target check |
| [D-002] | 行为/错误路径 | `src/app/main.cpp:145` | Help exits through graphics failure before printing usage |
| [D-003] | 行为/错误路径 | `src/core/MusicTypes.cpp:293` | JSON 整数验证接受负曲号和负碟号 |
| [D-004] | 行为/错误路径 | `src/core/MusicTypes.cpp:170` | 库解析错误绕过 Qt 翻译系统 |
| [D-005] | 代码规范/维护 | `src/library/LibraryStore.cpp:51` | Enum declarations omit the required trailing comma |
| [D-006] | 行为/错误路径 | `src/media/MediaMetadataProbe.cpp:215` | Artwork re-probe timeout becomes permanently cached |
| [D-007] | 行为/错误路径 | `src/presentation/PresentationController.cpp:100` | Indicator geometry updates prematurely complete the content fade |
| [D-008] | 行为/错误路径 | `src/presentation/TrackFocusController.cpp:71` | 定位请求在同步取消或替换后仍继续导航 |
| [D-009] | 行为/错误路径 | `src/presentation/TrackFocusController.cpp:175` | 开启减少动态会冻结正在显示的定位高亮 |
| [D-010] | 代码规范/维护 | `src/render/ArchiveField.cpp:98` | 扫描波分支及其逐帧状态永久不可达 |
| [D-011] | 行为/错误路径 | `src/render/SceneAdapter.cpp:348` | Opening reset changes detail without its property notification |
| [D-012] | 代码规范/维护 | `src/render/opening/OpeningController.h:93` | Phase 枚举末项缺少尾随逗号 |
| [D-013] | 代码规范/维护 | `src/render/opening/OpeningFrame.h:24` | OpeningFrame 用 QString 表达封闭阶段词汇 |
| [D-014] | 行为/错误路径 | `src/render/rhi/SceneCovers.cpp:135` | Standalone cover lease keeps the old decoder generation after selection reset |
| [D-015] | 行为/错误路径 | `src/render/text/TextLayoutAdapter.cpp:250` | Main-title ellipsis overlaps right-to-left text |
| [D-016] | 代码规范/维护 | `src/render/rhi/ScenePipeline.h:12` | Scene-pipeline enums omit required trailing commas |
| [D-017] | 行为/错误路径 | `src/services/CoverCache.cpp:535` | Output image allocation failure is cached as successful artwork |
| [D-018] | 行为/错误路径 | `src/services/metadata/MetadataCachePolicy.cpp:99` | Rejected release becomes the preferred release for future queries |
| [D-019] | 行为/错误路径 | `src/services/metadata/WikipediaSource.cpp:158` | Wikipedia fallback accepts an artist name embedded in another name |
| [D-020] | 行为/错误路径 | `tests/render/LibraryChecks.cpp:268` | Theme render failures do not fail the settings probe |
| [D-021] | 行为/错误路径 | `tests/render/RhiShellChecks.cpp:145` | Incomplete shell fixture can pass the source-state gate |
| [D-022] | 行为/错误路径 | `tests/render/StartupChecks.cpp:807` | Startup probe can overwrite an earlier run’s evidence |
| [D-023] | 行为/错误路径 | `tests/unit/DirectoryScannerTest.cpp:381` | Cancellation regression never establishes the late-completion boundary |
| [D-024] | 行为/错误路径 | `tests/unit/DisplayOutputTest.cpp:146` | Transfer test silently uses a different graphics backend from the requested one |
| [D-025] | 行为/错误路径 | `tests/unit/LibraryDataTest.cpp:1389` | Failed assertions can leave a provider callback referencing expired locals |
| [D-026] | 行为/错误路径 | `tests/unit/OnlineMetadataTest.cpp:913` | Cooldown rows skip the untrusted-host assertion |
| [D-027] | 行为/错误路径 | `tests/unit/OverviewTest.cpp:167` | Maximum-error reductions silently discard NaN results |
| [D-028] | 行为/错误路径 | `tests/unit/QmlBehaviorTest.cpp:170` | Failed temporary-directory creation redirects persistence tests into the working directory |
| [D-029] | 行为/错误路径 | `tools/assets/AssetTransaction.h:52` | Publication follows symlinked destination directories outside the repository |
| [D-030] | 行为/错误路径 | `tools/migration/TitleSourceProbe.cpp:571` | Validate title capture dimensions before integer arithmetic |
| [D-031] | 行为/错误路径 | `tools/migration/ProbeInput.h:95` | Accepted ruler frame counts can prevent the deadline from running |
| [D-032] | 行为/错误路径 | `tools/assets/ExportBranding.cpp:800` | Branding manifest inventory validation accepts duplicate entries |
| [D-033] | 代码规范/维护 | `qml/Yalahoo/Music/components/ArchiveSlider.qml:19` | Slider rail has an unused id |
| [D-034] | 行为/错误路径 | `qml/Yalahoo/Music/components/TrackRow.qml:39` | Track rows have no accessible activation action |
| [D-035] | 行为/错误路径 | `qml/Yalahoo/Music/panels/SearchFilters.qml:69` | Genre filter buttons have no visible keyboard focus |
| [S-001] | 行为/错误路径 | `tools/deploy/package_support.py:68` | Unix 打包器接受空的 Qt 许可目录 |
| [S-002] | 行为/错误路径 | `tools/deploy/verify-windows.ps1:25` | Windows 包验证器忽略清单外新增文件 |
| [S-003] | 行为/错误路径 | `tools/assets/prepare-postprocess-shaders.py:38` | 重新生成后处理 shader 会回退生产 SMAA 参数 |

## Qt Code Review Report

**Scope**: files: 全部跟踪的 C++/Objective-C++，详见分片附录。

**Files reviewed**: 369（66,262 行）

**Issues found**: 910（878 from lint，32 from deep analysis；lint 是原始诊断计数）

**Agents**: 27（独立分片，逐文件编号读取并恢复截断内容）

**Framework mode**: off；这是应用项目。QRhi/private 适配在项目契约允许的边界内，不按 Qt 模块开发要求误判。

---

### Lint findings

| Rule | Count | Locations | Mitigation |
| --- | ---: | --- | --- |
| DEP-1 QScopedPointer — use std::unique_ptr (const unique_ptr for scoped) | 1 | `tests/integration/AssetTest.cpp:80` | 按 skill 建议评估标准库/现代 Qt 替代；先核对实际对象类型和项目约束。 |
| DEP-10 .count()/.length() — prefer .size() for std consistency | 50 | `src/app/MaterialReview.cpp:160`; `src/app/OverviewReview.cpp:237,239`; `src/core/MonotonicTime.h:14`; `src/library/DirectoryScanner.cpp:356,358`; `src/render/OverviewController.cpp:318`; `src/render/SceneAdapter.cpp:394,395,397,398,404`; `src/render/opening/OpeningInk.cpp:255`; `src/render/rhi/SceneItem.cpp:852`; `tests/render/ArchivePoolChecks.cpp:484,486,521,525`; `tests/render/RhiFlickerChecks.cpp:250`; `tests/render/RhiShellChecks.cpp:216`; `tests/unit/ArchivePoolTest.cpp:340`; `tests/unit/LightingTest.cpp:87,89,93,95`; `tests/unit/OpeningTest.cpp:151,152,248,251`; `tests/unit/OverviewTest.cpp:213,214,464,468,472`; `tests/unit/SceneTest.cpp:28,141,177,201,228`; `tests/unit/TitleTest.cpp:345,350,356,385,391,408,410`; `tools/assets/ExportBranding.cpp:330,376`; `tools/migration/TitleSourceProbe.cpp:395,396` | 按 skill 建议评估标准库/现代 Qt 替代；先核对实际对象类型和项目约束。 |
| DEP-5 QPair — use std::pair (alias since Qt 6.0) | 4 | `tests/unit/ArchivePoolTest.cpp:364,366`; `tests/unit/TitleTest.cpp:162,259` | 按 skill 建议评估标准库/现代 Qt 替代；先核对实际对象类型和项目约束。 |
| DEP-7 qMin/qMax/qBound — use (std::min)()/(std::max)()/std::clamp() | 36 | `src/core/MusicTypes.cpp:25,32`; `src/render/ArchivePool.cpp:63`; `src/render/SelectionController.cpp:787`; `src/render/rhi/AtlasAllocator.cpp:19,114,137,146`; `src/render/rhi/AtlasUpload.cpp:40,47`; `src/render/rhi/PixelStorage.h:59`; `src/render/rhi/ResourcePlan.h:37,152`; `src/render/rhi/RhiRenderer.cpp:644,645,646`; `src/render/rhi/SceneItem.cpp:917`; `src/render/rhi/TransferAudit.h:43,45`; `src/services/CoverCache.cpp:292,378,398,424,452,455,508,587`; `tests/render/StartupChecks.cpp:79,83,329,439,709`; `tests/unit/DirectoryScannerTest.cpp:334`; `tests/unit/LibraryDataTest.cpp:294`; `tests/unit/MediaMetadataTest.cpp:107`; `tools/migration/RulerSourceProbe.cpp:610` | 按 skill 建议评估标准库/现代 Qt 替代；先核对实际对象类型和项目约束。 |
| ENM-2 Unscoped enum without explicit underlying type — add type to prevent BiC | 22 | `src/app/DeploymentProbe.cpp:31`; `src/app/DetailReview.cpp:58,64`; `src/app/MaterialReview.cpp:54`; `src/app/NavigationReview.cpp:39,47`; `src/app/OverviewReview.cpp:63,69`; `src/app/main.cpp:69`; `src/core/LibraryModels.h:52,108`; `src/media/MediaMetadataProbe.h:14`; `src/motion/MotionController.h:12`; `src/platform/macos/ApplicationMenu.mm:29`; `src/presentation/DetailModel.h:25`; `src/render/OpticalParameters.h:11`; `src/render/OverviewLabelsModel.h:30`; `src/render/rhi/DrawUniform.h:13`; `src/render/rhi/Postprocess.cpp:24`; `src/render/rhi/ReadbackImage.cpp:12`; `src/render/rhi/ReadbackQueue.h:13`; `src/render/rhi/RhiRenderer.cpp:38` | 为枚举/布尔参数明确类型、命名和枚举项风格；兼顾 Qt 元对象与实际 API 约束。 |
| ENM-9 Non-intuitive bool literal arguments — confusing at call site; use named enums | 68 | `src/app/DetailReview.cpp:371`; `src/presentation/PresentationController.cpp:50,52`; `src/presentation/TrackFocusController.cpp:77`; `src/render/rhi/ResourcePlan.h:55`; `src/render/text/TextLayoutAdapter.cpp:248,281,382`; `src/services/OnlineMetadataProvider.cpp:71`; `src/services/metadata/MetadataCoordinator.cpp:149`; `tests/render/LibraryChecks.cpp:84`; `tests/render/RhiOpaqueChecks.cpp:183,211,222`; `tests/render/RhiShadowChecks.cpp:444,446,448,450,452,456,458,460,461,463,465,467,469,470,472,474,475`; `tests/render/SmaaReference.h:119,120,130,131`; `tests/render/StartupChecks.cpp:472,1411`; `tests/unit/AudioAmbienceTest.cpp:257`; `tests/unit/DetailMotionTest.cpp:333,334`; `tests/unit/MediaMetadataTest.cpp:698`; `tests/unit/NavigationTest.cpp:71,99`; `tests/unit/PresentationTest.cpp:172,220,228`; `tests/unit/RealColumnsTest.cpp:111,252,253,254,275`; `tests/unit/RenderSupportTest.cpp:333,341,348`; `tests/unit/RhiMaterialTest.cpp:127,133`; `tests/unit/SceneTest.cpp:44,250`; `tests/unit/VerificationWindowTest.cpp:29,30,31,32,33,34,35,36`; `tools/migration/RulerSourceProbe.cpp:274,546` | 为枚举/布尔参数明确类型、命名和枚举项风格；兼顾 Qt 元对象与实际 API 约束。 |
| ENM-9 Two or more bool parameters — boolean trap; use named enums (f(x, LogRule::DontLog)) | 41 | `src/presentation/DetailModel.cpp:40,104`; `src/presentation/PresentationController.cpp:212`; `src/presentation/PresentationController.h:72`; `src/presentation/SurfaceTransition.cpp:41,54`; `src/presentation/SurfaceTransition.h:23,54`; `src/render/ArchiveField.cpp:45`; `src/render/ArchiveField.h:29`; `src/render/CaseClarity.cpp:71`; `src/render/CaseClarity.h:16`; `src/render/DetailMotion.cpp:57,70`; `src/render/DetailMotion.h:23,24`; `src/render/LayoutMetrics.cpp:56`; `src/render/PointerDragController.h:26`; `src/render/RealColumns.cpp:76`; `src/render/RealColumns.h:19`; `src/render/SelectionController.cpp:72,840`; `src/render/navigation/RulerController.cpp:409`; `src/render/navigation/RulerController.h:54,55`; `src/render/rhi/FrameUniforms.cpp:114`; `src/render/rhi/FrameUniforms.h:12,27`; `src/render/rhi/ReadbackImage.cpp:21`; `src/render/rhi/ReadbackImage.h:11`; `src/render/rhi/RenderTarget.cpp:27`; `src/render/rhi/RenderTarget.h:40`; `src/render/rhi/ResourcePlan.h:41`; `src/render/text/TextLayoutAdapter.cpp:358`; `src/services/PreferencesStore.cpp:267`; `src/services/PreferencesStore.h:29`; `tests/render/RhiOpaqueChecks.cpp:23`; `tests/render/SmaaReference.h:63`; `tests/render/StartupChecks.cpp:124,1467`; `tests/render/StartupChecks.h:13` | 为枚举/布尔参数明确类型、命名和枚举项风格；兼顾 Qt 元对象与实际 API 约束。 |
| ERR-1 QFile::open() return not checked — silent failure if file cannot open | 60 | `src/library/ArtworkStore.cpp:82,122`; `src/render/rhi/PreparedAssets.cpp:25`; `tests/integration/LibraryQmlTest.cpp:115`; `tests/render/ArchivePoolChecks.cpp:254,368`; `tests/render/RhiAoChecks.cpp:27,138,196`; `tests/render/RhiBokehChecks.cpp:182`; `tests/render/RhiFlickerChecks.cpp:75`; `tests/render/RhiFloorChecks.h:13`; `tests/render/RhiFogChecks.cpp:41`; `tests/render/RhiReviewInput.h:22`; `tests/render/RhiShadowChecks.cpp:489`; `tests/render/RhiShellChecks.cpp:73`; `tests/render/StartupChecks.cpp:835,845,1407`; `tests/unit/ArchivePoolTest.cpp:135,417`; `tests/unit/AudioAmbienceTest.cpp:44,161,188,226,266`; `tests/unit/CoverCacheTest.cpp:813`; `tests/unit/DetailMotionTest.cpp:116,169`; `tests/unit/DirectoryScannerTest.cpp:202`; `tests/unit/JsonFileTest.cpp:23,35`; `tests/unit/LibraryDataTest.cpp:434,438,765`; `tests/unit/LightingTest.cpp:100`; `tests/unit/LoggingTest.cpp:106,136`; `tests/unit/MediaMetadataTest.cpp:66,386`; `tests/unit/MotionTest.cpp:140`; `tests/unit/OnlineMetadataTest.cpp:327,351,997`; `tests/unit/OpeningTest.cpp:77`; `tests/unit/RealColumnsTest.cpp:119,246,256,268,278`; `tests/unit/RhiMaterialTest.cpp:160,210`; `tests/unit/SceneTest.cpp:90`; `tests/unit/SingleInstanceTest.cpp:152`; `tools/assets/ExportBranding.cpp:66,73,80,197`; `tools/assets/GeometryAudit.cpp:129,136` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-2 QJsonDocument::fromJson() not validated — check isNull()/isObject() | 44 | `src/app/MaterialReview.cpp:402`; `src/media/InterfaceSoundController.cpp:32`; `src/render/opening/OpeningInk.cpp:220`; `tests/integration/AssetTest.cpp:44,71,103`; `tests/integration/LibraryQmlTest.cpp:117`; `tests/render/ArchivePoolChecks.cpp:256`; `tests/render/RhiAoChecks.cpp:139,198`; `tests/render/RhiBokehChecks.cpp:183`; `tests/render/RhiFlickerChecks.cpp:76`; `tests/render/RhiFogChecks.cpp:42`; `tests/render/RhiShadowChecks.cpp:490`; `tests/unit/ArchivePoolTest.cpp:136,419`; `tests/unit/AudioAmbienceTest.cpp:163`; `tests/unit/DetailMotionTest.cpp:117,171`; `tests/unit/DirectoryScannerTest.cpp:358`; `tests/unit/JsonFileTest.cpp:24`; `tests/unit/LibraryDataTest.cpp:436,439,767`; `tests/unit/LightingTest.cpp:101`; `tests/unit/MediaMetadataTest.cpp:666,682`; `tests/unit/MotionTest.cpp:141,145`; `tests/unit/OpeningTest.cpp:78`; `tests/unit/OverviewTest.cpp:66`; `tests/unit/RealColumnsTest.cpp:120,257`; `tests/unit/RhiMaterialTest.cpp:161,211`; `tests/unit/SceneTest.cpp:91`; `tests/unit/SettingsTest.cpp:135,183,226`; `tools/assets/ExportBranding.cpp:558,796`; `tools/assets/PrepareAssets.cpp:87,95,234` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-3 QNetworkReply data read without prior error check | 1 | `src/services/metadata/MetadataHttp.cpp:193` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-4 Hardcoded http:// URL — use https:// | 6 | `tests/unit/QmlBehaviorTest.cpp:181,211,218,228,238`; `tools/assets/PrepareAssets.cpp:354` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-6 QString::arg() has %2 but only 1 .arg() calls | 1 | `src/presentation/DetailModel.cpp:164` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-6 QString::arg() has %3 but only 1 .arg() calls | 2 | `src/render/text/TextLayoutAdapter.cpp:202,254` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-6 QString::arg() has %4 but only 1 .arg() calls | 1 | `tests/render/StartupChecks.cpp:1549` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-6 QString::arg() has %5 but only 1 .arg() calls | 1 | `tests/render/OpeningChecks.cpp:237` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| ERR-9 QNetworkAccessManager without sslErrors handling | 15 | `src/services/OnlineMetadataProvider.cpp:22`; `src/services/OnlineMetadataProvider.h:8,19`; `src/services/metadata/MetadataHttp.cpp:12,47,49,60,71`; `src/services/metadata/MetadataHttp.h:10,25`; `src/services/metadata/MetadataWorker.cpp:64,112`; `src/services/metadata/MetadataWorker.h:8,20`; `tests/unit/OnlineMetadataTest.cpp:200` | 沿实际控制流检查成功/失败、JSON 类型、信号和调用返回值；为真实遗漏保留可达失败路径。 |
| HDR-3 Unprotected numeric_limits::min/max — use (std::numeric_limits<T>::min)() | 24 | `src/core/MusicTypes.cpp:89`; `src/core/RenderQualityParameters.h:15`; `src/render/RenderQuality.cpp:23,50,51`; `src/render/rhi/AtlasAllocator.cpp:17,20,140`; `src/render/rhi/ReadbackImage.cpp:33`; `src/render/rhi/SceneItem.cpp:55`; `src/render/text/ReelInk.cpp:37`; `src/services/CoverCache.cpp:268`; `tests/unit/CoverCacheTest.cpp:88,349,394`; `tests/unit/LibraryDataTest.cpp:800,801`; `tests/unit/RenderQualityTest.cpp:69`; `tests/unit/RenderSupportTest.cpp:104`; `tools/assets/GeometryAudit.cpp:80,291`; `tools/migration/ProbeInput.h:18,19,28` | 检查 Windows min/max 宏边界；在需要处使用括号保护或一致的 NOMINMAX；核对目标编译定义。 |
| HDR-3 Unprotected std::min/max — use (std::min)(a,b) for Windows macro safety | 243 | `src/app/DetailReview.cpp:149,151,548,550`; `src/app/MaterialReview.cpp:215,221,246,280`; `src/app/OverviewReview.cpp:145,536`; `src/core/LibraryOrder.cpp:36,91,126`; `src/core/Logging.cpp:257`; `src/core/SearchListModel.cpp:253`; `src/media/BackgroundMusicController.cpp:161`; `src/media/PlaybackController.cpp:557`; `src/motion/FrameClock.cpp:60`; `src/motion/MotionFunctions.cpp:114,146`; `src/motion/QuinticTrack.cpp:37,62`; `src/motion/SampledReelCurve.h:24,52`; `src/motion/ScalarSpring.h:18`; `src/platform/macos/DisplayMonitor.mm:40`; `src/platform/windows/DisplayMonitor.cpp:124`; `src/presentation/DetailModel.cpp:285,286,299,300`; `src/presentation/GenreRowsModel.cpp:85,117`; `src/presentation/OptionMetrics.cpp:61`; `src/presentation/PresentationController.cpp:124,135`; `src/presentation/SurfaceTransition.cpp:66,92`; `src/presentation/TrackFocusController.cpp:29,117,155,187`; `src/render/CaseClarity.cpp:89`; `src/render/LayoutMetrics.cpp:30,54,60,61`; `src/render/OverviewController.cpp:235,240,311,324,336`; `src/render/OverviewLabelsModel.cpp:214,234,328,329`; `src/render/RealColumns.cpp:39,41,50,92`; `src/render/RenderQuality.cpp:23,53,63`; `src/render/RowLightController.cpp:32,34`; `src/render/SceneAdapter.cpp:195,198,201,206,434,436`; `src/render/SceneMath.h:82,83`; `src/render/SelectionController.cpp:623,627,790`; `src/render/SelectionLighting.cpp:236`; `src/render/navigation/InputRouter.cpp:153,287,288`; `src/render/navigation/OrdinalMotion.cpp:39,78`; `src/render/navigation/RulerController.cpp:238,239,244,282`; `src/render/opening/OpeningController.cpp:75,93`; `src/render/opening/OpeningFrame.cpp:62,111,159,180,181`; `src/render/opening/OpeningInk.cpp:82,116,174`; `src/render/rhi/AtlasUpload.cpp:76`; `src/render/rhi/DiagnosticSamples.h:34,40`; `src/render/rhi/FrameUniforms.cpp:82`; `src/render/rhi/Postprocess.cpp:193`; `src/render/rhi/RhiItem.cpp:65`; `src/render/rhi/RhiRenderer.cpp:471,731,748,759`; `src/render/rhi/SceneCovers.cpp:159,162,167`; `src/render/rhi/SceneGeometryData.cpp:38,39`; `src/render/rhi/SceneItem.cpp:851,874`; `src/render/text/TextLayoutAdapter.cpp:98,130,134,178,205,208,209,274,389,392`; `src/render/text/TitleReel.cpp:36,38,45,72,164,169,171,243,247,259,260,262,273`; `src/render/text/TransportMotion.cpp:65`; `src/services/CoverCache.cpp:83,91,105,108,376,548`; `src/services/metadata/MetadataHttp.cpp:249`; `tests/render/ArchivePoolChecks.cpp:288,392,511`; `tests/render/CoverSamplingChecks.h:32,33,167`; `tests/render/ExperimentChecks.cpp:201`; `tests/render/LibraryChecks.cpp:232,594,630,696,814`; `tests/render/OpeningChecks.cpp:146`; `tests/render/PresentationChecks.cpp:385,386`; `tests/render/RhiAoChecks.cpp:91,106,113`; `tests/render/RhiBokehChecks.cpp:148`; `tests/render/RhiFloorChecks.h:77,116,155,205,206,227`; `tests/render/RhiFogChecks.cpp:86,89`; `tests/render/RhiOpaqueChecks.cpp:75,77,153,157,161,170,186,196,202`; `tests/render/RhiPostprocessChecks.cpp:300`; `tests/render/RhiShadowChecks.cpp:62,63,64,65,107,108,197`; `tests/render/RhiShellChecks.cpp:170,216,321,324`; `tests/render/RhiSmaaChecks.cpp:117`; `tests/render/SmaaReference.h:42,53,163`; `tests/support/OffscreenSurface.h:224`; `tests/unit/DetailMotionTest.cpp:141,195`; `tests/unit/LightingTest.cpp:108,135,142,144,147,151,155,167,169,179`; `tests/unit/MotionTest.cpp:277,278`; `tests/unit/OnlineMetadataTest.cpp:188`; `tests/unit/OpeningTest.cpp:144`; `tests/unit/OverviewTest.cpp:167,213,293`; `tests/unit/QmlBehaviorTest.cpp:573`; `tests/unit/RealColumnsTest.cpp:151`; `tests/unit/SceneTest.cpp:166,210,273`; `tools/assets/ExportBranding.cpp:127,128,129,130,379`; `tools/assets/GeometryAudit.cpp:84,85,299,310`; `tools/migration/RulerSourceProbe.cpp:74,83,518,522,533,649`; `tools/migration/TitleSourceProbe.cpp:60,73,131,188,317,740,820` | 检查 Windows min/max 宏边界；在需要处使用括号保护或一致的 NOMINMAX；核对目标编译定义。 |
| INC-2 C++ standard header before Qt header — reorder by descending specificity | 12 | `src/app/FixedTextureData.cpp:1`; `src/core/SearchListModel.cpp:3`; `src/library/LibraryStore.cpp:2`; `src/render/rhi/Postprocess.cpp:2`; `src/render/rhi/ReviewGlCapabilities.h:16`; `src/render/rhi/RhiRenderer.cpp:19`; `src/services/PreferencesStore.cpp:2`; `tests/render/RhiAoChecks.cpp:11`; `tests/unit/CoverCacheTest.cpp:9`; `tests/unit/RenderSupportTest.cpp:9`; `tests/unit/RhiMaterialTest.cpp:3`; `tools/assets/ExportBranding.cpp:19` | 按实际依赖和 Qt 风格检查 include；公共头保持自包含。 |
| LCY-1 QNetworkReply read without deleteLater() in this handler | 1 | `src/services/metadata/MetadataHttp.cpp:193` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-3 Q_ASSERT wraps side-effectful call — compiled out in release | 6 | `src/library/LibraryStore.cpp:256,283,310,319`; `src/render/StartupCoordinator.cpp:49`; `src/services/LibrarySession.cpp:73` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_connections grows without size cap — unbounded memory growth | 6 | `src/media/PlaybackController.cpp:206,211,219,226,240,251` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_entries grows without size cap — unbounded memory growth | 1 | `src/render/rhi/ResourceInventory.h:69` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_generation grows without size cap — unbounded memory growth | 1 | `src/library/LibraryStore.cpp:249` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_order grows without size cap — unbounded memory growth | 1 | `tests/render/PresentationChecks.cpp:51` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_path grows without size cap — unbounded memory growth | 1 | `src/library/LibraryStore.cpp:473` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| LCY-5 m_paths grows without size cap — unbounded memory growth | 1 | `tools/assets/AssetTransaction.h:37` | 追踪 QObject/资源的实际所有者、连接 context、销毁与在途回调。 |
| PAT-11 QRegularExpression constructed inside loop — compile once before loop | 2 | `src/render/opening/BrandPaths.h:45`; `src/services/metadata/WikipediaSource.cpp:90` | 检查该模式的业务目的、容器行为、访问前提和 Qt/标准库接口后再调整。 |
| PAT-12 Non-const range-for reference — may trigger COW detach; use const auto& if read-only | 48 | `src/library/DirectoryScanner.cpp:156,439`; `src/library/LegacyImporter.cpp:294,296`; `src/media/InterfaceSoundController.cpp:43,66,74,95,105`; `src/media/MediaMetadataProbe.cpp:124,133,239,255,320,367`; `src/motion/MotionController.cpp:85`; `src/render/ArchiveField.cpp:40,55`; `src/render/OverviewLabelsModel.cpp:89,275,401`; `src/render/SelectionController.cpp:799,932`; `src/render/navigation/RulerController.cpp:321`; `src/render/rhi/AtlasAllocator.cpp:187,193,197`; `src/render/rhi/AtlasUpload.cpp:64`; `src/render/rhi/Postprocess.cpp:293`; `src/render/rhi/ReadbackQueue.cpp:47`; `src/render/rhi/RenderTarget.h:49,51`; `src/render/rhi/SceneCovers.cpp:172,174,235,299,448,474`; `src/render/rhi/SceneItem.cpp:537`; `src/render/text/TextLayoutAdapter.cpp:371`; `tests/render/OutputReference.h:26`; `tests/render/SmaaReference.h:35,36`; `tests/unit/AtlasUploadTest.cpp:118`; `tests/unit/AudioAmbienceTest.cpp:194`; `tests/unit/LibraryDataTest.cpp:102`; `tools/assets/GeometryAudit.cpp:255,317` | 检查该模式的业务目的、容器行为、访问前提和 Qt/标准库接口后再调整。 |
| PAT-2 std::optional default-constructed — use std::nullopt explicitly (GCC warning bug) | 30 | `src/app/ApplicationOptions.h:40`; `src/core/MusicTypes.h:25,54`; `src/presentation/PresentationController.cpp:54,151`; `src/presentation/SurfaceTransition.h:61`; `src/render/ArchiveField.h:21`; `src/render/CaseClarity.h:26`; `src/render/ReturningCasePool.h:16`; `src/render/RowLightController.h:54`; `src/render/SceneAdapter.cpp:108`; `src/render/SelectionController.cpp:90,131,132,977,978`; `src/render/SelectionLighting.h:204`; `src/render/navigation/InputRouter.h:70`; `src/render/opening/OpeningController.h:104`; `src/render/rhi/RhiRenderer.cpp:298`; `src/render/rhi/SceneItem.cpp:89,90,144,146`; `src/render/text/TitleReel.h:98`; `src/services/PreferencesStore.cpp:46`; `src/services/metadata/MetadataWorker.cpp:110,140`; `tests/render/CaptureMailbox.h:17`; `tests/support/DriverInfoMailbox.h:28` | 检查该模式的业务目的、容器行为、访问前提和 Qt/标准库接口后再调整。 |
| PAT-7 QMap usage — verify copying is needed; std::map saves ~1.7KiB text | 10 | `src/app/AssetCatalog.cpp:24,26,51`; `src/render/rhi/AtlasUpload.h:61`; `tests/render/StartupChecks.cpp:1230,1262,1276`; `tools/assets/ExportBranding.cpp:654`; `tools/assets/PrepareAssets.cpp:99,313` | 检查该模式的业务目的、容器行为、访问前提和 Qt/标准库接口后再调整。 |
| TMO-1 Integer timeout/interval parameter — use QDeadlineTimer or std::chrono | 3 | `src/motion/SampledReelCurve.h:10`; `tests/render/StartupChecks.cpp:405`; `tests/unit/MediaMetadataTest.cpp:39` | 为时间值明确单位和类型；与所选 Qt API 的 chrono 支持一致。 |
| VAR-3 Direct brace initialization — prefer copy-init (var = {...}) for safety | 135 | `src/app/DeploymentProbe.cpp:70,113,148`; `src/app/DetailReview.cpp:158,582,731`; `src/app/MaterialReview.cpp:294,435`; `src/app/NavigationReview.cpp:281,397`; `src/app/OverviewReview.cpp:150,540,668`; `src/app/VerificationWindow.h:70`; `src/core/LibraryOrder.cpp:92`; `src/core/MusicTypes.cpp:207`; `src/library/DirectoryScanner.cpp:98`; `src/media/PlaybackController.cpp:487`; `src/motion/MotionController.h:47,113`; `src/platform/windows/DisplayMonitor.cpp:63,86,94,116`; `src/presentation/DetailModel.cpp:385`; `src/presentation/PanelController.h:41`; `src/presentation/PresentationController.cpp:49,51`; `src/render/OverviewController.cpp:70`; `src/render/OverviewLabelsModel.cpp:75`; `src/render/RealColumns.h:38`; `src/render/RenderQuality.h:51`; `src/render/SceneAdapter.cpp:218,220`; `src/render/SelectionController.cpp:783`; `src/render/navigation/OrdinalMotion.h:21`; `src/render/opening/BrandPaths.h:83`; `src/render/opening/OpeningFrame.cpp:188`; `src/render/opening/OpeningInk.h:72`; `src/render/rhi/AtlasAllocator.cpp:118`; `src/render/rhi/FrameUniforms.cpp:117`; `src/render/rhi/PixelStorage.h:56`; `src/render/rhi/Postprocess.cpp:104,295`; `src/render/rhi/RhiRenderer.cpp:961,1034`; `src/render/rhi/SceneCovers.cpp:65,82`; `src/render/rhi/SceneGeometryData.cpp:32`; `src/render/rhi/SceneGeometryData.h:12`; `src/render/rhi/TransferAudit.h:22`; `src/render/text/TextLayoutAdapter.cpp:224`; `src/render/text/TitleReel.h:29,71`; `src/services/MetadataPresentation.cpp:110`; `tests/render/ArchivePoolChecks.cpp:345,601,624`; `tests/render/CoverChecks.cpp:219`; `tests/render/CoverSamplingChecks.h:29,62`; `tests/render/LibraryChecks.cpp:449,1059`; `tests/render/OpeningChecks.cpp:160,161,162,163,320`; `tests/render/OverviewChecks.cpp:63,276`; `tests/render/PresentationChecks.cpp:308,391,490`; `tests/render/RhiBokehChecks.cpp:53,130`; `tests/render/RhiPostprocessChecks.cpp:517,525`; `tests/render/RhiShadowChecks.cpp:55`; `tests/render/SmaaReference.h:79`; `tests/render/StartupChecks.cpp:551,554,560,561,562,563,564,565,566,568,569,570,571,572,573,808,1389,1473`; `tests/unit/DetailMotionTest.cpp:32,33,35,36`; `tests/unit/LibraryDataTest.cpp:79`; `tests/unit/MotionTest.cpp:194`; `tests/unit/NavigationTest.cpp:80`; `tests/unit/OpeningTest.cpp:27,28,29`; `tests/unit/OverviewTest.cpp:29,30,31,33,34,39,355`; `tests/unit/PlaybackTest.cpp:29,30`; `tests/unit/PresentationTest.cpp:52,72`; `tests/unit/RenderSupportTest.cpp:259,463`; `tests/unit/StartupTest.cpp:32,33,34,35`; `tests/unit/TrackFocusTest.cpp:24,25,27,28,29`; `tools/assets/ExportBranding.cpp:392,756`; `tools/assets/GeometryAudit.cpp:142,188,205,220` | 按 skill 统一初始化表达；对有意义的类型/构造语义保留明确说明。 |


---

### Deep analysis findings

#### [D-001] Material review accepts invalid render dimensions until a fatal GPU-target check
- **File**: `src/app/MaterialReview.cpp:140`
- **Category**: Error Handling & Validation
- **Confidence**: 96/100
- **Finding**: Fixture `width` and `height` are converted with `toInt()` and passed directly to `QQuickWindow::resize()` and `QQuickItem::setSize()`. The input-validation loop checks only array lengths, so a structurally accepted fixture can contain an oversized dimension such as `2147483647`. In the default offscreen path this reaches `OffscreenSurface::render()` and calls `qFatal()` when the render target exceeds the device texture limit. Invalid fixture data consequently aborts the process rather than following the existing `InvalidReviewInput` failure protocol. Wrong JSON types also silently become default dimensions. This violates repository CPP-11 and CPP-15.
- **Trace**: `main.cpp:413–416` passes the user-selected `--verify-materials` fixture to `reviewMaterials()`; `MaterialReview.cpp:405–414` validates position/parameter/projection counts but no dimensions; `captureMaterialReview():140–142,157–158,178` converts and applies dimensions; `tests/support/OffscreenSurface.h:210–216` calculates target size and aborts on excess size. This is separate from the known JSON parse-validation lint diagnostic at line 402.
- **Mitigation**: Validate each supplied dimension’s JSON type, integer representation, positive range, and explicit rendering budget before creating or resizing any window. Check the effective DPR and device limit before allocating the target, and return the ordinary review failure result for unsupported input.

#### [D-002] Help exits through graphics failure before printing usage
- **File**: `src/app/main.cpp:145`
- **Category**: Performance & Code Quality
- **Confidence**: 95/100
- **Finding**: `--help` / `--help-all` is processed only at lines 175–183, after graphics selection, `QGuiApplication` creation, backend probing, and embedded font initialization. A user requesting usage with an unsupported `QSG_RHI_BACKEND`, unavailable explicitly selected Vulkan runtime, or failed font registration receives exit code 22/6 without help. The command also performs graphics initialization unnecessarily.
- **Trace**: `main()` lines 121–129 recognizes help only to isolate logging; lines 145–168 can return before parsing. `parseApplicationOptions()` lines 94–95 already provides an early help result. `GraphicsBackend.h` lines 73–74 rejects unsupported backend names; `BackendCapabilities.cpp` lines 79–82 rejects unavailable explicit Vulkan. `ApplicationOptionsTest::helpDoesNotStartASession()` tests the parser alone, so it does not cover this executable path.
- **Mitigation**: Process help and argument errors before initializing the GUI backend or loading product assets. Add an executable regression that requests help with an invalid backend setting and verifies successful usage output without graphics initialization.

#### [D-003] JSON 整数验证接受负曲号和负碟号
- **File**: `src/core/MusicTypes.cpp:293`
- **Category**: Error Handling & Validation
- **Confidence**: 98/100
- **Finding**: `validIntegers()`只验证整数性及 `int` 可表示范围，随后将负 `trackNumber`、`discNumber`、`discCount`、`bitsPerSample`、`sampleRate` 和 `bitrate` 接纳为有效领域值。例如合法整数 `trackNumber: -1` 会成功通过读取，并直接显示为负曲号；负碟号还参与排序和在线曲目匹配。缺失值已有 `std::optional` 表达，无须用负数表示未知。这违反项目 CPP-11 对外部数值符号和领域范围的验证要求。
- **Trace**: `MusicTypes.cpp:79–97` 的 `validIntegers()/integer()` → `readLibrary():248–251,293–308`；生产缓存入口 `LibraryStore.cpp:330–366` 和旧库导入 `LegacyImporter.cpp:285–287` 均使用该解析器。`DetailModel.cpp:189,210` 直接显示曲号/碟号，`DirectoryScanner.cpp:212–215` 使用其排序，`MusicBrainzSource.cpp:294–295` 使用其匹配。相比之下，`MediaMetadataProbe.cpp:176–178,183–185` 仅接纳正曲号和正码率。现有数值回归 `LibraryDataTest.cpp:800–825` 未覆盖这些负整数。
- **Mitigation**: 按字段定义域验证符号：曲号、碟号及技术参数应为正值，计数应非负；缺失保留为 optional。对失败维持现有事务式拒绝，并补充负整数与边界零值的针对性回归。

#### [D-004] 库解析错误绕过 Qt 翻译系统
- **File**: `src/core/MusicTypes.cpp:170`
- **Category**: Error Handling & Validation
- **Confidence**: 100/100
- **Finding**: 同一解析器中，数值错误使用 `QCoreApplication::translate()`，数组、字段类型、重复 ID 等错误却直接返回英文 `QString` 字面量。生产调用会把这些文本拼入可见的缓存/导入错误，因此中文界面仍显示未翻译的具体原因，且这些文本无法被 lupdate 提取。这违反项目 QT-19/QT-20。
- **Trace**: `readLibrary()` 的 `fail()` 将原文写入输出错误（`MusicTypes.cpp:165–168`）；未翻译路径包括 `170–177,202–215,221–244,269–287,321–342`，与 `205–206,248–249,293–302` 的已翻译错误形成不一致。`LibraryStore.cpp:349–355` 将其附加到已翻译的可见错误；`LegacyImporter.cpp:286–287` 原样保留该错误。
- **Mitigation**: 将所有面向用户的解析错误统一纳入明确 context 的 Qt 翻译调用，并更新相应 TS 目录；保留 ID、JSON 键等协议字段不翻译。

#### [D-005] Enum declarations omit the required trailing comma
- **File**: `src/library/LibraryStore.cpp:51`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: `LibraryStore::Private::Kind` ends with `Metadata` without a trailing comma. `BackgroundMusicController::SongActivity` similarly ends with `Playing` at `src/media/BackgroundMusicController.h:24`. Both violate A4 / ENM-1. This is a style finding with no identified runtime impact.
- **Trace**: Inspected both complete enum declarations; these locations are absent from the supplied lint results.
- **Mitigation**: Add a trailing comma after each enum’s final enumerator.

#### [D-006] Artwork re-probe timeout becomes permanently cached
- **File**: `src/media/MediaMetadataProbe.cpp:215`
- **Category**: Error Handling & Validation
- **Confidence**: 98/100
- **Finding**: A timeout preserves an existing current `_qtProbeVersion`. If a previously probed directory album loses its external cover, its tracks are re-probed for embedded artwork. When that attempt times out without an image, `persistArtwork()` records `_embeddedCover = null` (lines 150–154). On subsequent unchanged scans, `coverKnown` becomes true and the retained Qt version causes the track to be skipped (lines 333–342). Embedded artwork discovery therefore never retries, contrary to the documented timeout retry policy.
- **Trace**: `ProbeWorker::pump()` artwork admission → `complete()` → `apply()` timeout handling → `persistArtwork()`. Confirmed `DirectoryScanner.cpp:188–195` preserves these fields for unchanged files. `MediaMetadataTest::timeout()` only checks an initially uncached track, whose Qt version was absent already.
- **Mitigation**: Invalidate the completed-probe marker on timeout and avoid committing an absent-artwork result from an unfinished probe. Cover the sequence of successful cached probing, external-cover removal, timeout, and another unchanged scan.

#### [D-007] Indicator geometry updates prematurely complete the content fade
- **File**: `src/presentation/PresentationController.cpp:100`
- **Category**: Performance & Code Quality
- **Confidence**: 97/100
- **Finding**: `setTabIndicator(..., false)` calls `advanceTab(TabSlideSeconds)`. That function advances both indicator geometry and content opacity, so it immediately completes any active 150 ms content fade. Resizing or changing tab-button geometry during a reveal therefore snaps the content to full opacity, violating the independent animation durations.
- **Trace**: `Detail.qml:30–37` starts the content fade through `revealTabContent()`. `DetailTabs.qml:32–33,121–127` synchronizes indicator geometry without animation when button geometry or container width changes. `PresentationController.cpp:133–140` advances the content fade using the same synthetic 180 ms step.
- **Mitigation**: Complete the indicator geometry independently when animation is disabled; advance content opacity only from the frame clock or an explicit reduced-motion completion.

#### [D-008] 定位请求在同步取消或替换后仍继续导航
- **File**: `src/presentation/TrackFocusController.cpp:71`
- **Category**: Performance & Code Quality
- **Confidence**: 93/100
- **Finding**: `request()` 发出 `changed()` 后直接继续调用 `PresentationController::select()` 或 `LibrarySession::navigate()`。同步槽若调用 `cancel()`，请求已失效，但旧请求仍改变页面选择；若同步槽提交新请求，外层旧请求仍能继续导航，破坏最后请求优先语义。入口中的 `cancel()` 和 `navigationRequested()` 同样没有返回后的 revision 检查。
- **Trace**: 检查 `request()`、`cancel()`、`ready()`、其他已经使用 revision 防护的 `beginReveal()`/`startHighlight()`/`advance()`；追踪 main.cpp 的连接及 TrackFocusTest 中同步取消的既有契约。现有测试覆盖滚动和高亮阶段，未覆盖请求发布阶段。
- **Mitigation**: 为一次 request 建立独立 revision，在可同步重入的通知返回后检查请求是否仍有效；旧调用发现取消或替换后立即停止导航。增加请求发布期间取消和替换的回归覆盖。

#### [D-009] 开启减少动态会冻结正在显示的定位高亮
- **File**: `src/presentation/TrackFocusController.cpp:175`
- **Category**: Performance & Code Quality
- **Confidence**: 96/100
- **Finding**: `setReducedMotion(true)` 只处理滚动阶段。若已经处于 `m_pulsing`，它既不将 `m_highlight` 更新为减少动态模式规定的 1，也不通知变化；随后 `advance()` 因 `m_reduced` 为 true 跳过高亮更新，直至取消。刚进入普通高亮时值为 0，因此此时切换会使整个剩余高亮不可见；其他时刻切换则冻结为部分透明。
- **Trace**: `startHighlight()` 在普通模式设为 0、减少动态模式设为 1；`advance()` 的高亮计算受 `!m_reduced` 限制。main.cpp 将 reducedMotionChanged 接到该 setter。现有测试只验证开始定位前开启减少动态。
- **Mitigation**: 切换到减少动态时同时收敛正在进行的高亮状态并发送必要通知，明确保留时间的处理规则；覆盖普通高亮开始及中途切换的情况。

#### [D-010] 扫描波分支及其逐帧状态永久不可达
- **File**: `src/render/ArchiveField.cpp:98`
- **Category**: Performance & Code Quality
- **Confidence**: 100/100
- **Finding**: `m_scanBlend` 初始为 0，唯一赋值仍是置 0，其他操作仅乘以衰减系数，因此扫描波条件永久为 false。`m_scanTime` 仍逐帧递增，`m_scanBlend` 仍逐帧计算指数衰减，但这些状态不会产生输出。
- **Trace**: 全根目录检索 `m_scanBlend`、`m_scanTime`，确认只有 ArchiveField 中的初始化、清零、衰减和该采样分支；追踪 SelectionController 的 ArchiveField 调用，未发现启动扫描波的入口。
- **Mitigation**: 按当前产品意图移除退役分支及状态；若扫描波仍是必需行为，则补齐明确的启动入口和生命周期。不要保留无效果的逐帧维护。

#### [D-011] Opening reset changes detail without its property notification
- **File**: `src/render/SceneAdapter.cpp:348`
- **Category**: Performance & Code Quality
- **Confidence**: 96/100
- **Finding**: `setOpeningTime()` resets `m_detail` to zero without emitting `detailChanged()`. Replaying the opening from a nonzero detail state therefore leaves observers of the `detail` property stale. `frameChanged()` does not notify bindings subscribed to `detailChanged()`.
- **Trace**: `SceneAdapter.h:40` declares the notification dependency. `OpeningController::replay()` calls `setOpeningTime()` at `src/render/opening/OpeningController.cpp:39`; its preceding selection reset does not notify `SceneAdapter::detailChanged`. `qml/Yalahoo/Music/scene/GeometryScene.qml:15` binds `adapter.detail`. Production RHI reads the value during frame publication, so that consumer does receive the new value.
- **Mitigation**: Detect whether detail changed, establish the complete opening state, and emit `detailChanged()` when the reset changes the property.

#### [D-012] Phase 枚举末项缺少尾随逗号
- **File**: `src/render/opening/OpeningController.h:93`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: `Phase::Done` 缺少尾随逗号，不符合 A4 的 ENM-1。此项仅为代码规范问题。
- **Trace**: 已读取完整 Phase 定义；不在已知 lint 结果中。
- **Mitigation**: 为枚举末项补充尾随逗号。

#### [D-013] OpeningFrame 用 QString 表达封闭阶段词汇
- **File**: `src/render/opening/OpeningFrame.h:24`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 98/100
- **Finding**: `phase` 的封闭取值为 logo/orbit/slogan/array，却在内部以 QString 保存，不符合 A4 的 ENM-8，无法获得枚举域和穷尽检查。
- **Trace**: OpeningFrame 默认值、openingFrame() 的四阶段赋值及 values() 字符串输出均已检查；该词汇由项目定义。
- **Mitigation**: 内部使用具名枚举，在 QVariantMap 输出边界转换为既有字符串。

#### [D-014] Standalone cover lease keeps the old decoder generation after selection reset
- **File**: `src/render/rhi/SceneCovers.cpp:135`
- **Category**: Ownership & Lifecycle
- **Confidence**: 100/100
- **Finding**: Switching from a selection-backed scene to standalone mode recreates the demo cover lease with the default `coverGeneration = 1`. However, `resetPool()` has just called `m_covers.reset()`, which increments the decoder generation. Every subsequent demo snapshot therefore fails `SelectedCoverPool::upgrade()`’s generation check. The standalone cover remains transparent or retains its previous image, and cannot accept later resolution upgrades.
- **Trace**: `SceneCovers::setSelection()` → `Private::resetPool()` at lines 257–270 → `CoverCache::reset()` at lines 296–305; `SelectedCoverPool::acquire()` defaults `coverGeneration` to 1 in `AtlasAllocator.h:89`; `SelectedCoverPool::upgrade()` rejects generation mismatches at `AtlasAllocator.cpp:248`. Both `requestDemoCover()` and `handleCoverReady()` rely on that upgrade succeeding. `SceneItem::setAdapter()` and selection changes in `updatePolish()` reach this transition.
- **Mitigation**: Create the replacement demo lease using the current `m_covers.generation()`, as the owned-cover path already does.

#### [D-015] Main-title ellipsis overlaps right-to-left text
- **File**: `src/render/text/TextLayoutAdapter.cpp:250`
- **Category**: A4. API, Naming & C++ Correctness
- **Confidence**: 95/100
- **Finding**: The two-line clamping path removes glyphs from the end of logical string order, checks only their right edge against the available width, then places the ellipsis at `last.x + last.width`. Hebrew/Arabic glyphs advance toward decreasing visual x. Their last logical glyph is consequently near the left edge, so the width check often removes nothing and the ellipsis is placed over retained text. Long RTL or mixed-direction album titles reach this path when they still exceed two lines at the minimum font size.
- **Trace**: `TitleReel::reconcile()` passes album titles to `measure()` (`TitleReel.cpp:576`); `fitTitle()` selects clamping when no candidate fits (`TextLayoutAdapter.cpp:296`); `shape()` sorts groups by logical string index (`184–188`) and performs the direction-insensitive placement (`250–259`). The adjacent transport implementation explicitly addresses this problem through Qt bidi-aware elision (`278–281`). Existing Hebrew/Arabic/mixed elision tests call `measure(..., transport=true)` (`tests/unit/TitleTest.cpp:122`), leaving album-title clamping uncovered.
- **Mitigation**: Elide the final visible line through Qt’s bidirectional text handling, then shape the resulting complete line. Preserve grapheme boundaries and test clamped Hebrew, Arabic, and mixed-direction album titles for ellipsis placement, overlap, and width.

#### [D-016] Scene-pipeline enums omit required trailing commas
- **File**: `src/render/rhi/ScenePipeline.h:12`
- **Category**: A4. API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: `ScenePass::Shell` and `InstanceMode::Instanced` lack trailing commas, violating the requested ENM-1 enum convention.
- **Trace**: Complete declarations inspected at `ScenePipeline.h:9–17`; these locations are absent from the supplied lint results.
- **Mitigation**: Apply the enum trailing-comma convention consistently.

#### [D-017] Output image allocation failure is cached as successful artwork
- **File**: `src/services/CoverCache.cpp:535`
- **Category**: Error Handling & Validation
- **Confidence**: 93/100
- **Finding**: `paint()` constructs the destination `QImage` without checking allocation success, and `makeMipmaps()` likewise accepts unchecked conversion and allocation results. When painting returns a null image after source decoding succeeded, `pixels->error` remains empty. `complete()` caches and publishes that snapshot as valid artwork. `SelectedCoverPool::upgrade()` rejects its incorrect image dimensions, but `SceneCovers::requestDetailedCover()` subsequently hits the cached snapshot and returns without setting `failedResolution`. With a retained thumbnail, preparation can remain incomplete indefinitely because the requested detailed image is neither usable nor recognized as failed.
- **Trace**: Followed `request()` validation → `paint()` at lines 387–435 → `decode()` at lines 517–564 → `complete()` at lines 568–605; confirmed downstream handling in `src/render/rhi/AtlasAllocator.cpp:242` and `src/render/rhi/SceneCovers.cpp:378`. Existing allocation-policy coverage checks source decoding limits, not destination canvas or mip allocation failure.
- **Mitigation**: Check conversion, canvas, mip and tile allocation results before publishing. Propagate an explicit terminal failure through the existing error protocol, preserve available lower-resolution artwork, and avoid admitting malformed snapshots as successful cache entries.

#### [D-018] Rejected release becomes the preferred release for future queries
- **File**: `src/services/metadata/MetadataCachePolicy.cpp:99`
- **Category**: Error Handling & Validation
- **Confidence**: 95/100
- **Finding**: `stampMetadata()` stores the attempted release ID for every outcome, including `Uncertain` after the release fails album/track validation and `Error` before validation completes. It also gives that state a new `checkedAt`. `preferredRelease()` chooses between credits and enrichment IDs by timestamp alone. Consequently, selecting an incorrect candidate can replace an older verified release ID and steer subsequent credits/enrichment requests repeatedly toward the rejected release, contrary to the contract’s requirement to reuse the newest **verified** ID.
- **Trace**: `MetadataWorker::Private::release()` records the attempted ID at line 242; `receiveRelease()` lines 376–379 finishes even when `musicBrainzMetadata()` returns `Uncertain`; `finish()` lines 493–494 passes that ID to `stampMetadata()`. `MusicBrainzSource.cpp:150–152` confirms that failed identity matching produces `Uncertain` before publishing a validated release payload. `mergeMetadata()` retains the stamped state, and `preferredRelease()` lines 29–46 subsequently selects its ID without checking validation. Existing `releaseChoiceAndGenreEnrichment` coverage checks successful candidate selection, not rejected or failed candidates.
- **Mitigation**: Separate attempted lookup identity from verified release identity. Update the reusable release ID only after identity and complete track validation succeeds, preserving any previous verified ID on uncertain or failed attempts. Include a regression where a rejected candidate follows a successful release and the next query still uses the verified release.

#### [D-019] Wikipedia fallback accepts an artist name embedded in another name
- **File**: `src/services/metadata/WikipediaSource.cpp:158`
- **Category**: Error Handling & Validation
- **Confidence**: 97/100
- **Finding**: The text fallback removes whitespace and punctuation, then accepts any substring containing the normalized artist. It can therefore match another artist’s album. For example, a local album titled `Moon`, artist `Air`, year `1997`, accepts a same-title page whose introduction is `Moon is an album by Chair, released in 1997.` With no structured performer/date claims, every check passes because `chair` contains `air`, and the result becomes `Matched`. The accepted introduction and source can replace the local album’s description.
- **Trace**: Followed `metadataName`, `wikipediaMetadata` title/type/year/performer checks, `MetadataWorker::Private::receivePages` and `assess`, and `MetadataMerge.cpp:75–79`. Direct-title lookup can supply this page before the artist-qualified search. Existing `wikipediaChecksIdentityAndAttribution` tests allow fallback without entities but do not cover artist substring collisions.
- **Mitigation**: Preserve appropriate name boundaries and require affirmative performer attribution when using text evidence; return `Uncertain` when that evidence is insufficient. Add a regression for a short artist name embedded in another performer’s name.

#### [D-020] Theme render failures do not fail the settings probe
- **File**: `tests/render/LibraryChecks.cpp:268`
- **Category**: Error Handling & Validation
- **Confidence**: 93/100
- **Finding**: Both theme-transition calls to `renderSettingsTarget()` discard their results (lines 268 and 271). That helper returns false when rendering or receipt of matching submission telemetry times out. The subsequent `theme-keeps-resources` check compares cached `lastSettingsSample` against the baseline, so it can pass unchanged after a failed theme render. A failure confined to the night transition can therefore escape the report when rendering resumes after restoring day mode.
- **Trace**: Followed `verifySettingsInputs()` → `renderSettingsFrame()` → `collectSettingsSample()`. The helper checks actual rendering and matching input revision/target size, but neither transition contributes its result to `passed`. The resource assertion does not establish that either transition produced a fresh sample.
- **Mitigation**: Record both transition results through `check()` and require a successful current submission before comparing resource telemetry.

#### [D-021] Incomplete shell fixture can pass the source-state gate
- **File**: `tests/render/RhiShellChecks.cpp:145`
- **Category**: Error Handling & Validation
- **Confidence**: 92/100
- **Finding**: Only the number of cases is validated. Replacing one ordinary case, such as sourceIndex 4, with a duplicate of sourceIndex 5 preserves the 61-row count and final six sorting/two isolation counts. The omitted source state is never checked, the duplicate output overwrites the same PNG, and the report can still publish `passed=true`. Missing `projections` arrays likewise skip projection validation entirely while leaving `maxProjectionPixels` at zero.
- **Trace**: Followed `readFixture()` in `RhiReviewInput.h:19–25`, the case loop at `RhiShellChecks.cpp:151–195`, and final counts/report at `344–365`. `readFixture()` checks only that JSON is an object. No distinct/expected sourceIndex set or required projection evidence is checked. `docs/contracts/rhi-shell.md:24` explicitly defines the 61 source states and projection gate.
- **Mitigation**: Validate the expected distinct sourceIndex set and required projection sample structure before rendering. Reject missing or empty projection evidence and duplicate output identities.

#### [D-022] Startup probe can overwrite an earlier run’s evidence
- **File**: `tests/render/StartupChecks.cpp:807`
- **Category**: Error Handling & Validation
- **Confidence**: 96/100
- **Finding**: `reviewStartup()` accepts an existing output directory, ignores `mkpath()` failure, and subsequently replaces `product-restored.png` and `report.json`. Repeating the documented direct `ui-tests --startup` command with the same path can overwrite a failed run with a successful report, contrary to the evidence preservation contract.
- **Trace**: `UiTests.cpp:93–107` directly forwards the supplied path; `StartupChecks.cpp:1209` saves the fixed screenshot name and line 1380 calls `writeJsonFile()`, whose `QSaveFile::commit()` replaces an existing report. `verify-startup.py:54` protects only callers using that wrapper. Sibling `reviewGui()` and `reviewWindowLifecycle()` reject existing output paths.
- **Mitigation**: Reject existing output paths and failed directory creation before fixture generation or rendering; retain the existing report and image unchanged.

#### [D-023] Cancellation regression never establishes the late-completion boundary
- **File**: `tests/unit/DirectoryScannerTest.cpp:381`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 97/100
- **Finding**: The comment promises cancellation after worker completion but before queued delivery, yet `setRoots()` is immediately followed by `cancel()`. The worker can observe cancellation before scanning and return no snapshot, so all assertions pass without exercising rejection of an already completed result. A regression limited to late completion acceptance can therefore escape this test.
- **Trace**: `LibraryStore::setRoots()` → `rescan()` → `Private::startScan()` launches QtConcurrent work. `cancel()` sets its atomic token immediately. `DirectoryScanner::scan()` checks that token before constructing a snapshot and repeatedly during traversal. Nothing in the test establishes worker completion before cancellation.
- **Mitigation**: Use a controlled completion barrier or existing test seam to establish that a successful worker result exists while GUI completion remains undelivered, then cancel and verify publication and persistence remain unchanged.

#### [D-024] Transfer test silently uses a different graphics backend from the requested one
- **File**: `tests/unit/DisplayOutputTest.cpp:146`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 96/100
- **Finding**: `main()` accepts and probes `QSG_RHI_BACKEND`, but `transferShader()` creates Metal on macOS, D3D11 on Windows, and Vulkan on Linux unconditionally. For example, a successful Windows run requested as D3D12 still tests the production transfer shader on D3D11. Linux OpenGL requests likewise cannot exercise OpenGL and can fail solely because Vulkan development support is absent. This prevents this entry from providing reliable evidence for the selected target backend.
- **Trace**: `main():433–437` calls `selectGraphicsBackend()` and `probeOptionalGraphicsBackend()`. `transferShader():146–160` then chooses QRhi directly from platform macros without consulting the selected API. CTest registers this function as `display-transfer-gpu`; `test-gates.md` assigns it production shader and target backend coverage.
- **Mitigation**: Construct QRhi using the selected graphics API and its required initialization resources, and verify/report the actual API. Explicitly reject any unsupported test backend instead of running another one.

#### [D-025] Failed assertions can leave a provider callback referencing expired locals
- **File**: `tests/unit/LibraryDataTest.cpp:1389`
- **Category**: Ownership & Lifecycle
- **Confidence**: 90/100
- **Finding**: `provider.onCancel` captures `replaced` and other locals by reference, but cleanup happens only at line 1401 after several assertions. If an assertion returns early while a request remains pending, `replaced` reaches the end of its lifetime before `session` is destroyed. Session shutdown cancels pending provider requests and invokes the still-installed callback, which reads that expired capture. A failing regression can consequently invoke undefined behavior during teardown.
- **Trace**: Declaration order is provider, session, then `replaced`. `TestProvider::cancel():51–55` invokes `onCancel`. `LibrarySession::~LibrarySession()` calls `MetadataCoordinator::shutdown()`, whose `Private::invalidate():358–370` cancels every pending token. The explicit callback reset is bypassed by failed `QVERIFY`/`QCOMPARE`/`QTRY_COMPARE` macros.
- **Mitigation**: Install unconditional scope-bound callback cleanup that runs before captured locals expire and before session destruction, or restructure ownership/declaration order so every callback dependency outlives shutdown.

#### [D-026] Cooldown rows skip the untrusted-host assertion
- **File**: `tests/unit/OnlineMetadataTest.cpp:913`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 96/100
- **Finding**: For the `rate-limit` and `maxlag` rows, `responses.size()` is already 2 after the cooldown check. The subsequent `QTRY_VERIFY(responses.size() >= 2)` therefore succeeds immediately, before the untrusted-host request executes. `server->count() == 1` also passes using the preceding state. These rows finish without exercising or validating rejection of the unsupported address.
- **Trace**: Followed `httpFailureBoundaries()` lines 907–915 into `MetadataHttp::get()` (`src/services/metadata/MetadataHttp.cpp:103`), which schedules processing with a zero-delay timer. `MetadataHttp::~MetadataHttp()` cancels that timer. Host validation occurs later in `Private::start()` at lines 118–121.
- **Mitigation**: Record the response count before submitting the unsupported URL, wait for exactly one additional response, and assert that the new response contains the expected rejection while the network request count stays unchanged.

#### [D-027] Maximum-error reductions silently discard NaN results
- **File**: `tests/unit/OverviewTest.cpp:167`
- **Category**: API, Naming & C++ Correctness
- **Confidence**: 100/100
- **Finding**: The maximum-error reductions start with a finite accumulator and feed unchecked `std::abs()`/vector-length results into `std::max`. A NaN comparison does not replace the accumulator, so nonfinite production results can leave `maximum == 0` and pass the tolerance assertion. The same defect occurs in camera interpolation at lines 213–217 and projection coordinates at lines 293–297.
- **Trace**: Followed all three reducers and their final assertions; `SceneVector::length()` propagates NaN. Compared `RealColumnsTest.cpp:151–152`, which uses `tests/support/NumericChecks.h::errorMagnitude()` specifically to reject nonfinite differences before reduction.
- **Mitigation**: Reject nonfinite values before every reduction, using the existing numeric-check helper where suitable; verify the regression assertion fails when a compared component is NaN.

#### [D-028] Failed temporary-directory creation redirects persistence tests into the working directory
- **File**: `tests/unit/QmlBehaviorTest.cpp:170`
- **Category**: Error Handling & Validation
- **Confidence**: 95/100
- **Finding**: The test passes `directory.path()` to `LibraryStore` without checking `directory.isValid()`. If temporary-directory creation fails, its empty path makes `QDir(dataRoot).filePath("library-index.json")` target the working directory, where the test loads and saves metadata configuration. The settings tests repeat this at lines 250–251 and 320–329, allowing their updates to read/write a working-directory `preferences.json`.
- **Trace**: Followed `LibraryStore` construction at `src/library/LibraryStore.cpp:89–94` and metadata persistence at lines 230–236; followed `SettingsFixture` into `PreferencesStore` construction at `src/services/PreferencesStore.cpp:63–68` and shutdown persistence at lines 89–97.
- **Mitigation**: Assert temporary-directory validity before constructing any store or fixture so setup failure stops the test before filesystem access.

#### [D-029] Publication follows symlinked destination directories outside the repository
- **File**: `tools/assets/AssetTransaction.h:52`
- **Category**: Error Handling & Validation
- **Confidence**: 88/100
- **Finding**: Publication rejects a symlink at the final filename but follows symlinks in parent directories. For example, if `resources/audio` points outside the repository, publishing `resources/audio/atmosphere.ogg` moves and replaces the external file while the leaf passes `isSymLink()`. This bypasses the intended root-relative output boundary and the assumption that staging and destination share a filesystem.
- **Trace**: Followed `stage()` path validation, `publish()` destination checks and rollback, `GeometryAudit::saveFile()`, and `PrepareAssets.cpp` construction/staging/publication. The CLI accepts its repository root directly; neither caller nor transaction validates destination ancestors or canonical containment.
- **Mitigation**: Validate destination parent components before publication, reject symlinked ancestors, and verify that resolved destinations remain beneath the canonical repository root. Cover an external directory symlink with a regression that proves external files remain untouched.

#### [D-030] Validate title capture dimensions before integer arithmetic
- **File**: `tools/migration/TitleSourceProbe.cpp:571`
- **Category**: Error Handling & Validation
- **Confidence**: 100/100
- **Finding**: The external reference report receives only a protocol and nonempty-array check before its capture dimensions reach `toInt() + 2`. A matching-DPR capture with `width` or `budget` equal to `2147483647` passes the input checks and causes signed integer overflow here. Fractional, negative and incorrectly typed dimensions are also silently converted rather than rejected.
- **Trace**: `SourceUiProbe::main` passes the supplied source directory directly to `probeTitleSource`; lines 666–671 check only protocol and nonempty arrays; lines 735–737 filter only DPR; `finishTitleProbe` consumes unchecked dimensions at line 571. `verify-title.mjs` checks capture count/platform/protocol but does not validate dimensions.
- **Mitigation**: Validate the complete title report before creating rendering resources. Require finite positive dimensions with explicit practical limits, validate their integer representation, and check padding and DPR-scaled arithmetic before conversion or allocation.

#### [D-031] Accepted ruler frame counts can prevent the deadline from running
- **File**: `tools/migration/ProbeInput.h:95`
- **Category**: Error Handling & Validation
- **Confidence**: 98/100
- **Finding**: Ruler input validation accepts frame counts through `INT_MAX`, and permits an empty trace for such an operation. `runRulerAssertions` then executes every requested frame synchronously on the GUI thread. A small JSON file can therefore request billions of iterations, blocking event processing and preventing the probe’s deadline timer from enforcing its advertised execution bound.
- **Trace**: `integer()` allows values up to `INT_MAX`; `validateRuler` accepts empty traces at lines 95–100; `RulerSourceProbe.cpp:517–525` runs the entire frame count without yielding or checking elapsed time; the deadline at lines 696–698 is a GUI-thread `QTimer`.
- **Mitigation**: Bound per-operation and aggregate frame counts during validation. If long traces are intentionally supported, process bounded chunks with an elapsed-time or cancellation check so the deadline remains effective.

#### [D-032] Branding manifest inventory validation accepts duplicate entries
- **File**: `tools/assets/ExportBranding.cpp:800`
- **Category**: Error Handling & Validation
- **Confidence**: 95/100
- **Finding**: Verify-only mode treats an array size of 36 as proof of a complete manifest, then independently validates whichever paths it contains. A manifest containing 36 copies of one valid entry passes these checks while omitting the other 35 assets, so the reported success no longer proves the required complete bytes/SHA-256 inventory.
- **Trace**: Export mode enumerates both asset trees at lines 815–820. Verify-only mode checks only array size/display name at lines 799–801, and validates each supplied entry at lines 802–810; it never rejects duplicate paths or compares the path set against the expected inventory. Independent raster comparisons do not restore the missing per-file manifest hash checks.
- **Mitigation**: Validate unique, normalized asset paths and compare the resulting set against the complete expected inventory before checking individual sizes and hashes.

---

### Investigation targets (human verification needed)

#### [I-001] 搜索模型在 about-to 结构通知期间缺少重入保护
- **File**: `src/core/SearchListModel.cpp:265`
- **Category**: Model Contracts (QAbstractItemModel)
- **Confidence**: 76/100
- **Finding**: `replaceResults()`只在 `endRemoveRows()/endInsertRows()`之后检查 generation。`beginRemoveRows()/beginInsertRows()`本身会同步发出 about-to 信号；若其观察者调用公开 `update()` 换库或重新更新结果，外层仍会按照旧的 `prefix/removed/inserted` 修改已经替换的 `m_results`，甚至向变短的列表执行旧范围删除，并形成嵌套模型事务。当前对普通 `dataChanged/rowsRemoved/rowsInserted` 的保护未覆盖这个入口。
- **Unverified because**: 已确认代码上的重入窗口，但未找到当前生产连接明确从 `rowsAboutToBeRemoved/rowsAboutToBeInserted` 调用该模型的更新入口。现有回归 `LibraryDataTest.cpp:719–723` 仅连接完成通知。
- **How to verify**: 在模型协议回归中连接两个 about-to 信号，同步调用 `update()` 替换为空或不同大小的快照，配合 `QAbstractItemModelTester` 检查删除范围及事务完整性；若需要支持该重入，应在整个结构事务期间延期更新，并在事务结束后应用最后一个意图。

#### [I-002] Invalid-media processing can overwrite a replacement request after notification
- **File**: `src/media/PlaybackController.cpp:330`
- **Category**: Performance & Code Quality
- **Confidence**: 76/100
- **Finding**: `status(InvalidMedia, revision)` calls `setSupport()`, which synchronously emits `supportChanged`. If an observer starts another track from that notification, processing resumes without rechecking the revision and writes an error and Paused intent into the replacement request. Similar notification boundaries exist in `load()`.
- **Unverified because**: No current production receiver of `supportChanged` was found; this requires a subscriber that reenters playback commands.
- **How to verify**: Connect a direct `supportChanged(Unsupported)` handler that starts another valid track, then deliver InvalidMedia for the old revision. Check that the replacement retains its latest intent and error state.

#### [I-003] flush 的同步回调可能清掉更新的待提交目标
- **File**: `src/render/navigation/InputRouter.cpp:209`
- **Category**: Performance & Code Quality
- **Confidence**: 75/100
- **Finding**: `flush()` 在 `selectCell()` 返回成功后无条件清空 m_pending。selectCell() 同步发出 selectionChanged；若其回调提交新的导航，并因归位副本容量不足而留下新目标，外层 flush 会把新目标一并清掉。m_committing 没有识别待提交目标的代次。
- **Unverified because**: 当前检查到的生产接线未发现从 selectionChanged 同步发起新导航的调用者，因此尚不能认定正常产品路径会触发。
- **How to verify**: 检查所有同步选择通知处理器是否可能调用 router.row/lane/selectRow；以归位容量即将耗尽的状态连接一次性重入处理器，确认内层产生的新待提交目标是否在外层返回后保留。

---

### Summary

| Category | Lint | Deep | Investigate | Total |
| --- | ---: | ---: | ---: | ---: |
| 原始 lint 规则诊断（未按深度领域重分类） | 878 | 0 | 0 | 878 |
| API, Naming & C++ Correctness | 0 | 9 | 0 | 9 |
| Error Handling & Validation | 0 | 15 | 0 | 15 |
| Model Contracts | 0 | 0 | 1 | 1 |
| Ownership & Lifecycle | 0 | 2 | 0 | 2 |
| Performance & Code Quality | 0 | 6 | 2 | 8 |
| **Total** | **878** | **32** | **3** | **913** |

Findings below confidence 60 are suppressed. 诊断、确认事项和调查项的计数不可解释为同等严重性的缺陷总数。

## QML Code Review Report

**Scope**: files: 全部跟踪的 QML，详见分片附录。

**Files reviewed**: 93（8,329 行）

**Issues found**: 1528（1525 from lint，3 from deep analysis；lint 是原始诊断计数）

**Agents**: 6（独立分片，逐文件编号读取并恢复截断内容）

**qmllint**: ran（Qt 6.12.0）；项目 `Yalahoo_qmllint` / `ui-tests_qmllint` 均退出 0。

---

### Lint findings

| Rule | Count | Locations | Mitigation |
| --- | ---: | --- | --- |
| BND-1 property var -- use a typed property (int, string, etc.) for qmlsc compilation and type safety | 25 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:14,75`; `qml/Yalahoo/Music/components/ArchiveEditMenu.qml:10,64`; `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:7`; `qml/Yalahoo/Music/components/BackdropBlurPass.qml:14`; `qml/Yalahoo/Music/components/BrowseInfo.qml:13`; `qml/Yalahoo/Music/components/CreditCard.qml:10`; `qml/Yalahoo/Music/components/DetailAlbumInfo.qml:10,153`; `qml/Yalahoo/Music/components/DetailHeader.qml:10`; `qml/Yalahoo/Music/components/DetailIntroduction.qml:9`; `qml/Yalahoo/Music/components/DetailTabsBackdrop.qml:24,31`; `qml/Yalahoo/Music/components/DisplaySurface.qml:13`; `qml/Yalahoo/Music/pages/Browse.qml:19`; `qml/Yalahoo/Music/panels/LibraryPanel.qml:142`; `qml/Yalahoo/Music/panels/LightingModes.qml:38`; `qml/Yalahoo/Music/panels/LightingPanel.qml:112`; `qml/Yalahoo/Music/panels/SearchFilters.qml:62,71`; `qml/Yalahoo/Music/panels/SettingChoice.qml:10,11`; `qml/Yalahoo/Music/panels/SoundSettings.qml:10,22` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-2 Imperative '=' on 'activeTab' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/pages/Detail.qml:37` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-2 Imperative '=' on 'editMenu.x' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 2 | `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:37,45` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-2 Imperative '=' on 'editMenu.y' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 2 | `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml:38,46` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-2 Imperative '=' on 'root.activeTab' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/pages/Detail.qml:78` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-2 Imperative '=' on 'roots.text' destroys its binding -- use Qt.binding() to restore, or verify this is intentional | 1 | `qml/Yalahoo/Music/panels/LibraryPanel.qml:18` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| BND-5 list<> property has no granular change signals -- add/move/remove won't notify; consider a ListModel or notify manually | 1 | `qml/Yalahoo/Music/components/DesignTokens.qml:14` | 采用适当的 typed 属性并检查依赖；命令式赋值应有明确意图和重新绑定/通知协议。 |
| IMG-1 Image without sourceSize -- decodes full resolution into GPU memory; set sourceSize to display dimensions | 1 | `tests/render/OverviewVisual.qml:19` | 核对 sourceSize、实际源图片大小和图像生命周期，按显示需要设置解码尺寸。 |
| IMP-4 Non-Qt import before Qt import -- order: Qt modules, third-party, local C++, QML folders | 1 | `qml/Yalahoo/Music/components/ArchiveButton.qml:4` | 核对导入分组及构建目标提供的模块闭包。 |
| ORD-1 attached property appears after child object | 4 | `qml/Yalahoo/Music/components/Header.qml:72`; `qml/Yalahoo/Music/components/TrackRow.qml:159`; `qml/Yalahoo/Music/panels/LibraryPanels.qml:128`; `qml/Yalahoo/Music/panels/MetadataSection.qml:76` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 attached property appears after signal handler | 5 | `qml/Yalahoo/Music/components/CreditCard.qml:54`; `qml/Yalahoo/Music/components/PlaybackControls.qml:64`; `qml/Yalahoo/Music/panels/LightingPanel.qml:153`; `qml/Yalahoo/Music/panels/SettingChoice.qml:39`; `qml/Yalahoo/Music/panels/SettingRange.qml:62` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 id appears after child object | 2 | `qml/Yalahoo/Music/components/DetailTabs.qml:25`; `qml/Yalahoo/Music/scene/CaseMaterials.qml:16` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 id appears after property assignment | 11 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:74`; `qml/Yalahoo/Music/components/ArchiveTextArea.qml:29`; `qml/Yalahoo/Music/components/ArchiveTextField.qml:28`; `qml/Yalahoo/Music/components/OrdinalReel.qml:57`; `qml/Yalahoo/Music/components/Ruler.qml:30`; `qml/Yalahoo/Music/components/ThemeSwitch.qml:35`; `qml/Yalahoo/Music/components/TitleReel.qml:70,107`; `qml/Yalahoo/Music/panels/LightingModes.qml:37`; `qml/Yalahoo/Music/panels/SearchFilters.qml:61,68` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property assignment appears after attached property | 5 | `qml/Yalahoo/Music/components/ColumnNavigation.qml:86`; `qml/Yalahoo/Music/components/NavigationFrame.qml:144,157`; `qml/Yalahoo/Music/components/TitleReel.qml:125`; `qml/Yalahoo/Music/panels/SearchFilters.qml:45` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property assignment appears after child object | 7 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:25`; `qml/Yalahoo/Music/components/DesignTokens.qml:56`; `qml/Yalahoo/Music/components/Header.qml:76`; `qml/Yalahoo/Music/components/OrdinalReel.qml:22`; `qml/Yalahoo/Music/pages/Overview.qml:55`; `qml/Yalahoo/Music/scene/CaseMaterials.qml:40`; `qml/Yalahoo/Music/scene/GlassMaterial.qml:17` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property assignment appears after signal handler | 6 | `qml/Yalahoo/Music/components/TrackList.qml:117`; `qml/Yalahoo/Music/pages/MusicStage.qml:70`; `qml/Yalahoo/Music/panels/MetadataSettings.qml:39,55,75,121` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property declaration appears after child object | 2 | `qml/Yalahoo/Music/panels/SoundSettings.qml:10,22` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property declaration appears after property assignment | 37 | `qml/Yalahoo/Music/components/ArchiveEditMenu.qml:9,64`; `qml/Yalahoo/Music/components/ArchiveSlider.qml:11`; `qml/Yalahoo/Music/components/ArchiveTextArea.qml:10`; `qml/Yalahoo/Music/components/ArchiveTextField.qml:10`; `qml/Yalahoo/Music/components/BackdropBlurPass.qml:14`; `qml/Yalahoo/Music/components/CreditCard.qml:17,76`; `qml/Yalahoo/Music/components/DetailAlbumInfo.qml:153`; `qml/Yalahoo/Music/components/DetailTabsBackdrop.qml:24,31`; `qml/Yalahoo/Music/components/DisplaySurface.qml:13`; `qml/Yalahoo/Music/components/OpeningBackdrop.qml:16,27`; `qml/Yalahoo/Music/components/OpeningFilm.qml:15`; `qml/Yalahoo/Music/components/OrdinalReel.qml:50`; `qml/Yalahoo/Music/components/TrackList.qml:17,183,190`; `qml/Yalahoo/Music/components/TrackRow.qml:22`; `qml/Yalahoo/Music/components/TransportTitle.qml:10`; `qml/Yalahoo/Music/panels/ExperimentPanels.qml:10`; `qml/Yalahoo/Music/panels/LibraryPanel.qml:142`; `qml/Yalahoo/Music/panels/LibraryPanels.qml:47,85`; `qml/Yalahoo/Music/panels/LightingPanel.qml:9,112`; `qml/Yalahoo/Music/panels/MetadataSettings.qml:9,83,100`; `qml/Yalahoo/Music/panels/MotionPanel.qml:9`; `qml/Yalahoo/Music/panels/PlaybackPanel.qml:9`; `qml/Yalahoo/Music/panels/SearchResult.qml:19`; `qml/Yalahoo/Music/panels/SettingsPanel.qml:12`; `qml/Yalahoo/Music/panels/ThemeCard.qml:11`; `qml/Yalahoo/Music/scene/CaseMaterials.qml:78`; `qml/Yalahoo/Music/scene/GeometryScene.qml:43` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 property declaration appears after signal declaration | 5 | `qml/Yalahoo/Music/components/OverviewLabel.qml:19`; `qml/Yalahoo/Music/panels/LibraryPanels.qml:23`; `qml/Yalahoo/Music/panels/PanelContents.qml:15`; `qml/Yalahoo/Music/panels/PanelHeading.qml:13`; `qml/Yalahoo/Music/panels/SearchFilters.qml:21` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 signal declaration appears after property assignment | 1 | `qml/Yalahoo/Music/pages/Detail.qml:68` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| ORD-1 signal handler appears after child object | 5 | `qml/Yalahoo/Music/components/DetailTabs.qml:127`; `qml/Yalahoo/Music/components/OverviewLabel.qml:135`; `qml/Yalahoo/Music/components/TrackList.qml:126`; `qml/Yalahoo/Music/panels/LibraryPanel.qml:179`; `qml/Yalahoo/Music/panels/SearchPanel.qml:125` | 按 QML 声明约定检查属性/信号/对象顺序；遵守项目保留类型解析和绑定语义的要求。 |
| PRF-1 Rectangle with 'transparent' color -- use Item instead, or toggle visible on the Rectangle (creates geometry node even when transparent) | 18 | `qml/Yalahoo/Music/components/ArchiveButton.qml:48`; `qml/Yalahoo/Music/components/ArchiveCheckBox.qml:21,31`; `qml/Yalahoo/Music/components/ArchiveComboBox.qml:91`; `qml/Yalahoo/Music/components/ArchiveSlider.qml:44`; `qml/Yalahoo/Music/components/DetailAlbumInfo.qml:122`; `qml/Yalahoo/Music/components/OverviewLabel.qml:102,157`; `qml/Yalahoo/Music/components/Ruler.qml:48`; `qml/Yalahoo/Music/components/ThemeSwitch.qml:12,52,73`; `qml/Yalahoo/Music/components/TransportTitle.qml:42`; `qml/Yalahoo/Music/pages/Opening.qml:104`; `qml/Yalahoo/Music/pages/Overview.qml:136,158`; `qml/Yalahoo/Music/panels/SearchFilters.qml:84`; `qml/Yalahoo/Music/panels/ThemeCard.qml:22` | 结合真实条目数量、创建频率、缓存与可见生命周期判断成本，不把规则命中当作已测性能退化。 |
| PRF-1 Rectangle with 'transparent' in color expression -- use Item instead, or toggle visible (creates geometry node even when transparent) | 3 | `qml/Yalahoo/Music/components/ArchiveButton.qml:33`; `qml/Yalahoo/Music/panels/LightingModes.qml:52`; `qml/Yalahoo/Music/panels/SearchResult.qml:32` | 结合真实条目数量、创建频率、缓存与可见生命周期判断成本，不把规则命中当作已测性能退化。 |
| PRF-3 clip: true disables scene graph batching -- verify this is needed (acceptable on ListView) | 20 | `qml/Yalahoo/Music/components/ArchiveComboBox.qml:123`; `qml/Yalahoo/Music/components/ArchiveScrollView.qml:13`; `qml/Yalahoo/Music/components/ArchiveTextArea.qml:17`; `qml/Yalahoo/Music/components/ArchiveTextField.qml:17`; `qml/Yalahoo/Music/components/BackdropBlur.qml:21`; `qml/Yalahoo/Music/components/BrowseInfo.qml:42`; `qml/Yalahoo/Music/components/CreditCard.qml:70`; `qml/Yalahoo/Music/components/OpeningFilm.qml:77`; `qml/Yalahoo/Music/components/OrdinalReel.qml:62`; `qml/Yalahoo/Music/components/OverviewLabel.qml:142`; `qml/Yalahoo/Music/components/Ruler.qml:13,25`; `qml/Yalahoo/Music/components/TitleReel.qml:24,115`; `qml/Yalahoo/Music/components/TrackList.qml:16,128`; `qml/Yalahoo/Music/components/TransportTitle.qml:50`; `qml/Yalahoo/Music/panels/ExperimentPanels.qml:21`; `qml/Yalahoo/Music/panels/SearchFilters.qml:55`; `qml/Yalahoo/Music/panels/SearchPanel.qml:49` | 结合真实条目数量、创建频率、缓存与可见生命周期判断成本，不把规则命中当作已测性能退化。 |
| STY-1 Top-level component should have id: root (enables qualified lookup, future-proofs for QML 3) | 26 | `qml/Yalahoo/Music/App.qml:11`; `qml/Yalahoo/Music/components/ArchiveButton.qml:7`; `qml/Yalahoo/Music/components/ArchiveCheckBox.qml:7`; `qml/Yalahoo/Music/components/ArchiveComboBox.qml:10`; `qml/Yalahoo/Music/components/ArchiveEditMenu.qml:6`; `qml/Yalahoo/Music/components/ArchiveProgressBar.qml:6`; `qml/Yalahoo/Music/components/ArchiveScrollBar.qml:6`; `qml/Yalahoo/Music/components/ArchiveScrollView.qml:6`; `qml/Yalahoo/Music/components/ArchiveSlider.qml:6`; `qml/Yalahoo/Music/components/ArchiveTextArea.qml:6`; `qml/Yalahoo/Music/components/ArchiveTextField.qml:6`; `qml/Yalahoo/Music/components/ArchiveToolTip.qml:6`; `qml/Yalahoo/Music/components/ColumnNavigation.qml:6`; `qml/Yalahoo/Music/components/CreditCard.qml:7`; `qml/Yalahoo/Music/components/DetailTabs.qml:6`; `qml/Yalahoo/Music/components/HeaderTransport.qml:6`; `qml/Yalahoo/Music/components/OverviewLabel.qml:7`; `qml/Yalahoo/Music/components/TrackRow.qml:6`; `qml/Yalahoo/Music/pages/MusicStage.qml:11`; `qml/Yalahoo/Music/panels/SearchFilters.qml:8`; `qml/Yalahoo/Music/panels/SearchResult.qml:8`; `qml/Yalahoo/Music/scene/CaseMaterials.qml:6`; `qml/Yalahoo/Music/scene/GeometryScene.qml:9`; `qml/Yalahoo/Music/scene/GlassMaterial.qml:8`; `qml/Yalahoo/Music/scene/LightingMaterial.qml:5`; `tests/render/LibraryReview.qml:6` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 3 anchors.* properties using dot notation -- use group notation: anchors { ... } | 1 | `qml/Yalahoo/Music/panels/StagePanels.qml:114` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 3 font.* properties using dot notation -- use group notation: font { ... } | 4 | `qml/Yalahoo/Music/components/BrowseInfo.qml:163`; `qml/Yalahoo/Music/components/DetailAlbumInfo.qml:75,89`; `qml/Yalahoo/Music/components/NavigationFrame.qml:101` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 3 layer.* properties using dot notation -- use group notation: layer { ... } | 2 | `qml/Yalahoo/Music/components/DisplaySurface.qml:9`; `qml/Yalahoo/Music/components/TitleReel.qml:79` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 4 font.* properties using dot notation -- use group notation: font { ... } | 13 | `qml/Yalahoo/Music/components/ArchiveButton.qml:17`; `qml/Yalahoo/Music/components/ArchiveComboBox.qml:20`; `qml/Yalahoo/Music/components/NavigationFrame.qml:49,56,77`; `qml/Yalahoo/Music/components/OrdinalReel.qml:26`; `qml/Yalahoo/Music/pages/Opening.qml:69`; `qml/Yalahoo/Music/pages/Overview.qml:29,36,43,50`; `qml/Yalahoo/Music/panels/SearchFilters.qml:76`; `qml/Yalahoo/Music/panels/SearchPanel.qml:19` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 5 anchors.* properties using dot notation -- use group notation: anchors { ... } | 1 | `qml/Yalahoo/Music/panels/LibraryPanels.qml:108` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 5 font.* properties using dot notation -- use group notation: font { ... } | 1 | `qml/Yalahoo/Music/components/ArchiveText.qml:29` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-3 6 font.* properties using dot notation -- use group notation: font { ... } | 1 | `qml/Yalahoo/Music/components/ArchiveText.qml:62` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'frosted_Polymer_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:10` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'ivory_Edges_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:24` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'music_Case_V040__Frosted_Polymer' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:57` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'music_Case_V040__Ivory_Edges' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:65` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'music_Case_V040__Optical_Diffuser' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:73` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'optical_Diffuser_material' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:38` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |
| STY-6 id 'rhine_Music_Case_V040' -- use lowerCamelCase for QML ids | 1 | `resources/models/Music_case_v040.qml:54` | 核对 QML 自绘/格式契约并统一声明风格；属性表达与布局来源优先。 |


QML 上表为 222 条 Python 规则诊断；其余 1,303 条独立 qmllint 诊断在附录完整按规则/消息保留。它们包含缺导入/未解析类型的级联信息；项目 CMake target 的无诊断结果优先解释对应模块的类型事实，不能把独立扫描的级联数量当成生产类型错误数量。

---

### Deep analysis findings

#### [D-033] Slider rail has an unused id
- **File**: `qml/Yalahoo/Music/components/ArchiveSlider.qml:19`
- **Category**: Performance & Code Quality
- **Confidence**: 100/100
- **Finding**: The rail rectangle declares `id: rail`, but nothing references it. This is a minor maintainability cleanup with no demonstrated user-visible impact.
- **Trace**: Searched the complete component for `rail`; the declaration is its only occurrence. Its children reference their parent directly.
- **Mitigation**: Remove the unused id.

#### [D-034] Track rows have no accessible activation action
- **File**: `qml/Yalahoo/Music/components/TrackRow.qml:39`
- **Category**: Logic & Wiring
- **Confidence**: 94/100
- **Finding**: Playable tracks expose a plain `Item` as `Accessible.ListItem`, but playback is connected only to keyboard handlers and `TapHandler`. No accessible press action invokes `trackRequested`, so assistive technology cannot activate the row through its action interface. The nested Rectangle also exposes the same list-item name at lines 159–160, creating a duplicate accessible entry.
- **Trace**: Read all TrackRow activation paths (lines 35–38 and 144–157), its TrackList delegate wiring, and MusicStage playback handler. Whole-QML search found no `Accessible.onPressAction` implementation. Templates button behavior cannot supply an action because both exposed objects are plain Item/Rectangle objects.
- **Mitigation**: Expose a single accessible actionable row, preferably using an explicitly drawn Templates delegate for behavior, and connect its accessible activation to the same guarded playback intent. Suppress the decorative inner node from accessibility.

#### [D-035] Genre filter buttons have no visible keyboard focus
- **File**: `qml/Yalahoo/Music/panels/SearchFilters.qml:69`
- **Category**: A7. Logic & Wiring
- **Confidence**: 98/100
- **Finding**: Genre chips accept keyboard focus through `Qt.StrongFocus`, but their complete custom visual tree never reads `visualFocus` or `activeFocus`. Tabbing from search into an unselected genre therefore gives no visible indication of the focused control; the selected chip remains visually emphasized even when another chip owns keyboard focus.
- **Trace**: Read the entire `T.Button` delegate at lines 67–102. Background and text appearance depend only on `checked`; Templates supplies no default focus drawing. Compared the project’s `ArchiveButton.qml:41–52`, which explicitly draws the required focus outline from `visualFocus`, and `controls.md`/QML-26 accessibility requirements. This is independent of the known transparent-rectangle lint diagnostic.
- **Mitigation**: Draw an explicit keyboard-focus indicator bound to `visualFocus`, retaining selection appearance separately and ensuring the genre viewport does not clip the indicator.

---

### Investigation targets (human verification needed)

无。

---

### Summary

| Category | Lint | Deep | Investigate | Total |
| --- | ---: | ---: | ---: | ---: |
| 原始 lint 规则诊断（未按深度领域重分类） | 1525 | 0 | 0 | 1525 |
| Logic & Wiring | 0 | 2 | 0 | 2 |
| Performance & Code Quality | 0 | 1 | 0 | 1 |
| **Total** | **1525** | **3** | **0** | **1528** |

Findings below confidence 60 are suppressed. 诊断、确认事项和调查项的计数不可解释为同等严重性的缺陷总数。

TrackRow 无障碍事项的主审补证：[Qt 6.12 Accessible 文档](https://doc.qt.io/qt-6/qml-qtquick-accessible.html)要求自定义交互项提供与点击等效的可访问动作；[Qt Quick 动作实现](https://github.com/qt/qtdeclarative/blob/dev/src/quick/accessible/qaccessiblequickitem.cpp#L476)没有为普通 ListItem 自动映射 TapHandler/Keys 的播放意图。这是源码接线判定，未执行屏幕阅读器实物验收。

## 构建、部署与生成链补充审查

#### [S-001] Unix 打包器接受空的 Qt 许可目录
- **File**: `tools/deploy/package_support.py:68`
- **Category**: Error Handling & Validation
- **Confidence**: 100/100
- **Finding**: `copy_licenses()`只核对最近的 `Licenses` 是目录，随后无条件复制；目录为空或只有空子目录也成功。macOS/Linux 打包和最终闭包检查均不要求 Qt 许可文件非空，损坏或不完整的安装可生成并安装缺少 Qt/媒体署名的产品包。最近层级的空目录还会遮蔽祖先中完整的许可目录。
- **Trace**: `macos.py`/`linux.py` → `copy_licenses()` → `shutil.copytree()`；`inspect_bundle()`不检查许可内容，`inspect_package()`只检查顶层 licenses 存在，inventory 只校验实际文件一致性。使用系统临时目录中的匿名伪 Kit 和项目署名文件直接调用该函数：空 Qt Licenses 被接受且复制后仍为空。`UnixDeploymentTest.py:122–142`覆盖成功/不存在，未覆盖空目录；Windows 的 `Copy-QtLicenses` 已明确拒绝空目录。
- **Mitigation**: 在修改 staging 前验证所选许可目录确有文件，缺失或空目录明确失败；按确定的 Qt 安装结构核对需要的媒体许可，补充空目录与只有子目录的定向回归。

#### [S-002] Windows 包验证器忽略清单外新增文件
- **File**: `tools/deploy/verify-windows.ps1:25`
- **Category**: Error Handling & Validation
- **Confidence**: 98/100
- **Finding**: 验证器只逐条检查 package.json 已列文件的 hash，从不枚举磁盘文件集合。保持已列文件不变、向包内新增任意文件（包括额外插件或研究组件），不会触发包内容变化错误；空 files 数组也会跳过整段校验。此行为无法证明其声称的包清单完整性，且明显弱于 macOS/Linux 的清单与实际 inventory 全等检查。
- **Trace**: `verify-windows.ps1:24–34` 读取并遍历声明的 files 后直接准备启动，没有检查 files 非空/类型、集合相等、重复路径或新增文件。`windows.ps1` 只在生成时拒绝研究组件；其后复制/验证路径不再执行这项检查。`package_support.verify_package_inventory()`通过全等比较拒绝同类差异。此结论为静态控制流确认，未冒称在 Windows/PowerShell 上执行。
- **Mitigation**: 先严格验证清单结构和唯一的相对路径，再对实际包文件集合（排除清单自身）与声明集合做精确比较，之后检查 hash/体积；扩展 Windows 工具回归覆盖新增、缺失、重复和空清单。

#### [S-003] 重新生成后处理 shader 会回退生产 SMAA 参数
- **File**: `tools/assets/prepare-postprocess-shaders.py:38`
- **Category**: Error Handling & Validation
- **Confidence**: 100/100
- **Finding**: 生成器硬编码颜色阈值 0.1、最多搜索 8 步，而已提交的生产 shader 和 postprocess.md 明确要求 0.05/32 步。正常重新生成资产会静默降低既定 SMAA 质量；后续 RHI 包装直接包含这些共享 shader，因此影响生产路径，不只是隔离 Review。
- **Trace**: `prepare-postprocess-shaders.py:37–41` → 写出 `resources/shaders/smaa-edges.frag`/`smaa-weights.frag`；`resources/shaders/rhi/smaa-*.frag`包含共享实现。`docs/contracts/postprocess.md:27`与 `tests/render/SmaaReference.h`使用既定生产参数。将生成器与两份锁定 Three 源文件复制到系统临时目录执行，退出 0；生成值为 0.1/8，与当前提交的 0.05/32 不同，未改动仓库资产。
- **Mitigation**: 使生成器保留当前生产 0.05/32 参数并同步清除过时的 Medium 描述；增加隔离生成后的语义一致性检查，避免只检查脚本语法和格式。


## 分片覆盖附录

每个分片均检查其适用的全部技能领域；“无新增”表示没有超过已列 lint、且达到置信度门槛的深度事项，不表示环境验收或不存在任何缺陷。C++ 的 A1–A6 覆盖模型、所有权、线程、API/语言、验证/错误和性能；QML 的 A1–A7 覆盖绑定、布局、加载、委托、状态、性能和逻辑接线。

### cpp-01 · 16 文件 / 2,240 行

深度 findings：1；调查：0。

- `src/app/ApplicationIdentity.h`
- `src/app/ApplicationOptions.cpp`
- `src/app/ApplicationOptions.h`
- `src/app/AssetCatalog.cpp`
- `src/app/AssetCatalog.h`
- `src/app/DeploymentProbe.cpp`
- `src/app/DeploymentProbe.h`
- `src/app/DetailReview.cpp`
- `src/app/DetailReview.h`
- `src/app/FixedTextureData.cpp`
- `src/app/FixedTextureData.h`
- `src/app/GraphicsBackend.h`
- `src/app/LibraryPersistence.cpp`
- `src/app/LibraryPersistence.h`
- `src/app/MaterialReview.cpp`
- `src/app/MaterialReview.h`


### cpp-02 · 10 文件 / 2,438 行

深度 findings：1；调查：0。

- `src/app/NavigationReview.cpp`
- `src/app/NavigationReview.h`
- `src/app/OverviewReview.cpp`
- `src/app/OverviewReview.h`
- `src/app/QmlTypes.h`
- `src/app/ReviewApplication.h`
- `src/app/ReviewSupport.h`
- `src/app/VerificationWindow.h`
- `src/app/VerificationWindowMac.mm`
- `src/app/main.cpp`


### cpp-03 · 27 文件 / 2,241 行

深度 findings：2；调查：1。

- `src/core/AlbumListModel.cpp`
- `src/core/AppIdentity.cpp`
- `src/core/AppIdentity.h`
- `src/core/ColorTransfer.h`
- `src/core/CoverPixels.h`
- `src/core/JsonFile.h`
- `src/core/LibraryIndex.cpp`
- `src/core/LibraryIndex.h`
- `src/core/LibraryModels.h`
- `src/core/LibraryOrder.cpp`
- `src/core/Localization.cpp`
- `src/core/Localization.h`
- `src/core/Logging.cpp`
- `src/core/Logging.h`
- `src/core/MetadataConfiguration.h`
- `src/core/MonotonicTime.h`
- `src/core/MusicTypes.cpp`
- `src/core/MusicTypes.h`
- `src/core/NumericValue.h`
- `src/core/ProductDefaults.h`
- `src/core/RenderQualityParameters.h`
- `src/core/Require.h`
- `src/core/SceneGeometry.h`
- `src/core/SearchListModel.cpp`
- `src/core/Units.h`
- `src/library/ArtworkStore.cpp`
- `src/library/ArtworkStore.h`


### cpp-04 · 15 文件 / 2,032 行

深度 findings：1；调查：0。

- `src/library/DirectoryScanner.cpp`
- `src/library/DirectoryScanner.h`
- `src/library/LegacyImporter.cpp`
- `src/library/LegacyImporter.h`
- `src/library/LibraryStore.cpp`
- `src/library/LibraryStore.h`
- `src/media/AudioOutputChanges.h`
- `src/media/AudioPreferencesBinding.cpp`
- `src/media/AudioPreferencesBinding.h`
- `src/media/BackgroundMusicController.cpp`
- `src/media/BackgroundMusicController.h`
- `src/media/BackgroundMusicController_p.h`
- `src/media/InterfaceSoundController.cpp`
- `src/media/InterfaceSoundController.h`
- `src/media/InterfaceSoundController_p.h`


### cpp-05 · 18 文件 / 2,462 行

深度 findings：1；调查：1。

- `src/media/MediaMetadataProbe.cpp`
- `src/media/MediaMetadataProbe.h`
- `src/media/PlaybackController.cpp`
- `src/media/PlaybackController.h`
- `src/media/PlaybackController_p.h`
- `src/media/PlaybackQueue.cpp`
- `src/media/PlaybackQueue.h`
- `src/motion/EasingCurve.h`
- `src/motion/FrameClock.cpp`
- `src/motion/FrameClock.h`
- `src/motion/FrameConnection.h`
- `src/motion/MotionController.cpp`
- `src/motion/MotionController.h`
- `src/motion/MotionFunctions.cpp`
- `src/motion/MotionFunctions.h`
- `src/motion/OpeningTimeline.h`
- `src/motion/QuinticTrack.cpp`
- `src/motion/QuinticTrack.h`


### cpp-06 · 18 文件 / 2,364 行

深度 findings：1；调查：0。

- `src/motion/SampledReelCurve.h`
- `src/motion/ScalarSpring.cpp`
- `src/motion/ScalarSpring.h`
- `src/platform/linux/DisplayMonitor.cpp`
- `src/platform/macos/ApplicationMenu.h`
- `src/platform/macos/ApplicationMenu.mm`
- `src/platform/macos/DisplayMonitor.mm`
- `src/platform/windows/DisplayMonitor.cpp`
- `src/presentation/DetailModel.cpp`
- `src/presentation/DetailModel.h`
- `src/presentation/GenreRowsModel.cpp`
- `src/presentation/GenreRowsModel.h`
- `src/presentation/OptionMetrics.cpp`
- `src/presentation/OptionMetrics.h`
- `src/presentation/PanelController.cpp`
- `src/presentation/PanelController.h`
- `src/presentation/PresentationController.cpp`
- `src/presentation/PresentationController.h`


### cpp-07 · 24 文件 / 2,442 行

深度 findings：3；调查：0。

- `src/presentation/SurfaceTransition.cpp`
- `src/presentation/SurfaceTransition.h`
- `src/presentation/TrackFocusController.cpp`
- `src/presentation/TrackFocusController.h`
- `src/render/ArchiveField.cpp`
- `src/render/ArchiveField.h`
- `src/render/ArchivePool.cpp`
- `src/render/ArchivePool.h`
- `src/render/CameraFraming.h`
- `src/render/CanvasSupport.h`
- `src/render/CaseClarity.cpp`
- `src/render/CaseClarity.h`
- `src/render/ColumnMemory.cpp`
- `src/render/ColumnMemory.h`
- `src/render/DetailMotion.cpp`
- `src/render/DetailMotion.h`
- `src/render/DetailPorts.cpp`
- `src/render/DetailPorts.h`
- `src/render/LayoutMetrics.cpp`
- `src/render/LayoutMetrics.h`
- `src/render/OpticalParameters.h`
- `src/render/OverviewController.cpp`
- `src/render/OverviewController.h`
- `src/render/OverviewLabelsModel.cpp`


### cpp-08 · 18 文件 / 1,699 行

深度 findings：1；调查：0。

- `src/render/OverviewLabelsModel.h`
- `src/render/PointerDragController.cpp`
- `src/render/PointerDragController.h`
- `src/render/RealColumns.cpp`
- `src/render/RealColumns.h`
- `src/render/RenderBackend.h`
- `src/render/RenderQuality.cpp`
- `src/render/RenderQuality.h`
- `src/render/ReturningCasePool.cpp`
- `src/render/ReturningCasePool.h`
- `src/render/RowLightController.cpp`
- `src/render/RowLightController.h`
- `src/render/SceneAdapter.cpp`
- `src/render/SceneAdapter.h`
- `src/render/SceneFrame.h`
- `src/render/SceneGraphLifecycle.cpp`
- `src/render/SceneGraphLifecycle.h`
- `src/render/SceneMath.h`


### cpp-09 · 9 文件 / 2,277 行

深度 findings：0；调查：0。

- `src/render/SelectionController.cpp`
- `src/render/SelectionController.h`
- `src/render/SelectionLighting.cpp`
- `src/render/SelectionLighting.h`
- `src/render/SettingsController.cpp`
- `src/render/SettingsController.h`
- `src/render/SourceOptics.h`
- `src/render/StartupCoordinator.cpp`
- `src/render/StartupCoordinator.h`


### cpp-10 · 15 文件 / 2,400 行

深度 findings：2；调查：1。

- `src/render/navigation/InputRouter.cpp`
- `src/render/navigation/InputRouter.h`
- `src/render/navigation/OrdinalMotion.cpp`
- `src/render/navigation/OrdinalMotion.h`
- `src/render/navigation/RulerController.cpp`
- `src/render/navigation/RulerController.h`
- `src/render/navigation/RulerInk.cpp`
- `src/render/navigation/RulerInk.h`
- `src/render/opening/BrandPaths.h`
- `src/render/opening/OpeningController.cpp`
- `src/render/opening/OpeningController.h`
- `src/render/opening/OpeningFrame.cpp`
- `src/render/opening/OpeningFrame.h`
- `src/render/opening/OpeningInk.cpp`
- `src/render/opening/OpeningInk.h`


### cpp-11 · 24 文件 / 2,542 行

深度 findings：0；调查：0。

- `src/render/rhi/AtlasAllocator.cpp`
- `src/render/rhi/AtlasAllocator.h`
- `src/render/rhi/AtlasUpload.cpp`
- `src/render/rhi/AtlasUpload.h`
- `src/render/rhi/BackendCapabilities.cpp`
- `src/render/rhi/BackendCapabilities.h`
- `src/render/rhi/DiagnosticSamples.h`
- `src/render/rhi/DisplayOutput.cpp`
- `src/render/rhi/DisplayOutput.h`
- `src/render/rhi/DisplayState.h`
- `src/render/rhi/DrawUniform.h`
- `src/render/rhi/FrameUniforms.cpp`
- `src/render/rhi/FrameUniforms.h`
- `src/render/rhi/PixelStorage.h`
- `src/render/rhi/Postprocess.cpp`
- `src/render/rhi/Postprocess.h`
- `src/render/rhi/PostprocessDefaults.h`
- `src/render/rhi/PreparedAssets.cpp`
- `src/render/rhi/PreparedAssets.h`
- `src/render/rhi/ReadbackImage.cpp`
- `src/render/rhi/ReadbackImage.h`
- `src/render/rhi/ReadbackQueue.cpp`
- `src/render/rhi/ReadbackQueue.h`
- `src/render/rhi/ReadbackQueue_p.h`


### cpp-12 · 11 文件 / 1,037 行

深度 findings：0；调查：0。

- `src/render/rhi/ReelTextNode.cpp`
- `src/render/rhi/ReelTextNode.h`
- `src/render/rhi/RenderData.h`
- `src/render/rhi/RenderTarget.cpp`
- `src/render/rhi/RenderTarget.h`
- `src/render/rhi/ResourceInventory.h`
- `src/render/rhi/ResourcePlan.h`
- `src/render/rhi/ReviewGlCapabilities.h`
- `src/render/rhi/RhiApi.h`
- `src/render/rhi/RhiItem.cpp`
- `src/render/rhi/RhiItem.h`


### cpp-13 · 6 文件 / 2,623 行

深度 findings：1；调查：0。

- `src/render/rhi/RhiRenderer.cpp`
- `src/render/rhi/RhiRenderer.h`
- `src/render/rhi/SceneCovers.cpp`
- `src/render/rhi/SceneCovers.h`
- `src/render/rhi/SceneGeometryData.cpp`
- `src/render/rhi/SceneGeometryData.h`


### cpp-14 · 13 文件 / 2,220 行

深度 findings：2；调查：0。

- `src/render/rhi/SceneItem.cpp`
- `src/render/rhi/SceneItem.h`
- `src/render/rhi/ScenePipeline.cpp`
- `src/render/rhi/ScenePipeline.h`
- `src/render/rhi/ShellMaterial.cpp`
- `src/render/rhi/ShellMaterial.h`
- `src/render/rhi/TransferAudit.h`
- `src/render/text/ReelInk.cpp`
- `src/render/text/ReelInk.h`
- `src/render/text/TextAreaFormat.cpp`
- `src/render/text/TextAreaFormat.h`
- `src/render/text/TextLayoutAdapter.cpp`
- `src/render/text/TextLayoutAdapter.h`


### cpp-15 · 13 文件 / 2,616 行

深度 findings：1；调查：0。

- `src/render/text/TitleReel.cpp`
- `src/render/text/TitleReel.h`
- `src/render/text/TransportMotion.cpp`
- `src/render/text/TransportMotion.h`
- `src/services/CoverCache.cpp`
- `src/services/CoverCache.h`
- `src/services/DemoLibrary.cpp`
- `src/services/DemoLibrary.h`
- `src/services/IMetadataProvider.cpp`
- `src/services/IMetadataProvider.h`
- `src/services/LibrarySession.cpp`
- `src/services/LibrarySession.h`
- `src/services/LibrarySession_p.h`


### cpp-16 · 20 文件 / 2,628 行

深度 findings：1；调查：0。

- `src/services/MetadataPresentation.cpp`
- `src/services/MetadataPresentation.h`
- `src/services/OfflineMetadataProvider.cpp`
- `src/services/OfflineMetadataProvider.h`
- `src/services/OnlineMetadataProvider.cpp`
- `src/services/OnlineMetadataProvider.h`
- `src/services/PreferencesStore.cpp`
- `src/services/PreferencesStore.h`
- `src/services/SingleInstance.cpp`
- `src/services/SingleInstance.h`
- `src/services/metadata/MetadataCachePolicy.cpp`
- `src/services/metadata/MetadataCachePolicy.h`
- `src/services/metadata/MetadataCoordinator.cpp`
- `src/services/metadata/MetadataCoordinator.h`
- `src/services/metadata/MetadataHttp.cpp`
- `src/services/metadata/MetadataHttp.h`
- `src/services/metadata/MetadataMerge.cpp`
- `src/services/metadata/MetadataMerge.h`
- `src/services/metadata/MetadataWorker.cpp`
- `src/services/metadata/MetadataWorker.h`


### cpp-17 · 13 文件 / 2,486 行

深度 findings：1；调查：0。

- `src/services/metadata/MusicBrainzSource.cpp`
- `src/services/metadata/MusicBrainzSource.h`
- `src/services/metadata/WikipediaSource.cpp`
- `src/services/metadata/WikipediaSource.h`
- `tests/integration/AssetTest.cpp`
- `tests/integration/LibraryQmlTest.cpp`
- `tests/integration/MacApplicationMenuTest.mm`
- `tests/render/ArchivePoolChecks.cpp`
- `tests/render/CaptureMailbox.h`
- `tests/render/ControlsChecks.cpp`
- `tests/render/ControlsChecks.h`
- `tests/render/CoverChecks.cpp`
- `tests/render/CoverSamplingChecks.h`


### cpp-18 · 13 文件 / 2,318 行

深度 findings：1；调查：0。

- `tests/render/ExperimentChecks.cpp`
- `tests/render/ExperimentChecks.h`
- `tests/render/ImageSampling.h`
- `tests/render/LibraryChecks.cpp`
- `tests/render/LibraryChecks.h`
- `tests/render/OffscreenRhi.h`
- `tests/render/OpeningChecks.cpp`
- `tests/render/OpeningChecks.h`
- `tests/render/OpeningLayoutCases.h`
- `tests/render/OutputReference.h`
- `tests/render/OverviewChecks.cpp`
- `tests/render/OverviewChecks.h`
- `tests/render/PostprocessChecks.h`


### cpp-19 · 11 文件 / 2,428 行

深度 findings：0；调查：0。

- `tests/render/PresentationChecks.cpp`
- `tests/render/PresentationChecks.h`
- `tests/render/RequiredOutput.h`
- `tests/render/RhiAoChecks.cpp`
- `tests/render/RhiBokehChecks.cpp`
- `tests/render/RhiCaptureHarness.h`
- `tests/render/RhiFlickerChecks.cpp`
- `tests/render/RhiFloorChecks.h`
- `tests/render/RhiFogChecks.cpp`
- `tests/render/RhiFogChecks.h`
- `tests/render/RhiOpaqueChecks.cpp`


### cpp-20 · 9 文件 / 2,606 行

深度 findings：1；调查：0。

- `tests/render/RhiPostprocessChecks.cpp`
- `tests/render/RhiProductionFixture.h`
- `tests/render/RhiReviewInput.h`
- `tests/render/RhiScaleChecks.cpp`
- `tests/render/RhiShadowChecks.cpp`
- `tests/render/RhiShadowChecks.h`
- `tests/render/RhiShellChecks.cpp`
- `tests/render/RhiSmaaChecks.cpp`
- `tests/render/SmaaReference.h`


### cpp-21 · 14 文件 / 2,530 行

深度 findings：1；调查：0。

- `tests/render/StartupChecks.cpp`
- `tests/render/StartupChecks.h`
- `tests/render/TransferChecks.h`
- `tests/render/UiTests.cpp`
- `tests/support/AudioFixture.h`
- `tests/support/DriverInfoMailbox.h`
- `tests/support/FileFixture.h`
- `tests/support/LibraryFixture.h`
- `tests/support/LibrarySearchOracle.h`
- `tests/support/NumericChecks.h`
- `tests/support/OffscreenSurface.h`
- `tests/support/RenderControlFrame.h`
- `tests/support/SettingsFixture.h`
- `tests/unit/ApplicationOptionsTest.cpp`


### cpp-22 · 5 文件 / 2,469 行

深度 findings：0；调查：0。

- `tests/unit/ArchivePoolTest.cpp`
- `tests/unit/AtlasUploadTest.cpp`
- `tests/unit/AudioAmbienceTest.cpp`
- `tests/unit/CoverCacheTest.cpp`
- `tests/unit/DetailMotionTest.cpp`


### cpp-23 · 9 文件 / 2,997 行

深度 findings：3；调查：0。

- `tests/unit/DirectoryScannerTest.cpp`
- `tests/unit/DisplayOutputTest.cpp`
- `tests/unit/DisplayOutputTypes.h`
- `tests/unit/ExperimentTest.cpp`
- `tests/unit/GraphicsBackendTest.cpp`
- `tests/unit/IdentityTest.cpp`
- `tests/unit/JsonFileTest.cpp`
- `tests/unit/LibraryDataTest.cpp`
- `tests/unit/LightingTest.cpp`


### cpp-24 · 8 文件 / 2,957 行

深度 findings：1；调查：0。

- `tests/unit/LocalizationTest.cpp`
- `tests/unit/LoggingTest.cpp`
- `tests/unit/MediaMetadataTest.cpp`
- `tests/unit/ModuleProbe.cpp`
- `tests/unit/MotionTest.cpp`
- `tests/unit/NativeToolTest.cpp`
- `tests/unit/NavigationTest.cpp`
- `tests/unit/OnlineMetadataTest.cpp`


### cpp-25 · 9 文件 / 3,063 行

深度 findings：2；调查：0。

- `tests/unit/OpeningTest.cpp`
- `tests/unit/OverviewTest.cpp`
- `tests/unit/PlaybackTest.cpp`
- `tests/unit/PresentationTest.cpp`
- `tests/unit/QmlBehaviorTest.cpp`
- `tests/unit/ReadbackQueueTest.cpp`
- `tests/unit/RealColumnsTest.cpp`
- `tests/unit/RenderQualityTest.cpp`
- `tests/unit/RenderSupportTest.cpp`


### cpp-26 · 10 文件 / 2,632 行

深度 findings：1；调查：0。

- `tests/unit/RhiMaterialTest.cpp`
- `tests/unit/SceneTest.cpp`
- `tests/unit/SettingsTest.cpp`
- `tests/unit/SingleInstanceTest.cpp`
- `tests/unit/StartupTest.cpp`
- `tests/unit/TitleTest.cpp`
- `tests/unit/TrackFocusTest.cpp`
- `tests/unit/VerificationInputTest.cpp`
- `tests/unit/VerificationWindowTest.cpp`
- `tools/assets/AssetTransaction.h`


### cpp-27 · 11 文件 / 3,515 行

深度 findings：3；调查：0。

- `tools/assets/ExportBranding.cpp`
- `tools/assets/GeometryAudit.cpp`
- `tools/assets/GeometryAudit.h`
- `tools/assets/PrepareAssets.cpp`
- `tools/migration/MetadataNetworkProbe.cpp`
- `tools/migration/ProbeInput.h`
- `tools/migration/RulerSourceProbe.cpp`
- `tools/migration/RulerSourceProbe.h`
- `tools/migration/SourceUiProbe.cpp`
- `tools/migration/TitleSourceProbe.cpp`
- `tools/migration/TitleSourceProbe.h`


### qml-01 · 20 文件 / 1,366 行

深度 findings：1；调查：0。

- `qml/Yalahoo/Music/App.qml`
- `qml/Yalahoo/Music/components/ArchiveButton.qml`
- `qml/Yalahoo/Music/components/ArchiveCheckBox.qml`
- `qml/Yalahoo/Music/components/ArchiveComboBox.qml`
- `qml/Yalahoo/Music/components/ArchiveEditMenu.qml`
- `qml/Yalahoo/Music/components/ArchiveEditorSurface.qml`
- `qml/Yalahoo/Music/components/ArchiveProgressBar.qml`
- `qml/Yalahoo/Music/components/ArchiveScrollBar.qml`
- `qml/Yalahoo/Music/components/ArchiveScrollView.qml`
- `qml/Yalahoo/Music/components/ArchiveSlider.qml`
- `qml/Yalahoo/Music/components/ArchiveText.qml`
- `qml/Yalahoo/Music/components/ArchiveTextArea.qml`
- `qml/Yalahoo/Music/components/ArchiveTextField.qml`
- `qml/Yalahoo/Music/components/ArchiveToolTip.qml`
- `qml/Yalahoo/Music/components/ArchiveVectorImage.qml`
- `qml/Yalahoo/Music/components/BackdropBlur.qml`
- `qml/Yalahoo/Music/components/BackdropBlurPass.qml`
- `qml/Yalahoo/Music/components/BackdropTexture.qml`
- `qml/Yalahoo/Music/components/BrowseInfo.qml`
- `qml/Yalahoo/Music/components/ColumnNavigation.qml`


### qml-02 · 14 文件 / 1,372 行

深度 findings：0；调查：0。

- `qml/Yalahoo/Music/components/CreditCard.qml`
- `qml/Yalahoo/Music/components/DesignTokens.qml`
- `qml/Yalahoo/Music/components/DetailAlbumInfo.qml`
- `qml/Yalahoo/Music/components/DetailHeader.qml`
- `qml/Yalahoo/Music/components/DetailIntroduction.qml`
- `qml/Yalahoo/Music/components/DetailTabs.qml`
- `qml/Yalahoo/Music/components/DetailTabsBackdrop.qml`
- `qml/Yalahoo/Music/components/DisplaySurface.qml`
- `qml/Yalahoo/Music/components/Footer.qml`
- `qml/Yalahoo/Music/components/Header.qml`
- `qml/Yalahoo/Music/components/HeaderTransport.qml`
- `qml/Yalahoo/Music/components/NavigationFrame.qml`
- `qml/Yalahoo/Music/components/OpeningBackdrop.qml`
- `qml/Yalahoo/Music/components/OpeningFilm.qml`


### qml-03 · 13 文件 / 1,283 行

深度 findings：0；调查：0。

- `qml/Yalahoo/Music/components/OrdinalReel.qml`
- `qml/Yalahoo/Music/components/OverviewBackdrop.qml`
- `qml/Yalahoo/Music/components/OverviewLabel.qml`
- `qml/Yalahoo/Music/components/PlaybackControls.qml`
- `qml/Yalahoo/Music/components/ReadingVeil.qml`
- `qml/Yalahoo/Music/components/Ruler.qml`
- `qml/Yalahoo/Music/components/SceneGraphVisual.qml`
- `qml/Yalahoo/Music/components/StageFocus.qml`
- `qml/Yalahoo/Music/components/StageFrameDriver.qml`
- `qml/Yalahoo/Music/components/ThemeSwitch.qml`
- `qml/Yalahoo/Music/components/TitleReel.qml`
- `qml/Yalahoo/Music/components/TrackList.qml`
- `qml/Yalahoo/Music/components/TrackListReveal.qml`


### qml-04 · 12 文件 / 1,432 行

深度 findings：1；调查：0。

- `qml/Yalahoo/Music/components/TrackRow.qml`
- `qml/Yalahoo/Music/components/TransportTitle.qml`
- `qml/Yalahoo/Music/pages/Browse.qml`
- `qml/Yalahoo/Music/pages/Detail.qml`
- `qml/Yalahoo/Music/pages/MusicStage.qml`
- `qml/Yalahoo/Music/pages/Opening.qml`
- `qml/Yalahoo/Music/pages/Overview.qml`
- `qml/Yalahoo/Music/panels/AppearanceSettings.qml`
- `qml/Yalahoo/Music/panels/ArrayQualitySettings.qml`
- `qml/Yalahoo/Music/panels/ExperimentOverlay.qml`
- `qml/Yalahoo/Music/panels/ExperimentPanels.qml`
- `qml/Yalahoo/Music/panels/InterfaceSoundPreview.qml`


### qml-05 · 13 文件 / 1,338 行

深度 findings：1；调查：0。

- `qml/Yalahoo/Music/panels/LibraryPanel.qml`
- `qml/Yalahoo/Music/panels/LibraryPanels.qml`
- `qml/Yalahoo/Music/panels/LightingModes.qml`
- `qml/Yalahoo/Music/panels/LightingPanel.qml`
- `qml/Yalahoo/Music/panels/MetadataSection.qml`
- `qml/Yalahoo/Music/panels/MetadataSettings.qml`
- `qml/Yalahoo/Music/panels/MotionPanel.qml`
- `qml/Yalahoo/Music/panels/MotionSpeedRange.qml`
- `qml/Yalahoo/Music/panels/PanelContents.qml`
- `qml/Yalahoo/Music/panels/PanelDescription.qml`
- `qml/Yalahoo/Music/panels/PanelHeading.qml`
- `qml/Yalahoo/Music/panels/PlaybackPanel.qml`
- `qml/Yalahoo/Music/panels/SearchFilters.qml`


### qml-06 · 21 文件 / 1,538 行

深度 findings：0；调查：0。

- `qml/Yalahoo/Music/panels/SearchPanel.qml`
- `qml/Yalahoo/Music/panels/SearchResult.qml`
- `qml/Yalahoo/Music/panels/SettingChoice.qml`
- `qml/Yalahoo/Music/panels/SettingRange.qml`
- `qml/Yalahoo/Music/panels/SettingsPanel.qml`
- `qml/Yalahoo/Music/panels/SettingsSection.qml`
- `qml/Yalahoo/Music/panels/SoundSettings.qml`
- `qml/Yalahoo/Music/panels/StagePanels.qml`
- `qml/Yalahoo/Music/panels/ThemeCard.qml`
- `qml/Yalahoo/Music/scene/CaseMaterials.qml`
- `qml/Yalahoo/Music/scene/GeometryScene.qml`
- `qml/Yalahoo/Music/scene/GlassMaterial.qml`
- `qml/Yalahoo/Music/scene/LightingMaterial.qml`
- `resources/models/Music_case_v040.qml`
- `tests/render/ControlsReview.qml`
- `tests/render/DetailReview.qml`
- `tests/render/LibraryReview.qml`
- `tests/render/OverviewVisual.qml`
- `tests/unit/DisplayOutputProbe.qml`
- `tools/migration/RulerReview.qml`
- `tools/migration/TitleReview.qml`


## 独立 qmllint 诊断附录

以下为 skill 首轮独立扫描的全部 QL 规则/消息及位置；为复核完整性保留，经上下文配置后是否成立应以上述官方 target 及相应未编入配置文件的人工追踪为准。

| Rule / message | Locations | Mitigation |
| --- | --- | --- |
| QL:import ArchiveButton was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:74 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveCheckBox was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:23 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveComboBox was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:36 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveProgressBar was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:68 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveSlider was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:28 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveTextArea was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:62 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ArchiveTextField was not found. Did you add all imports and dependencies? (1) | tests/render/ControlsReview.qml:56 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import BackgroundMusicController was not found. Did you add all imports and dependencies? (8) | qml/Yalahoo/Music/App.qml:22  qml/Yalahoo/Music/pages/MusicStage.qml:26  qml/Yalahoo/Music/panels/LibraryPanels.qml:17  qml/Yalahoo/Music/panels/PanelContents.qml:16  qml/Yalahoo/Music/panels/PlaybackPanel.qml:10  qml/Yalahoo/Music/panels/SettingsPanel.qml:16  qml/Yalahoo/Music/panels/SoundSettings.qml:31  qml/Yalahoo/Music/panels/StagePanels.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Browse was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/StageFocus.qml:12 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import DesignTokens was not found. Did you add all imports and dependencies? (5) | tests/render/ControlsReview.qml:12  tests/render/DetailReview.qml:15  tests/render/OverviewVisual.qml:13  tools/migration/RulerReview.qml:16  tools/migration/TitleReview.qml:20 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Detail was not found. Did you add all imports and dependencies? (1) | tests/render/DetailReview.qml:26 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import DetailModel was not found. Did you add all imports and dependencies? (6) | qml/Yalahoo/Music/App.qml:28  qml/Yalahoo/Music/components/TrackList.qml:19  qml/Yalahoo/Music/components/TrackRow.qml:9  qml/Yalahoo/Music/pages/Detail.qml:20  qml/Yalahoo/Music/pages/MusicStage.qml:32  tests/render/DetailReview.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import DisplayOutput was not found. Did you add all imports and dependencies? (3) | qml/Yalahoo/Music/App.qml:34  qml/Yalahoo/Music/components/DisplaySurface.qml:6  tests/unit/DisplayOutputProbe.qml:8 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import DisplaySurface was not found. Did you add all imports and dependencies? (1) | tests/unit/DisplayOutputProbe.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Failed to import Yalahoo.Music. Are your import paths set up properly? (7) | qml/Yalahoo/Music/App.qml:7  qml/Yalahoo/Music/components/ArchiveButton.qml:4  qml/Yalahoo/Music/pages/Browse.qml:3  qml/Yalahoo/Music/panels/AppearanceSettings.qml:3  qml/Yalahoo/Music/scene/CaseMaterials.qml:4  tests/render/ControlsReview.qml:3  tools/migration/RulerReview.qml:3 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import File or directory you are trying to import does not exist: tests/render/components. (3) | tests/render/ControlsReview.qml:4  tests/render/DetailReview.qml:4  tests/render/OverviewVisual.qml:4 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import File or directory you are trying to import does not exist: tests/render/pages. (3) | tests/render/DetailReview.qml:5  tests/render/LibraryReview.qml:4  tests/render/OverviewVisual.qml:5 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import File or directory you are trying to import does not exist: tests/unit/components. (1) | tests/unit/DisplayOutputProbe.qml:4 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import File or directory you are trying to import does not exist: tools/migration/components. (2) | tools/migration/RulerReview.qml:4  tools/migration/TitleReview.qml:4 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import FixedTextureData was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/scene/CaseMaterials.qml:18 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import FrameClock was not found. Did you add all imports and dependencies? (11) | qml/Yalahoo/Music/components/BrowseInfo.qml:15  qml/Yalahoo/Music/components/ColumnNavigation.qml:10  qml/Yalahoo/Music/components/Header.qml:23  qml/Yalahoo/Music/components/HeaderTransport.qml:9  qml/Yalahoo/Music/components/NavigationFrame.qml:14  qml/Yalahoo/Music/components/OrdinalReel.qml:8  qml/Yalahoo/Music/components/TitleReel.qml:11  qml/Yalahoo/Music/components/TransportTitle.qml:11  qml/Yalahoo/Music/pages/Browse.qml:13  tools/migration/RulerReview.qml:9  tools/migration/TitleReview.qml:10 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import GenreRowsModel was not found. Did you add all imports and dependencies? (2) | qml/Yalahoo/Music/panels/SearchFilters.qml:11  qml/Yalahoo/Music/panels/SearchPanel.qml:24 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Header was not found. Did you add all imports and dependencies? (1) | tools/migration/TitleReview.qml:35 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import InputRouter was not found. Did you add all imports and dependencies? (8) | qml/Yalahoo/Music/App.qml:18  qml/Yalahoo/Music/components/ColumnNavigation.qml:9  qml/Yalahoo/Music/components/NavigationFrame.qml:13  qml/Yalahoo/Music/components/StageFrameDriver.qml:10  qml/Yalahoo/Music/pages/Browse.qml:12  qml/Yalahoo/Music/pages/MusicStage.qml:20  qml/Yalahoo/Music/pages/Overview.qml:14  tests/render/OverviewVisual.qml:10 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import InterfaceSoundController was not found. Did you add all imports and dependencies? (9) | qml/Yalahoo/Music/App.qml:23  qml/Yalahoo/Music/pages/MusicStage.qml:27  qml/Yalahoo/Music/panels/InterfaceSoundPreview.qml:8  qml/Yalahoo/Music/panels/LibraryPanels.qml:18  qml/Yalahoo/Music/panels/PanelContents.qml:17  qml/Yalahoo/Music/panels/PlaybackPanel.qml:11  qml/Yalahoo/Music/panels/SettingsPanel.qml:17  qml/Yalahoo/Music/panels/SoundSettings.qml:32  qml/Yalahoo/Music/panels/StagePanels.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import LayoutMetrics was not found. Did you add all imports and dependencies? (9) | qml/Yalahoo/Music/components/BrowseInfo.qml:11  qml/Yalahoo/Music/components/Footer.qml:7  qml/Yalahoo/Music/components/Header.qml:18  qml/Yalahoo/Music/components/NavigationFrame.qml:10  qml/Yalahoo/Music/components/ReadingVeil.qml:14  qml/Yalahoo/Music/pages/Browse.qml:8  qml/Yalahoo/Music/pages/Detail.qml:17  tests/render/DetailReview.qml:9  tools/migration/TitleReview.qml:9 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import LibrarySession was not found. Did you add all imports and dependencies? (10) | qml/Yalahoo/Music/App.qml:13  qml/Yalahoo/Music/pages/MusicStage.qml:13  qml/Yalahoo/Music/panels/LibraryPanel.qml:9  qml/Yalahoo/Music/panels/LibraryPanels.qml:13  qml/Yalahoo/Music/panels/MetadataSection.qml:9  qml/Yalahoo/Music/panels/MetadataSettings.qml:9  qml/Yalahoo/Music/panels/PanelContents.qml:10  qml/Yalahoo/Music/panels/SearchPanel.qml:10  qml/Yalahoo/Music/panels/SettingsPanel.qml:13  qml/Yalahoo/Music/panels/StagePanels.qml:10 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import LibraryStore was not found. Did you add all imports and dependencies? (8) | qml/Yalahoo/Music/App.qml:27  qml/Yalahoo/Music/pages/MusicStage.qml:31  qml/Yalahoo/Music/panels/LibraryPanel.qml:11  qml/Yalahoo/Music/panels/LibraryPanels.qml:23  qml/Yalahoo/Music/panels/MetadataSettings.qml:10  qml/Yalahoo/Music/panels/PanelContents.qml:22  qml/Yalahoo/Music/panels/SettingsPanel.qml:14  qml/Yalahoo/Music/panels/StagePanels.qml:17 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import MotionController was not found. Did you add all imports and dependencies? (3) | qml/Yalahoo/Music/App.qml:14  qml/Yalahoo/Music/components/StageFrameDriver.qml:9  qml/Yalahoo/Music/pages/MusicStage.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import MusicStage was not found. Did you add all imports and dependencies? (1) | tests/render/LibraryReview.qml:6 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Opening was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/StageFocus.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OpeningController was not found. Did you add all imports and dependencies? (10) | qml/Yalahoo/Music/App.qml:17  qml/Yalahoo/Music/components/Header.qml:22  qml/Yalahoo/Music/components/OpeningFilm.qml:8  qml/Yalahoo/Music/components/StageFocus.qml:8  qml/Yalahoo/Music/components/StageFrameDriver.qml:14  qml/Yalahoo/Music/pages/Browse.qml:10  qml/Yalahoo/Music/pages/MusicStage.qml:16  qml/Yalahoo/Music/pages/Opening.qml:13  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:12  qml/Yalahoo/Music/panels/StagePanels.qml:18 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OpeningInk was not found. Did you add all imports and dependencies? (2) | qml/Yalahoo/Music/components/OpeningFilm.qml:28,47 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OptionMetrics was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/ArchiveComboBox.qml:16 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OrdinalMotion was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/OrdinalReel.qml:36 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OrdinalReel was not found. Did you add all imports and dependencies? (1) | tools/migration/RulerReview.qml:38 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Overview was not found. Did you add all imports and dependencies? (1) | tests/render/OverviewVisual.qml:29 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import OverviewController was not found. Did you add all imports and dependencies? (8) | qml/Yalahoo/Music/App.qml:29  qml/Yalahoo/Music/components/Header.qml:24  qml/Yalahoo/Music/components/OverviewLabel.qml:9  qml/Yalahoo/Music/components/StageFocus.qml:10  qml/Yalahoo/Music/pages/Browse.qml:15  qml/Yalahoo/Music/pages/MusicStage.qml:33  qml/Yalahoo/Music/pages/Overview.qml:11  tests/render/OverviewVisual.qml:9 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import PanelController was not found. Did you add all imports and dependencies? (6) | qml/Yalahoo/Music/App.qml:26  qml/Yalahoo/Music/pages/MusicStage.qml:30  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:13  qml/Yalahoo/Music/panels/LibraryPanels.qml:14  qml/Yalahoo/Music/panels/PanelContents.qml:11  qml/Yalahoo/Music/panels/StagePanels.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import PlaybackController was not found. Did you add all imports and dependencies? (9) | qml/Yalahoo/Music/App.qml:21  qml/Yalahoo/Music/components/PlaybackControls.qml:8  qml/Yalahoo/Music/pages/MusicStage.qml:25  qml/Yalahoo/Music/panels/LibraryPanels.qml:16  qml/Yalahoo/Music/panels/PanelContents.qml:15  qml/Yalahoo/Music/panels/PlaybackPanel.qml:9  qml/Yalahoo/Music/panels/SettingsPanel.qml:15  qml/Yalahoo/Music/panels/SoundSettings.qml:30  qml/Yalahoo/Music/panels/StagePanels.qml:13 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import PresentationController was not found. Did you add all imports and dependencies? (10) | qml/Yalahoo/Music/App.qml:20  qml/Yalahoo/Music/components/DetailAlbumInfo.qml:9  qml/Yalahoo/Music/components/DetailHeader.qml:9  qml/Yalahoo/Music/components/DetailTabs.qml:9  qml/Yalahoo/Music/components/StageFocus.qml:9  qml/Yalahoo/Music/components/StageFrameDriver.qml:13  qml/Yalahoo/Music/pages/Detail.qml:19  qml/Yalahoo/Music/pages/MusicStage.qml:23  qml/Yalahoo/Music/panels/StagePanels.qml:19  tests/render/DetailReview.qml:10 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ReelInk was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/TitleReel.qml:98 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import ReelSlot was not found. Did you add all imports and dependencies? (2) | qml/Yalahoo/Music/components/TitleReel.qml:71,72 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Ruler was not found. Did you add all imports and dependencies? (1) | tools/migration/RulerReview.qml:32 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import RulerController was not found. Did you add all imports and dependencies? (6) | qml/Yalahoo/Music/App.qml:19  qml/Yalahoo/Music/components/NavigationFrame.qml:12  qml/Yalahoo/Music/components/Ruler.qml:8  qml/Yalahoo/Music/pages/Browse.qml:11  qml/Yalahoo/Music/pages/MusicStage.qml:21  tools/migration/RulerReview.qml:8 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import RulerInk was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/Ruler.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SceneAdapter was not found. Did you add all imports and dependencies? (4) | qml/Yalahoo/Music/App.qml:15  qml/Yalahoo/Music/components/StageFrameDriver.qml:8  qml/Yalahoo/Music/pages/MusicStage.qml:22  qml/Yalahoo/Music/scene/GeometryScene.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SceneGraphLifecycle was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/SceneGraphVisual.qml:8 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SceneItem was not found. Did you add all imports and dependencies? (4) | qml/Yalahoo/Music/pages/MusicStage.qml:125  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:14  qml/Yalahoo/Music/panels/ExperimentPanels.qml:13  qml/Yalahoo/Music/panels/StagePanels.qml:20 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SelectionController was not found. Did you add all imports and dependencies? (2) | qml/Yalahoo/Music/components/NavigationFrame.qml:15  qml/Yalahoo/Music/pages/Browse.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SelectionLighting was not found. Did you add all imports and dependencies? (5) | qml/Yalahoo/Music/panels/ExperimentOverlay.qml:11  qml/Yalahoo/Music/panels/ExperimentPanels.qml:12  qml/Yalahoo/Music/panels/LightingPanel.qml:10  qml/Yalahoo/Music/scene/CaseMaterials.qml:8  qml/Yalahoo/Music/scene/LightingMaterial.qml:6 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SettingsController was not found. Did you add all imports and dependencies? (12) | qml/Yalahoo/Music/App.qml:25  qml/Yalahoo/Music/pages/MusicStage.qml:29  qml/Yalahoo/Music/panels/AppearanceSettings.qml:9  qml/Yalahoo/Music/panels/ArrayQualitySettings.qml:9  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:10  qml/Yalahoo/Music/panels/ExperimentPanels.qml:11  qml/Yalahoo/Music/panels/LibraryPanels.qml:19  qml/Yalahoo/Music/panels/MotionPanel.qml:10  qml/Yalahoo/Music/panels/MotionSpeedRange.qml:7  qml/Yalahoo/Music/panels/PanelContents.qml:18  qml/Yalahoo/Music/panels/SettingsPanel.qml:12  qml/Yalahoo/Music/panels/StagePanels.qml:16 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SourceOptics was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/scene/GlassMaterial.qml:10 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import StartupCoordinator was not found. Did you add all imports and dependencies? (3) | qml/Yalahoo/Music/App.qml:16  qml/Yalahoo/Music/components/StageFrameDriver.qml:11  qml/Yalahoo/Music/pages/MusicStage.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import SurfaceTransition was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/pages/Browse.qml:25 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import TextAreaFormat was not found. Did you add all imports and dependencies? (1) | qml/Yalahoo/Music/components/ArchiveTextArea.qml:33 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import TitleReel was not found. Did you add all imports and dependencies? (1) | tools/migration/TitleReview.qml:27 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import TitleReelMotion was not found. Did you add all imports and dependencies? (10) | qml/Yalahoo/Music/components/BrowseInfo.qml:19  qml/Yalahoo/Music/components/Header.qml:30  qml/Yalahoo/Music/components/HeaderTransport.qml:18  qml/Yalahoo/Music/components/TitleReel.qml:19,20,37  qml/Yalahoo/Music/components/TransportTitle.qml:18  qml/Yalahoo/Music/pages/Browse.qml:34,35  tools/migration/TitleReview.qml:8 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import TrackFocusController was not found. Did you add all imports and dependencies? (10) | qml/Yalahoo/Music/App.qml:24  qml/Yalahoo/Music/components/StageFrameDriver.qml:12  qml/Yalahoo/Music/components/TrackList.qml:20  qml/Yalahoo/Music/components/TrackListReveal.qml:10  qml/Yalahoo/Music/components/TrackRow.qml:10  qml/Yalahoo/Music/pages/Detail.qml:21  qml/Yalahoo/Music/pages/MusicStage.qml:28  qml/Yalahoo/Music/panels/LibraryPanels.qml:15  qml/Yalahoo/Music/panels/PanelContents.qml:12  qml/Yalahoo/Music/panels/StagePanels.qml:12 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import TransportMotion was not found. Did you add all imports and dependencies? (2) | qml/Yalahoo/Music/components/HeaderTransport.qml:69  qml/Yalahoo/Music/components/TransportTitle.qml:67 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:import Warnings occurred while importing module "Yalahoo.Music" (7) | qml/Yalahoo/Music/App.qml:7  qml/Yalahoo/Music/components/ArchiveButton.qml:4  qml/Yalahoo/Music/pages/Browse.qml:3  qml/Yalahoo/Music/panels/AppearanceSettings.qml:3  qml/Yalahoo/Music/scene/CaseMaterials.qml:4  tests/render/ControlsReview.qml:3  tools/migration/RulerReview.qml:3 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign binding of type SceneItem to QQuickItem (3) | qml/Yalahoo/Music/pages/MusicStage.qml:163,201,213 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to BackgroundMusicController (8) | qml/Yalahoo/Music/App.qml:22  qml/Yalahoo/Music/pages/MusicStage.qml:26  qml/Yalahoo/Music/panels/LibraryPanels.qml:17  qml/Yalahoo/Music/panels/PanelContents.qml:16  qml/Yalahoo/Music/panels/PlaybackPanel.qml:10  qml/Yalahoo/Music/panels/SettingsPanel.qml:16  qml/Yalahoo/Music/panels/SoundSettings.qml:31  qml/Yalahoo/Music/panels/StagePanels.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to DetailModel (2) | qml/Yalahoo/Music/App.qml:28  qml/Yalahoo/Music/pages/MusicStage.qml:32 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to DisplayOutput (2) | qml/Yalahoo/Music/App.qml:34  qml/Yalahoo/Music/components/DisplaySurface.qml:6 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to FrameClock (10) | qml/Yalahoo/Music/components/BrowseInfo.qml:15  qml/Yalahoo/Music/components/ColumnNavigation.qml:10  qml/Yalahoo/Music/components/Header.qml:23  qml/Yalahoo/Music/components/HeaderTransport.qml:9  qml/Yalahoo/Music/components/NavigationFrame.qml:14  qml/Yalahoo/Music/components/OrdinalReel.qml:8  qml/Yalahoo/Music/components/TitleReel.qml:11  qml/Yalahoo/Music/components/TransportTitle.qml:11  qml/Yalahoo/Music/pages/Browse.qml:13  tools/migration/RulerReview.qml:9 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to InputRouter (6) | qml/Yalahoo/Music/App.qml:18  qml/Yalahoo/Music/components/ColumnNavigation.qml:9  qml/Yalahoo/Music/components/NavigationFrame.qml:13  qml/Yalahoo/Music/components/StageFrameDriver.qml:10  qml/Yalahoo/Music/pages/Browse.qml:12  qml/Yalahoo/Music/pages/MusicStage.qml:20 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to InterfaceSoundController (8) | qml/Yalahoo/Music/App.qml:23  qml/Yalahoo/Music/pages/MusicStage.qml:27  qml/Yalahoo/Music/panels/LibraryPanels.qml:18  qml/Yalahoo/Music/panels/PanelContents.qml:17  qml/Yalahoo/Music/panels/PlaybackPanel.qml:11  qml/Yalahoo/Music/panels/SettingsPanel.qml:17  qml/Yalahoo/Music/panels/SoundSettings.qml:32  qml/Yalahoo/Music/panels/StagePanels.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to LibraryStore (8) | qml/Yalahoo/Music/App.qml:27  qml/Yalahoo/Music/pages/MusicStage.qml:31  qml/Yalahoo/Music/panels/LibraryPanel.qml:11  qml/Yalahoo/Music/panels/LibraryPanels.qml:23  qml/Yalahoo/Music/panels/MetadataSettings.qml:10  qml/Yalahoo/Music/panels/PanelContents.qml:22  qml/Yalahoo/Music/panels/SettingsPanel.qml:14  qml/Yalahoo/Music/panels/StagePanels.qml:17 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to Opening (1) | qml/Yalahoo/Music/components/StageFocus.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to OpeningController (8) | qml/Yalahoo/Music/App.qml:17  qml/Yalahoo/Music/components/Header.qml:22  qml/Yalahoo/Music/components/StageFocus.qml:8  qml/Yalahoo/Music/components/StageFrameDriver.qml:14  qml/Yalahoo/Music/pages/Browse.qml:10  qml/Yalahoo/Music/pages/MusicStage.qml:16  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:12  qml/Yalahoo/Music/panels/StagePanels.qml:18 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to OverviewController (5) | qml/Yalahoo/Music/App.qml:29  qml/Yalahoo/Music/components/Header.qml:24  qml/Yalahoo/Music/components/StageFocus.qml:10  qml/Yalahoo/Music/pages/Browse.qml:15  qml/Yalahoo/Music/pages/MusicStage.qml:33 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to PanelController (4) | qml/Yalahoo/Music/App.qml:26  qml/Yalahoo/Music/pages/MusicStage.qml:30  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:13  qml/Yalahoo/Music/panels/StagePanels.qml:11 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to PlaybackController (7) | qml/Yalahoo/Music/App.qml:21  qml/Yalahoo/Music/pages/MusicStage.qml:25  qml/Yalahoo/Music/panels/LibraryPanels.qml:16  qml/Yalahoo/Music/panels/PanelContents.qml:15  qml/Yalahoo/Music/panels/SettingsPanel.qml:15  qml/Yalahoo/Music/panels/SoundSettings.qml:30  qml/Yalahoo/Music/panels/StagePanels.qml:13 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to PresentationController (5) | qml/Yalahoo/Music/App.qml:20  qml/Yalahoo/Music/components/StageFocus.qml:9  qml/Yalahoo/Music/components/StageFrameDriver.qml:13  qml/Yalahoo/Music/pages/MusicStage.qml:23  qml/Yalahoo/Music/panels/StagePanels.qml:19 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to RulerController (4) | qml/Yalahoo/Music/App.qml:19  qml/Yalahoo/Music/components/NavigationFrame.qml:12  qml/Yalahoo/Music/pages/Browse.qml:11  qml/Yalahoo/Music/pages/MusicStage.qml:21 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to SelectionController (2) | qml/Yalahoo/Music/components/NavigationFrame.qml:15  qml/Yalahoo/Music/pages/Browse.qml:14 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to SettingsController (6) | qml/Yalahoo/Music/App.qml:25  qml/Yalahoo/Music/pages/MusicStage.qml:29  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:10  qml/Yalahoo/Music/panels/LibraryPanels.qml:19  qml/Yalahoo/Music/panels/PanelContents.qml:18  qml/Yalahoo/Music/panels/StagePanels.qml:16 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to StartupCoordinator (3) | qml/Yalahoo/Music/App.qml:16  qml/Yalahoo/Music/components/StageFrameDriver.qml:11  qml/Yalahoo/Music/pages/MusicStage.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to SurfaceTransition (1) | qml/Yalahoo/Music/pages/Browse.qml:25 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to TitleReelMotion (1) | qml/Yalahoo/Music/components/TitleReel.qml:19 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:incompatible-type Cannot assign literal of type null to TrackFocusController (8) | qml/Yalahoo/Music/App.qml:24  qml/Yalahoo/Music/components/StageFrameDriver.qml:12  qml/Yalahoo/Music/components/TrackList.qml:20  qml/Yalahoo/Music/components/TrackListReveal.qml:10  qml/Yalahoo/Music/components/TrackRow.qml:10  qml/Yalahoo/Music/pages/Detail.qml:21  qml/Yalahoo/Music/pages/MusicStage.qml:28  qml/Yalahoo/Music/panels/StagePanels.qml:12 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "backgroundMusic". This may be due to a missing import statement or incomplete qmltypes files. (6) | qml/Yalahoo/Music/panels/LibraryPanels.qml:118  qml/Yalahoo/Music/panels/PanelContents.qml:68,92  qml/Yalahoo/Music/panels/PlaybackPanel.qml:71  qml/Yalahoo/Music/panels/SettingsPanel.qml:130  qml/Yalahoo/Music/panels/StagePanels.qml:39 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "backgroundMusicController". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:71  qml/Yalahoo/Music/pages/MusicStage.qml:239 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "browse". This may be due to a missing import statement or incomplete qmltypes files. (1) | qml/Yalahoo/Music/pages/MusicStage.qml:102 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "clock". This may be due to a missing import statement or incomplete qmltypes files. (14) | qml/Yalahoo/Music/components/BrowseInfo.qml:124,147  qml/Yalahoo/Music/components/ColumnNavigation.qml:57,73  qml/Yalahoo/Music/components/Header.qml:131  qml/Yalahoo/Music/components/HeaderTransport.qml:61  qml/Yalahoo/Music/components/NavigationFrame.qml:89,110,165  qml/Yalahoo/Music/components/TransportTitle.qml:55  qml/Yalahoo/Music/pages/Browse.qml:81,100,124  qml/Yalahoo/Music/pages/MusicStage.qml:184 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "controller". This may be due to a missing import statement or incomplete qmltypes files. (7) | qml/Yalahoo/Music/components/NavigationFrame.qml:148  qml/Yalahoo/Music/pages/MusicStage.qml:164,202  qml/Yalahoo/Music/pages/Opening.qml:52  qml/Yalahoo/Music/pages/Overview.qml:88  qml/Yalahoo/Music/panels/LibraryPanels.qml:115  qml/Yalahoo/Music/panels/StagePanels.qml:36 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "detailModel". This may be due to a missing import statement or incomplete qmltypes files. (4) | qml/Yalahoo/Music/App.qml:77  qml/Yalahoo/Music/components/TrackList.qml:164  qml/Yalahoo/Music/pages/Detail.qml:164  qml/Yalahoo/Music/pages/MusicStage.qml:217 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "genreRows". This may be due to a missing import statement or incomplete qmltypes files. (1) | qml/Yalahoo/Music/panels/SearchPanel.qml:58 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "inputRouter". This may be due to a missing import statement or incomplete qmltypes files. (6) | qml/Yalahoo/Music/App.qml:67  qml/Yalahoo/Music/components/NavigationFrame.qml:164  qml/Yalahoo/Music/pages/Browse.qml:123  qml/Yalahoo/Music/pages/MusicStage.qml:108,183,205 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "interfaceSound". This may be due to a missing import statement or incomplete qmltypes files. (7) | qml/Yalahoo/Music/panels/LibraryPanels.qml:119  qml/Yalahoo/Music/panels/PanelContents.qml:69,93  qml/Yalahoo/Music/panels/PlaybackPanel.qml:72  qml/Yalahoo/Music/panels/SettingsPanel.qml:131  qml/Yalahoo/Music/panels/SoundSettings.qml:77  qml/Yalahoo/Music/panels/StagePanels.qml:40 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "interfaceSoundController". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:72  qml/Yalahoo/Music/pages/MusicStage.qml:240 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "librarySession". This may be due to a missing import statement or incomplete qmltypes files. (11) | qml/Yalahoo/Music/App.qml:62  qml/Yalahoo/Music/pages/MusicStage.qml:235  qml/Yalahoo/Music/panels/LibraryPanels.qml:114  qml/Yalahoo/Music/panels/MetadataSettings.qml:26  qml/Yalahoo/Music/panels/PanelContents.qml:58,89,122  qml/Yalahoo/Music/panels/SettingsPanel.qml:64,76,135  qml/Yalahoo/Music/panels/StagePanels.qml:35 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "libraryStore". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:76  qml/Yalahoo/Music/pages/MusicStage.qml:242 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "lighting". This may be due to a missing import statement or incomplete qmltypes files. (6) | qml/Yalahoo/Music/panels/ExperimentOverlay.qml:43  qml/Yalahoo/Music/panels/ExperimentPanels.qml:54  qml/Yalahoo/Music/panels/StagePanels.qml:68  qml/Yalahoo/Music/scene/CaseMaterials.qml:40,65  qml/Yalahoo/Music/scene/GeometryScene.qml:66 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "metrics". This may be due to a missing import statement or incomplete qmltypes files. (7) | qml/Yalahoo/Music/pages/Browse.qml:44,78,96,110,118  qml/Yalahoo/Music/pages/MusicStage.qml:172,214 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "motionController". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:63  qml/Yalahoo/Music/pages/MusicStage.qml:107 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "opening". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/pages/Browse.qml:80  qml/Yalahoo/Music/pages/MusicStage.qml:174 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "openingController". This may be due to a missing import statement or incomplete qmltypes files. (5) | qml/Yalahoo/Music/App.qml:66  qml/Yalahoo/Music/pages/MusicStage.qml:98,112,243  qml/Yalahoo/Music/panels/StagePanels.qml:69 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "openingOverlay". This may be due to a missing import statement or incomplete qmltypes files. (1) | qml/Yalahoo/Music/pages/MusicStage.qml:101 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "output". This may be due to a missing import statement or incomplete qmltypes files. (1) | qml/Yalahoo/Music/App.qml:53 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "overviewController". This may be due to a missing import statement or incomplete qmltypes files. (4) | qml/Yalahoo/Music/App.qml:78  qml/Yalahoo/Music/pages/Browse.qml:84  qml/Yalahoo/Music/pages/MusicStage.qml:100,181 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "panelController". This may be due to a missing import statement or incomplete qmltypes files. (3) | qml/Yalahoo/Music/App.qml:75  qml/Yalahoo/Music/pages/MusicStage.qml:236  qml/Yalahoo/Music/panels/StagePanels.qml:70 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "playback". This may be due to a missing import statement or incomplete qmltypes files. (7) | qml/Yalahoo/Music/panels/LibraryPanels.qml:117  qml/Yalahoo/Music/panels/PanelContents.qml:67,91  qml/Yalahoo/Music/panels/PlaybackPanel.qml:52,70  qml/Yalahoo/Music/panels/SettingsPanel.qml:129  qml/Yalahoo/Music/panels/StagePanels.qml:38 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "playbackController". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:70  qml/Yalahoo/Music/pages/MusicStage.qml:238 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "presentation". This may be due to a missing import statement or incomplete qmltypes files. (4) | qml/Yalahoo/Music/components/DetailHeader.qml:22  qml/Yalahoo/Music/pages/Detail.qml:184,198  qml/Yalahoo/Music/pages/MusicStage.qml:216 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "presentationController". This may be due to a missing import statement or incomplete qmltypes files. (4) | qml/Yalahoo/Music/App.qml:69  qml/Yalahoo/Music/pages/MusicStage.qml:99,111,244 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "rulerController". This may be due to a missing import statement or incomplete qmltypes files. (3) | qml/Yalahoo/Music/App.qml:68  qml/Yalahoo/Music/pages/Browse.qml:122  qml/Yalahoo/Music/pages/MusicStage.qml:182 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "sceneAdapter". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:64  qml/Yalahoo/Music/pages/MusicStage.qml:106 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "sceneSource". This may be due to a missing import statement or incomplete qmltypes files. (3) | qml/Yalahoo/Music/pages/MusicStage.qml:245  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:44  qml/Yalahoo/Music/panels/StagePanels.qml:71 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "selection". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/pages/Browse.qml:125  qml/Yalahoo/Music/pages/MusicStage.qml:189 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "settings". This may be due to a missing import statement or incomplete qmltypes files. (9) | qml/Yalahoo/Music/panels/ExperimentOverlay.qml:42  qml/Yalahoo/Music/panels/ExperimentPanels.qml:48  qml/Yalahoo/Music/panels/LibraryPanels.qml:120  qml/Yalahoo/Music/panels/MotionPanel.qml:28  qml/Yalahoo/Music/panels/PanelContents.qml:88  qml/Yalahoo/Music/panels/SettingsPanel.qml:56,87,107  qml/Yalahoo/Music/panels/StagePanels.qml:41 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "settingsController". This may be due to a missing import statement or incomplete qmltypes files. (3) | qml/Yalahoo/Music/App.qml:74  qml/Yalahoo/Music/pages/MusicStage.qml:241  qml/Yalahoo/Music/panels/StagePanels.qml:67 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "startupCoordinator". This may be due to a missing import statement or incomplete qmltypes files. (2) | qml/Yalahoo/Music/App.qml:65  qml/Yalahoo/Music/pages/MusicStage.qml:109 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "store". This may be due to a missing import statement or incomplete qmltypes files. (5) | qml/Yalahoo/Music/panels/LibraryPanels.qml:121  qml/Yalahoo/Music/panels/PanelContents.qml:90,123  qml/Yalahoo/Music/panels/SettingsPanel.qml:136  qml/Yalahoo/Music/panels/StagePanels.qml:60 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "surface". This may be due to a missing import statement or incomplete qmltypes files. (1) | qml/Yalahoo/Music/pages/MusicStage.qml:185 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "trackFocus". This may be due to a missing import statement or incomplete qmltypes files. (6) | qml/Yalahoo/Music/components/TrackList.qml:88,165  qml/Yalahoo/Music/pages/Detail.qml:165  qml/Yalahoo/Music/pages/MusicStage.qml:218  qml/Yalahoo/Music/panels/LibraryPanels.qml:116  qml/Yalahoo/Music/panels/StagePanels.qml:37 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:missing-type No type found for property "trackFocusController". This may be due to a missing import statement or incomplete qmltypes files. (3) | qml/Yalahoo/Music/App.qml:73  qml/Yalahoo/Music/pages/MusicStage.qml:110,237 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unqualified Unqualified access (25) | qml/Yalahoo/Music/components/DesignTokens.qml:31,49,50,51  qml/Yalahoo/Music/components/NavigationFrame.qml:77,78,79,101,102,103  qml/Yalahoo/Music/components/OpeningFilm.qml:30,31,51,52,53,56  qml/Yalahoo/Music/components/OrdinalReel.qml:15  qml/Yalahoo/Music/components/TitleReel.qml:99  qml/Yalahoo/Music/pages/MusicStage.qml:133  tests/render/ControlsReview.qml:59,65,71  tests/render/DetailReview.qml:29  tests/render/OverviewVisual.qml:30  tests/unit/DisplayOutputProbe.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unqualified unknown grouped property scope anchors. (5) | qml/Yalahoo/Music/components/TitleReel.qml:99  qml/Yalahoo/Music/pages/MusicStage.qml:133  tests/render/DetailReview.qml:29  tests/render/OverviewVisual.qml:30  tests/unit/DisplayOutputProbe.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unqualified unknown grouped property scope font. (1) | qml/Yalahoo/Music/components/ArchiveComboBox.qml:20 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "BackgroundMusicController" of property "backgroundMusic" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (8) | qml/Yalahoo/Music/panels/LibraryPanels.qml:118  qml/Yalahoo/Music/panels/PanelContents.qml:68,92  qml/Yalahoo/Music/panels/PlaybackPanel.qml:71  qml/Yalahoo/Music/panels/SettingsPanel.qml:130  qml/Yalahoo/Music/panels/SoundSettings.qml:60,66,70 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "BackgroundMusicController" of property "backgroundMusicController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/App.qml:71  qml/Yalahoo/Music/pages/MusicStage.qml:239  qml/Yalahoo/Music/panels/StagePanels.qml:39 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "Browse" of property "browse" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/components/StageFocus.qml:25,43,51 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "DetailModel" of property "detailModel" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (17) | qml/Yalahoo/Music/App.qml:77  qml/Yalahoo/Music/components/TrackList.qml:43,89,149,164  qml/Yalahoo/Music/components/TrackRow.qml:23,35,37,98,147  qml/Yalahoo/Music/pages/Detail.qml:54,164,189  qml/Yalahoo/Music/pages/MusicStage.qml:211,217,219  tests/render/DetailReview.qml:33 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "DisplayOutput" of property "displayOutput" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (4) | qml/Yalahoo/Music/App.qml:43,53,59,60 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "DisplayOutput" of property "output" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (5) | qml/Yalahoo/Music/components/DisplaySurface.qml:8,9,14,15  tests/unit/DisplayOutputProbe.qml:16 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "FrameClock" of property "clock" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (27) | qml/Yalahoo/Music/components/BrowseInfo.qml:117,124,125,147  qml/Yalahoo/Music/components/ColumnNavigation.qml:57,73  qml/Yalahoo/Music/components/Header.qml:131  qml/Yalahoo/Music/components/HeaderTransport.qml:61,71  qml/Yalahoo/Music/components/NavigationFrame.qml:74,88,89,98,100,109,110,165  qml/Yalahoo/Music/components/OrdinalReel.qml:38  qml/Yalahoo/Music/components/TitleReel.qml:39  qml/Yalahoo/Music/components/TransportTitle.qml:55,69  qml/Yalahoo/Music/pages/Browse.qml:81,100,124  tools/migration/RulerReview.qml:43  tools/migration/TitleReview.qml:25,39 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "GenreRowsModel" of property "genreRows" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (1) | qml/Yalahoo/Music/panels/SearchFilters.qml:58 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "InputRouter" of property "inputRouter" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (23) | qml/Yalahoo/Music/App.qml:67  qml/Yalahoo/Music/components/ColumnNavigation.qml:23,31,59,102  qml/Yalahoo/Music/components/NavigationFrame.qml:91,112,126,145,158,164,172  qml/Yalahoo/Music/components/StageFrameDriver.qml:34  qml/Yalahoo/Music/pages/Browse.qml:101,123  qml/Yalahoo/Music/pages/MusicStage.qml:77,108,152,183,205  qml/Yalahoo/Music/pages/Overview.qml:132,154  tests/render/OverviewVisual.qml:32 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "InterfaceSoundController" of property "interfaceSound" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (10) | qml/Yalahoo/Music/panels/InterfaceSoundPreview.qml:10,11  qml/Yalahoo/Music/panels/LibraryPanels.qml:119  qml/Yalahoo/Music/panels/PanelContents.qml:69,93  qml/Yalahoo/Music/panels/PlaybackPanel.qml:72  qml/Yalahoo/Music/panels/SettingsPanel.qml:131  qml/Yalahoo/Music/panels/SoundSettings.qml:48,54,77 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "InterfaceSoundController" of property "interfaceSoundController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/App.qml:72  qml/Yalahoo/Music/pages/MusicStage.qml:240  qml/Yalahoo/Music/panels/StagePanels.qml:40 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "LayoutMetrics" of property "metrics" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (110) | qml/Yalahoo/Music/components/BrowseInfo.qml:17,20,21,22,24,27,28,29,30,32,36,38,39,41,59,60,61,65,105,106,107,108,109,110,118,151,154,155,166,167,172,182,188,195,197,198,199  qml/Yalahoo/Music/components/Footer.qml:11,12,13,21  qml/Yalahoo/Music/components/Header.qml:33,34,37,40,44,45,46,52,68,70,82,84,85,86,93,94,96,115,116,123,132,134  qml/Yalahoo/Music/components/NavigationFrame.qml:21,22,25,26,41,42,46,65,104,113,115,120,122,123,133,142,155,166,181  qml/Yalahoo/Music/components/ReadingVeil.qml:18,30,54,146,148,150  qml/Yalahoo/Music/pages/Browse.qml:44,53,54,78,96,110,118  qml/Yalahoo/Music/pages/Detail.qml:55,58,61,103,122,129,143,144,145,153,173,175,204  tests/render/DetailReview.qml:30  tools/migration/TitleReview.qml:37 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "LibrarySession" of property "librarySession" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (54) | qml/Yalahoo/Music/App.qml:62  qml/Yalahoo/Music/pages/MusicStage.qml:179,180,203,220,221,225,235  qml/Yalahoo/Music/panels/LibraryPanel.qml:112,129,133,137  qml/Yalahoo/Music/panels/LibraryPanels.qml:114  qml/Yalahoo/Music/panels/MetadataSection.qml:13,14,15,16,17,66,67,68,88,89,117,118,144,145  qml/Yalahoo/Music/panels/MetadataSettings.qml:26,36,52,72,87,90,104,117,118  qml/Yalahoo/Music/panels/PanelContents.qml:58,89,122  qml/Yalahoo/Music/panels/SearchPanel.qml:14,26,31,53,60,65,68,80,82,97,117  qml/Yalahoo/Music/panels/SettingsPanel.qml:64,76,135  qml/Yalahoo/Music/panels/StagePanels.qml:35 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "LibraryStore" of property "libraryStore" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/App.qml:76  qml/Yalahoo/Music/pages/MusicStage.qml:242  qml/Yalahoo/Music/panels/StagePanels.qml:60 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "LibraryStore" of property "store" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (32) | qml/Yalahoo/Music/panels/LibraryPanel.qml:18,24,73,80,81,86,87,92,93,103,104,105,107,109,111,113  qml/Yalahoo/Music/panels/LibraryPanels.qml:121  qml/Yalahoo/Music/panels/MetadataSettings.qml:37,38,53,54,73,74,118,119,120,128,130,141  qml/Yalahoo/Music/panels/PanelContents.qml:90,123  qml/Yalahoo/Music/panels/SettingsPanel.qml:136 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "MotionController" of property "motionController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (16) | qml/Yalahoo/Music/App.qml:63  qml/Yalahoo/Music/components/StageFrameDriver.qml:21,25,27,30,36,48,49,55,56,57  qml/Yalahoo/Music/pages/MusicStage.qml:88,89,91,107,184 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "Opening" of property "openingOverlay" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (2) | qml/Yalahoo/Music/components/StageFocus.qml:35,36 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OpeningController" of property "controller" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (23) | qml/Yalahoo/Music/components/OpeningFilm.qml:14,26,32,54,55,57,76,82,83,100  qml/Yalahoo/Music/pages/Opening.qml:16,20,31,34,41,47,52,109,110,111,112,113,118 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OpeningController" of property "opening" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (8) | qml/Yalahoo/Music/components/Header.qml:91,92  qml/Yalahoo/Music/pages/Browse.qml:28,31,61,66,80,91 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OpeningController" of property "openingController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (26) | qml/Yalahoo/Music/App.qml:66  qml/Yalahoo/Music/components/StageFocus.qml:29  qml/Yalahoo/Music/components/StageFrameDriver.qml:53,54  qml/Yalahoo/Music/pages/MusicStage.qml:76,98,112,131,132,136,138,139,149,150,161,164,174,175,243  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:37  qml/Yalahoo/Music/panels/StagePanels.qml:48,49,69,100,101,106 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OverviewController" of property "controller" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (17) | qml/Yalahoo/Music/components/OverviewLabel.qml:88,135,152,175  qml/Yalahoo/Music/pages/Overview.qml:55,59,60,86,88,104,105,106,111,122,124,125,164 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OverviewController" of property "overview" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (1) | tests/render/OverviewVisual.qml:31 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "OverviewController" of property "overviewController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (20) | qml/Yalahoo/Music/App.qml:78  qml/Yalahoo/Music/components/Header.qml:25,27,29  qml/Yalahoo/Music/components/StageFocus.qml:48  qml/Yalahoo/Music/pages/Browse.qml:26,27,54,55,61,62,63,64,65,67,84  qml/Yalahoo/Music/pages/MusicStage.qml:100,181,198,202 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PanelController" of property "controller" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (19) | qml/Yalahoo/Music/panels/LibraryPanels.qml:37,44,48,94,115,129,130,133  qml/Yalahoo/Music/panels/PanelContents.qml:24,26,47,59,64,84,110,126,128,130,132 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PanelController" of property "panelController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (20) | qml/Yalahoo/Music/App.qml:75,92,93  qml/Yalahoo/Music/pages/MusicStage.qml:52,53,57,58,75,137,140,236  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:38  qml/Yalahoo/Music/panels/StagePanels.qml:32,36,43,47,70,100,102,107 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PlaybackController" of property "playback" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (43) | qml/Yalahoo/Music/components/PlaybackControls.qml:12,13,26,27,33,34,45,46,47,51,54,56,60,62,70,71  qml/Yalahoo/Music/panels/LibraryPanels.qml:117  qml/Yalahoo/Music/panels/PanelContents.qml:64,67,91  qml/Yalahoo/Music/panels/PlaybackPanel.qml:29,36,39,40,42,52,59,60,65,70,77,87,94,99,104,105  qml/Yalahoo/Music/panels/SettingsPanel.qml:129  qml/Yalahoo/Music/panels/SoundSettings.qml:42,83,84,85,87,88 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PlaybackController" of property "playbackController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (15) | qml/Yalahoo/Music/App.qml:70  qml/Yalahoo/Music/pages/MusicStage.qml:44,45,48,49,74,78,176,177,178,224,225,238  qml/Yalahoo/Music/panels/StagePanels.qml:38,81 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PresentationController" of property "presentation" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (25) | qml/Yalahoo/Music/components/DetailAlbumInfo.qml:49,50,58,59  qml/Yalahoo/Music/components/DetailHeader.qml:22,36  qml/Yalahoo/Music/components/DetailTabs.qml:117,118,122,125,135  qml/Yalahoo/Music/pages/Detail.qml:36,40,52,65,66,67,76,80,169,171,184,198  tests/render/DetailReview.qml:32,34 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "PresentationController" of property "presentationController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (22) | qml/Yalahoo/Music/App.qml:69  qml/Yalahoo/Music/components/StageFocus.qml:19,55,57  qml/Yalahoo/Music/components/StageFrameDriver.qml:51,52  qml/Yalahoo/Music/pages/MusicStage.qml:65,66,99,111,153,154,185,187,188,211,216,222,244  qml/Yalahoo/Music/panels/StagePanels.qml:108,109,110 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "ReelSlot" of property "slot" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (19) | qml/Yalahoo/Music/components/TitleReel.qml:73,74,75,76,77,78,91,92,93,94,100,101,102,105,109,110,111,112,125 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "ReelSlot" of property "slotObject" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (1) | qml/Yalahoo/Music/components/TitleReel.qml:72 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "RulerController" of property "controller" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (14) | qml/Yalahoo/Music/components/Ruler.qml:11,12,17,19,33,34,36,37,52,53,59,71  tools/migration/RulerReview.qml:35,47 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "RulerController" of property "rulerController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (10) | qml/Yalahoo/Music/App.qml:68  qml/Yalahoo/Music/components/NavigationFrame.qml:126,132,148,179,182,185,189  qml/Yalahoo/Music/pages/Browse.qml:122  qml/Yalahoo/Music/pages/MusicStage.qml:182 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SceneAdapter" of property "adapter" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (10) | qml/Yalahoo/Music/scene/GeometryScene.qml:14,15,23,44,57,58,59,61,62,66 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SceneAdapter" of property "sceneAdapter" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (15) | qml/Yalahoo/Music/App.qml:44,54,64  qml/Yalahoo/Music/components/StageFrameDriver.qml:19  qml/Yalahoo/Music/pages/MusicStage.qml:87,106,134,172,189,192,193,214,252,253,254 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SceneItem" of property "sceneSource" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (6) | qml/Yalahoo/Music/panels/ExperimentOverlay.qml:44  qml/Yalahoo/Music/panels/ExperimentPanels.qml:68  qml/Yalahoo/Music/panels/StagePanels.qml:68,71,118,123 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SelectionController" of property "selection" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/components/NavigationFrame.qml:144,157  qml/Yalahoo/Music/pages/Browse.qml:125 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SelectionLighting" of property "lighting" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (34) | qml/Yalahoo/Music/panels/ExperimentOverlay.qml:43  qml/Yalahoo/Music/panels/ExperimentPanels.qml:54  qml/Yalahoo/Music/panels/LightingPanel.qml:13,16,21,59,124,148,160  qml/Yalahoo/Music/scene/CaseMaterials.qml:10,40,61,65  qml/Yalahoo/Music/scene/GlassMaterial.qml:32,34  qml/Yalahoo/Music/scene/LightingMaterial.qml:8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SettingsController" of property "settings" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (31) | qml/Yalahoo/Music/panels/AppearanceSettings.qml:33,34,42,43,73,74  qml/Yalahoo/Music/panels/ArrayQualitySettings.qml:33,34,40,41,69,70  qml/Yalahoo/Music/panels/ExperimentPanels.qml:48  qml/Yalahoo/Music/panels/LibraryPanels.qml:120  qml/Yalahoo/Music/panels/MotionPanel.qml:28,41  qml/Yalahoo/Music/panels/MotionSpeedRange.qml:10,11,12,13,14  qml/Yalahoo/Music/panels/PanelContents.qml:84,88  qml/Yalahoo/Music/panels/SettingsPanel.qml:42,43,56,87,101,102,107,124 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SettingsController" of property "settingsController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (8) | qml/Yalahoo/Music/App.qml:74,90  qml/Yalahoo/Music/pages/MusicStage.qml:53,241  qml/Yalahoo/Music/panels/ExperimentOverlay.qml:36,42  qml/Yalahoo/Music/panels/StagePanels.qml:41,67 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "StartupCoordinator" of property "startupCoordinator" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (7) | qml/Yalahoo/Music/App.qml:65  qml/Yalahoo/Music/components/StageFrameDriver.qml:40,43  qml/Yalahoo/Music/pages/MusicStage.qml:109,117,147,148 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "SurfaceTransition" of property "surface" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (2) | qml/Yalahoo/Music/pages/Browse.qml:28,31 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TitleReelMotion" of property "controller" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (10) | qml/Yalahoo/Music/components/BrowseInfo.qml:19  qml/Yalahoo/Music/components/TitleReel.qml:21,22,68,85,86,90  qml/Yalahoo/Music/components/TransportTitle.qml:18,81  tools/migration/TitleReview.qml:30 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TitleReelMotion" of property "reelController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (1) | qml/Yalahoo/Music/components/HeaderTransport.qml:18 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TitleReelMotion" of property "sharedController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (2) | qml/Yalahoo/Music/components/TitleReel.qml:20,50 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TitleReelMotion" of property "titleController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (2) | qml/Yalahoo/Music/pages/Browse.qml:34  qml/Yalahoo/Music/pages/MusicStage.qml:155 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TitleReelMotion" of property "transportController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (3) | qml/Yalahoo/Music/components/Header.qml:30  qml/Yalahoo/Music/pages/Browse.qml:35  qml/Yalahoo/Music/pages/MusicStage.qml:156 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TrackFocusController" of property "trackFocus" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (29) | qml/Yalahoo/Music/components/TrackList.qml:88,95,96,98,100,102,165  qml/Yalahoo/Music/components/TrackListReveal.qml:39,64,68,72,75,78,83,86,96,101  qml/Yalahoo/Music/components/TrackRow.qml:58,62,71,72  qml/Yalahoo/Music/pages/Detail.qml:33,34,44,94,165  qml/Yalahoo/Music/panels/LibraryPanels.qml:116  qml/Yalahoo/Music/panels/PanelContents.qml:129,138 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type "TrackFocusController" of property "trackFocusController" not found. This is likely due to a missing dependency entry or a type not being exposed declaratively. (11) | qml/Yalahoo/Music/App.qml:73  qml/Yalahoo/Music/components/StageFrameDriver.qml:49,50  qml/Yalahoo/Music/pages/MusicStage.qml:61,62,110,218,237  qml/Yalahoo/Music/panels/StagePanels.qml:32,37,90 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type anchors is used but it is not resolved (5) | qml/Yalahoo/Music/components/TitleReel.qml:99  qml/Yalahoo/Music/pages/MusicStage.qml:133  tests/render/DetailReview.qml:29  tests/render/OverviewVisual.qml:30  tests/unit/DisplayOutputProbe.qml:15 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |
| QL:unresolved-type Type font is used but it is not resolved (1) | qml/Yalahoo/Music/components/ArchiveComboBox.qml:20 | 使用项目生成的模块/资源上下文核对类型；对未编入当前目标的研究入口单独核对，避免依据缺导入级联直接改动实现。 |

本报告只提交审查结论与必要源码定位；未附原始日志、截图、曲库、机器配置或批量验证产物。
