# yalahOO!!!

C++23 / Qt Quick / QML 原生音乐收藏播放器。普通入口使用本机音乐库，提供收藏架浏览、详情、总览、本地扫描、普通歌曲播放、开场与设置。支持按需查询 MusicBrainz 制作信息与资料、Wikipedia/Wikidata 专辑介绍，结果可离线查看。

## 构建、启动与部署

开发环境需要 CMake/Ninja、支持 C++23 的原生编译器和本机最新完整 Qt Desktop Kit。安装与 Kit 发现规则见[原生工程契约](docs/contracts/native-bootstrap.md)，格式与检查见[代码风格](docs/contracts/code-style.md)和[测试门禁](docs/contracts/test-gates.md)。

| 平台 | Release 产品构建 | 普通启动 | 独立部署 |
| --- | --- | --- | --- |
| Windows / PowerShell 7.2+ | `./tools/build/build.ps1 -Preset qt-release -Action Build` | `./yalahoo.ps1` | `./tools/deploy/windows.ps1` |
| macOS | `sh tools/build/macos.sh -DYALAHOO_REVIEW=OFF` | `./yalahoo.command` | `sh tools/deploy/macos.sh` |
| Linux | `sh tools/build/linux.sh -DYALAHOO_REVIEW=OFF` | `./yalahoo.sh` | `sh tools/deploy/linux.sh` |

Windows 构建前先执行 `. ./tools/build/enter-msvc.ps1`，再执行 `cmake --preset qt-release -DYALAHOO_REVIEW=OFF`。

根启动器从自身位置解析路径，优先使用完整的 `out/<平台>/` 部署包，否则使用 `build/release/` 开发产物；参数原样传递。部署工具只生成本地包。各平台工具、包外验证、系统依赖与操作清单见 [Windows](docs/platforms/windows.md)、[macOS](docs/platforms/macos.md)、[Linux](docs/platforms/linux.md)。普通应用运行不需要 Node、浏览器、Python、Swift 或外部 FFmpeg。

匿名演示使用 `--demo`，光照与速度实验使用 `--lighting-lab`；也可使用 `tools/launchers/` 下对应的 demo/lighting-lab 启动器。两者只使用嵌入的匿名专辑，不读取个人库或偏好、不扫描、不自动播放音频；不能与数据或验证参数组合。

## 本地音乐与旧数据导入

首次启动为空库，可在音乐库面板选择目录并扫描，或传入绝对路径（`--music-root` 可重复）：

```sh
./yalahoo.sh --music-root "/绝对路径/音乐"
```

Windows 使用 `./yalahoo.ps1`，macOS 也可使用 `./yalahoo.command` 传递同样参数。`--rescan-library` 重扫已保存目录；`--data-dir /绝对路径/数据目录` 指定独立的原生缓存与偏好目录，省略时使用系统的应用本地数据位置。

旧 Web 数据只在明确指定时读取。将以下路径替换为本机绝对路径；新旧数据目录必须分离，导入不会改写旧目录或歌曲：

```sh
./yalahoo.sh --data-dir "/绝对路径/原生数据" \
    --import-legacy "/绝对路径/music-data-v3"
./yalahoo.sh --data-dir "/绝对路径/原生数据" \
    --import-preferences "/绝对路径/浏览器导出.json"
```

曲库导入读取旧索引、配置、人工流派与引用的封面；偏好导入只读取明确导出的 JSON，不读取浏览器数据库。扫描与导入分别执行；跨系统路径需要在目标系统重新配置。身份、数据格式、缓存保护和导入细则以[数据契约](docs/contracts/library-data.md)为准。

## 离线功能与边界

| 功能 | 当前行为 |
| --- | --- |
| 收藏架 | 真实列/填充阵列、列记忆、排序、搜索、滑尺导航、详情抽取/拖动/归位与总览定位；浏览和定位独立于播放。 |
| 播放 | 本地歌曲播放/暂停/停止、上下曲、专辑队列、进度、音量、输出设备与错误提示；歌曲、氛围音乐及界面音效分别控制。 |
| 资料 | 本地标签/封面；MusicBrainz 资料与制作信息、Wikipedia/Wikidata 介绍及来源缓存。高级设置开启联网后可查询，支持独立自动查询开关和 HTTP 代理。 |
| 外观与设置 | 双主题、减少动态、0.25×–3×动效速度、画质/光效、开场重播与视觉重载；动效速度不改变歌曲速度。 |
| 声音资产 | 已有氛围音乐；界面事件尚缺独立短音效资产，因此事件与试听保持静音。 |

可播放格式取决于当前 QtMultimedia 后端、平台和文件本身；被扫描或读到标签不代表可解码播放。实际格式错误会显示 Qt 原因。普通播放与设备行为见[播放契约](docs/contracts/playback.md)。

原生场景使用 RHI，保留源景深、磨砂、玻璃、阴影和后处理。当前仍需完成的目标系统、物理输入/音频、跨屏、实际呈现和性能检查，以及具体失败或能力缺口，仅在[迁移计划](QT_QUICK_MIGRATION_PLAN.md)维护。离屏输出、Qt 呈现事件与屏幕显示是不同测量范围；不能据组件结果承诺所有设备的完整体验。冻结源码自身的外观例外见[来源例外契约](docs/contracts/source-defect-exceptions.md)。

## 开发与来源

C++ 在 `src/`，QML 在 `qml/Yalahoo/Music/`，产品资产在 `resources/`，测试在 `tests/`，工具在 `tools/`。冻结 Web 源码、基线和研究资料隔离于 [legacy-web](legacy-web/README.md)；其开发命令在该目录运行。模块来源与原生归属见[源码映射](docs/contracts/source-map.json)，专项技术职责由迁移计划索引确定，[AGENTS.md](AGENTS.md)提供代理入口。

构建、部署、本地验证输出及个人数据不入 Git，边界见[验证产物契约](docs/contracts/verification-artifacts.md)。

音乐适配与维护：RonaldDeng；基于 LBEILC / RhineLabUI。原 [LICENSE](legacy-web/LICENSE)、[NOTICE](legacy-web/NOTICE.md)及[资源署名](resources/licenses/)保留。
