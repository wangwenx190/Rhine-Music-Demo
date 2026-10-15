# macOS开发与部署操作

本文只管macOS工具/应用入口/系统差异；通用构建见[native-bootstrap.md](../contracts/native-bootstrap.md)，组件入口见迁移计划职责索引，未完成验收/门槛统一在R34。运行记录不在此按轮次追加。

## 开发环境

- Apple Silicon使用原生arm64终端，Universal Kit须含arm64；不要复制其他机器的build/cache/UserPresets/Kit/node_modules/私人数据。克隆保留冻结源需要的Git历史，不用浅克隆。
- 安装支持项目C++23探针的Xcode/Command Line Tools、CMake/Ninja、现代clang-format及本机最新完整Qt Desktop Kit（含CanvasPainter/private headers）。自定义根YALAHOO_QT_ROOTS；formatter本机cache变量YALAHOO_CLANG_FORMAT，不写公共路径。
- 配置/构建：`sh tools/build/build.sh qt-release --fresh -DYALAHOO_REVIEW=OFF`；产品为`build/release/bin/yalahoo.app`。公共格式/测试命令只在对应契约维护。

## 用户主动运行

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/release/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" build/release/bin/yalahoo.app/Contents/MacOS/yalahoo \
    --data-dir "$PWD/verification/macos/manual-data"
```

这是开发Kit运行，不代表独立部署包。bundle/Info.plist、Frameworks/PlugIns/qml与macdeployqt脱离Kit检查按R31实施；普通.command/菜单/后台/媒体按对应轮次，不能提前宣称完成。

## 图形与验证差异

- 默认Metal/threaded；显式OpenGL在QGuiApplication前选择4.1/CoreProfile、可见drawable用basic主线程。多采样纹理入口缺失时同格式/样本renderbuffer+resolve，不降画质。两种线程模式成本分开。
- 普通验证遵守[test-window-policy.md](../contracts/test-window-policy.md)，macOS默认可见OFF，RenderControl真正隐藏。用户指定可见轮次授权不跨session；前台/窗口/焦点只能由该契约入口选择。
- DPR从`ui-audit --native-dpr`及实际窗口报告核对；有效缩放不等于换物理显示器。字体面与CoreText差异见assets/title-reels，不能改入库字体匹配平台报告值。
- 本机已有Metal/OpenGL隐藏和授权可见组件运行事实，不得把整个macOS写成仅静态；正常VSYNC、物理设备、完整产品与部署的欠验以R34具体任务为准。
