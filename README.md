# yalahOO!!!

C++23 / Qt Quick / QML 原生音乐收藏播放器，处于迁移开发阶段。已接入浏览、详情、总览、双主题、原生RHI场景、本地扫描、普通歌曲播放、二维序章和设置；Windows生产部署脚本已接入，其目标实机验证及macOS/Linux独立部署按剩余计划执行；联网暂缓。普通入口使用本机原生数据，匿名演示与光照/速度研究入口的参数和启动器见[原生工程契约](docs/contracts/native-bootstrap.md)。

## 构建与运行

- Windows：`./tools/build/build.ps1 -Preset qt-release`。
- macOS/Linux：`sh tools/build/build.sh qt-release`。
- 使用本机最新原生Qt Kit；工具链配置、输出路径与开发运行见[原生工程契约](docs/contracts/native-bootstrap.md)。

## 文档入口

- [迁移计划](QT_QUICK_MIGRATION_PLAN.md)：优先级、固定取舍、剩余轮次、经验和R34欠验清单；内含专项文档职责索引。
- [开发约定](AGENTS.md)：代理执行入口。
- [测试门禁](docs/contracts/test-gates.md)：快速、隐藏完整和显式窗口测试；[窗口策略](docs/contracts/test-window-policy.md)约束采集行为。
- [Windows](docs/platforms/windows.md) / [macOS](docs/platforms/macos.md) / [Linux](docs/platforms/linux.md)：平台操作差异。
- [legacy-web](legacy-web/README.md)：冻结Web源码、资源、历史资料与旧启动器；旧命令在该目录运行。

C++在`src/`，QML在`qml/Yalahoo/Music/`，产品资产在`resources/`，测试在`tests/`，工具在`tools/`。构建与本地验证输出不提交，详见[产物边界](docs/contracts/verification-artifacts.md)。

音乐适配与维护：RonaldDeng；基于LBEILC / RhineLabUI。原[LICENSE](legacy-web/LICENSE)、[NOTICE](legacy-web/NOTICE.md)及素材署名保留。

Windows生产包使用`pwsh -NoProfile -File tools/deploy/windows.ps1`生成，构建、运行库、脱离Kit检查与用户清单见[Windows平台操作](docs/platforms/windows.md)。
