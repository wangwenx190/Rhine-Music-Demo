# yalahOO!!! · Qt Quick / QML

**yalahOO!!!** 按 [原生迁移计划](QT_QUICK_MIGRATION_PLAN.md) 改造为 C++23、Qt Quick/QML 跨平台音乐播放器，Qt 使用开发机安装的最新版本，构建与脚本不锁定具体版本或安装路径。GUI、动画与特效尽最大可能逼近原 Web 版本，以冻结旧源码为准；截图和各类采集数据仅作参考，渲染缺陷（包括可见锯齿）应改善，音乐播放使用 QtMultimedia 公开 API；联网功能暂缓，保留接口与占位。

原创品牌 SVG、双主题字标、小尺寸图标与平台衍生文件已保存，规范和预览见 [branding.md](docs/contracts/branding.md)。Qt 默认无窗口导出命令为 `./tools/assets/export-branding.ps1 -Preset qt-release`，原生 paint 检查才显式加 `-WindowPreview`，仍置底/不接受焦点；页眉/开场接入归 R14/R20，系统集成归 R30–R32。品牌替换与其他界面复刻分开验收。

当前已有 Qt 原生运行探针和完整 qrc 资产接入：原专辑盒三壳体、四字重 MiSans、八个功能图标双主题、现有完整音频与演示封面。单盒已接入主盖、侧脊、后托自定义材质与独立漫反射纸面，音乐播放器功能按后续轮次接入。[资源契约](docs/contracts/assets.md)包含来源/hash、字体权重映射、准备与验证命令；[原生工程契约](docs/contracts/native-bootstrap.md)包含 Kit 发现和构建入口。可复建的匿名 Web 参照见 [冻结基线契约](docs/contracts/web-baseline.md)。计划是项目内最高优先级的实施契约，开发约定见 [AGENTS.md](AGENTS.md)。

- `src/`：C++ 应用、模型、运动、渲染、媒体与平台代码。
- `qml/Yalahoo/Music/`：界面与场景 QML 模块。
- `resources/`、`tests/`：原生资产和 Qt 测试。
- `cmake/`、`tools/`：构建、部署、审阅和静态审计工具。
- `docs/`、`verification/`：长期契约、平台说明和验收证据。
- [legacy-web/](legacy-web/)：原项目全部源码、资源、文档和启动器。旧版使用说明见 [归档 README](legacy-web/README.md)，Web 参照命令须在该目录运行。

当前主力开发与验证环境为 macOS，Windows/macOS/Linux 均须完整实现。每轮按实际环境、后端、设备和采集入口判断：能可靠验证的真实执行，当前无法执行或可靠验证的部分暂用严格静态审计，原实机要求和已知补修完整保留到最后 R34。当前主线按计划顺序连续推进，不等待已移交 R34 的 D3D11 等专项，但不能跳过当前可执行的前序任务。

常规构建、测试、GUI/性能验证与验收只使用 Release。Windows 运行 `./tools/build/build.ps1`（默认 `qt-release`），自动导入 MSVC 环境、发现最新 Qt Kit 并执行 configure/build/CTest。macOS/Linux 使用 `sh tools/build/build.sh qt-release`；当前macOS优先真正离屏验证，默认暂缓尚无可靠非激活显示入口的可见窗口CTest（`YALAHOO_MACOS_VISIBLE_TESTS=OFF`）；本机arm64 Release与Metal二维离屏已实测，不能将整个macOS写成仅静态通过。用户明确要求可见验证即授权指定轮次前置/激活/焦点，复现及恢复默认见[测试窗口契约](docs/contracts/test-window-policy.md)。窗口/VSYNC/媒体设备和其他平台检查按实际可靠能力执行或移交R34；纯静态项标明“静态审计通过，实机待验证（R34）”。清单见 [macOS](docs/platforms/macos.md) / [Linux](docs/platforms/linux.md)。保留 `qt-debug`，但只有明确需要获取堆栈时才编译和运行，不执行例行双配置验证。构建输出在 `build/`，未来部署输出在 `out/`；当前依赖开发 Kit，完整播放器及独立部署包尚未接入。

数据模型、排序列、冻结规则搜索和可注入的异步离线资料 Provider 已接入；导航 ID 与播放 ID 独立，默认不读取真实曲库。接口、模型角色与 Release 验证命令见 [数据契约](docs/contracts/library-data.md)。

生产场景已接入 9×48 填充池、独立真实列、波浪、鼠标拾取/键盘/滚轮/触摸导航、选中/归位接管和 rebase；普通入口使用 27 张内置资源演示专辑，不扫描私人曲库。固定池与 256/1024 封面、上传/资源预算、来源对照和平台审计见 [填充池契约](docs/contracts/archive-pool.md)。探针可切换真实/填充模式和会话列记忆；真实列首尾停止，未选列居中，关闭记忆后从目标列实际深度选择。仅偏好保存在原生数据根的 preferences.json，`--data-dir` 可覆盖，会话行位不持久化。规则和复建入口见 [真实列契约](docs/contracts/real-columns.md)及 `./tools/build/verify-r13.ps1`。二维页眉、原创 Logo、双主题 Basic 控件、浏览静态信息和页脚已接入；导航滑尺/字轮与详情状态机、虚拟化歌单、介绍/本地备注和制作人员静态界面已接入；详情同步抽取/换片、扫描与普通播放依后续轮次接入。详情的状态顺序、Release 验证和待执行实机范围见[Presentation 契约](docs/contracts/presentation.md)。绘制能力、同输入对照和重建入口见 [二维绘制契约](docs/contracts/canvas-painter.md) 与 `tools/build/verify-r14.ps1`。
所有后续轮次强制使用 [Qt 源码风格](docs/contracts/code-style.md)：四空格缩进、展开短语句。提交前执行 `cmake --build --preset qt-release --target format` 与 `check-format`；Release CTest 包含 `source-format` 门槛。维护 PowerShell 的入口为 `./tools/format/FormatTools.ps1`（检查使用 `-Check`）。

所有 session 的测试和采集遵守 [测试窗口契约](docs/contracts/test-window-policy.md)：无需显示的保持隐藏，必须显示的默认置底且不接受焦点，禁止自动置前或抢焦点。统一窗口 helper 和首次显示前的 QML 注入同时覆盖运行、运动、场景、材质、RHI 和品牌检查；采集自身渲染，不截取用户桌面。

单一 QML 帧源、临界阻尼解析解、保留 x/v/a 的五次运动和独立开场计时已接入；改速、反向、坐标平移、减少动态与隐藏恢复通过冻结数值对照和 Windows 运行验证。接口与证据重建见 [运动契约](docs/contracts/motion.md)。单盒无光几何、共用布局指标与原长焦相机已接入，浏览/详情、横竖屏分界及九尺寸投影通过独立冻结源码和 Windows GPU 对照；接口与精确 Review 验证见 [场景契约](docs/contracts/scene.md)。单盒材质、双主题参数、局部纸面 LOD、线性透射捕获与精确 sRGB 输出已接入，90 个逐层对照和漫反射探针见 [材质契约](docs/contracts/materials.md)。世界空间光带、选中光场、原环境/半球/方向/面光及650ms可打断主题轨迹已接入，契约与复建见 [灯光契约](docs/contracts/lighting.md)。阴影/SSAO、41 点景深、实际几何 MSAA、高/极高 SMAA 和一次 ACES/sRGB 输出已接入，105 个 Windows 状态与局部边缘对照见 [后处理契约](docs/contracts/postprocess.md)。透射纸面边界及邻盒的具体残差保留为 R10 输入；阵列和完整产品动画按后续轮次接入。

音乐适配与维护：RonaldDeng；基于 LBEILC / RhineLabUI。原有 [LICENSE](legacy-web/LICENSE)、[NOTICE](legacy-web/NOTICE.md) 与素材来源说明保留在归档目录。

RHI Review 已具备独立不透明捕获、4× MSAA resolve/mips、磨砂壳体与全局稳定深度排序，保持源屏幕空间透射和三层壳体深度规则；契约与 Windows/静态审计复建见 [RHI 材质契约](docs/contracts/rhi-shell.md)。五区域的全部残差与同输入对照保留，不能把平均误差降低表述为全部图像目标通过。RHI 图集局部上传、432 实例和 AO/景深/SMAA/输出见 [图集与后处理契约](docs/contracts/rhi-postprocess.md)，当前复建入口为 `tools/build/verify-r10c.ps1`。普通入口现为唯一 RHI SceneItem，Quick3D oracle 只在 Review 构建打包。生产集成、资源清单、正常 VSYNC 的输入/帧样本及 D3D11/OpenGL 实机验证见 [RHI 集成契约](docs/contracts/rhi-integration.md)，本轮证据重建入口为 `tools/build/verify-r10d.ps1`。

性能采用[设计、审计与验证契约](docs/contracts/performance.md)：先检查算法复杂度、线程/队列、快照/增量和渲染资源失效，再用分场景Release实测确认收益。RHI未限速Review、产品正常VSYNC节奏、输入响应和资源峰值分别报告，不能以单个benchmark宣称完整体验达标。

异步封面解码、16MiB 完成缓存、固定分页图集及独立选中/归位租约见[封面契约](docs/contracts/covers.md)。生产演示封面从 256 升级到 1024；源尺寸、队列、在途像素与上传分别受限，稳定帧零图像上传，复用槽位在新租约上传前保持隐藏。Windows 复建为 `tools/build/verify-r11.ps1` 与 `tools/audit/audit-r11.ps1`；既有macOS/Linux静态结论只代表对应历史轮次，后续按当前能力记录真实覆盖，未执行实机任务保留到R34。
