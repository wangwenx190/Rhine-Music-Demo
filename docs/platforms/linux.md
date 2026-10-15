# Linux开发与部署操作

本文只管Linux工具/平台插件/启动差异；通用构建见[native-bootstrap.md](../contracts/native-bootstrap.md)，组件入口见迁移计划职责索引，未完成验收/门槛统一在R34。当前缺目标系统的项目仍未执行，不以其他平台结果代替。

## 开发环境与启动

安装本机原生C++23编译器、CMake/Ninja、formatter及最新完整Qt Desktop Kit；标准根由Kit工具发现，自定义根用YALAHOO_QT_ROOTS，系统发行版非标准布局须提供完整CMake/mkspec/qmake前缀。

构建：`sh tools/build/build.sh qt-release`；升级Kit移除过期覆盖后用`--fresh`。不要重复执行相同配置冒充双配置或平台验收。

用户在对应真实会话中主动运行：

```sh
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' build/release/CMakeCache.txt)
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=xcb build/release/bin/yalahoo
QT_PLUGIN_PATH="$kit/plugins" QT_QPA_PLATFORM=wayland build/release/bin/yalahoo
```

xcb与Wayland分别在真实可用会话验证；未具会话不伪称通过。普通验证窗口遵守test-window-policy.md，不能为采集提高层级。

## 平台差异

- 默认OpenGL，可选Vulkan需能力探测/回退；平台插件及媒体共享库依赖按实际Kit确认，不照搬Windows DLL清单。
- 路径区分大小写，处理Unicode/空格/权限/XDG数据根；根.sh从自身位置解析产物、正确引用参数、LF及Git可执行位。
- Fontconfig选择必须匹配同源MiSans物理表；设备消失、不可seek、格式/权限错误由公开Qt API处理，不静默修改系统音频/驱动配置。
- 独立部署库/插件/QML/CanvasPainter/媒体闭包按R32实施；普通开发Kit启动不证明部署完成。组件矩阵、物理输入、音频设备和性能执行要求只在R34保留。
