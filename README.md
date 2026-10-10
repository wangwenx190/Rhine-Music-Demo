# yalahOO!!! · Qt Quick / QML

**yalahOO!!!** 按 [原生迁移计划](QT_QUICK_MIGRATION_PLAN.md) 改造为 C++23、Qt Quick/QML 跨平台音乐播放器，Qt 使用开发机安装的最新版本，构建与脚本不锁定具体版本或安装路径。GUI、动画与特效尽最大可能逼近原 Web 版本，音乐播放使用 QtMultimedia 公开 API；联网功能暂缓，保留接口与占位。

品牌图标与 Logo 将在计划 R03B 原创设计，以 SVG 为主素材，围绕音乐收藏、实体专辑盒与双 O 唱片圆环，提供适配暖昼/深夜和桌面系统的版本；品牌替换与其他界面的复刻分开验收。

迁移轮次尚待执行；本页列出目标结构，不表示 Qt 应用已经具备可运行产物。计划是项目内最高优先级的实施契约，开发约定见 [AGENTS.md](AGENTS.md)。

- `src/`：C++ 应用、模型、运动、渲染、媒体与平台代码。
- `qml/Yalahoo/Music/`：界面与场景 QML 模块。
- `resources/`、`tests/`：原生资产和 Qt 测试。
- `cmake/`、`tools/`：构建、部署、审阅和静态审计工具。
- `docs/`、`verification/`：长期契约、平台说明和验收证据。
- [legacy-web/](legacy-web/)：原项目全部源码、资源、文档和启动器。旧版使用说明见 [归档 README](legacy-web/README.md)，Web 参照命令须在该目录运行。

Windows 必须严格实机验证。macOS、Linux 必须完整实现代码及构建/部署方案，本阶段通过严格静态审计即可验收；实际环境构建、GUI 和音频验证以后由用户执行，结果须明确区分。

根 CMake 和 presets 在计划 R02 建立，后续统一使用 `qt-debug`、`qt-release`，构建输出放 `build/`，部署输出放 `out/`。具体命令与 Qt Kit 配置以计划及届时的构建说明为准。

音乐适配与维护：RonaldDeng；基于 LBEILC / RhineLabUI。原有 [LICENSE](legacy-web/LICENSE)、[NOTICE](legacy-web/NOTICE.md) 与素材来源说明保留在归档目录。
