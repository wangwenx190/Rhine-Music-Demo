# yalahOO!!! · 原生项目开发入口

- [QT_QUICK_MIGRATION_PLAN.md](QT_QUICK_MIGRATION_PLAN.md)是项目内最高优先级契约，用户最新明确要求优先。按当前顺序一次执行一个轮次；完成小节直接移除，独立缺环境事项按原门槛和计划后置分工移交。
- 实施前读取计划公共规则、涉及的专项契约和对应经验，评估规模/耗时；专项职责由计划索引确定，同一要求不得多文档重复维护。
- 经验只保留明确触发条件与核心解决方案；不记录实验/尝试过程、失败数值、验证成绩或流水账。未决问题归具体待办。
- 正式展示名`yalahOO!!!`，技术标识`yalahoo`。根CMake/C++23，C++在`src/`、QML在`qml/Yalahoo/Music/`、资产`resources/`、测试`tests/`、工具`tools/`；`legacy-web/`冻结，只作来源与追溯。
- 语言、Qt对象/线程/模型及QML/C++职责分别遵守[cpp23-best-practices.md](docs/contracts/cpp23-best-practices.md)、[qt-best-practices.md](docs/contracts/qt-best-practices.md)、[qml-best-practices.md](docs/contracts/qml-best-practices.md)；专项门槛仍由计划索引确定。
- 源码风格与格式化/qmllint入口按[code-style.md](docs/contracts/code-style.md)；四空格、展开语句，提交前`check-format`及Release `source-format`必须通过。
- Kit发现/C++23/模块闭包按[native-bootstrap.md](docs/contracts/native-bootstrap.md)，不得固定Qt minor/patch/安装目录或降级。
- 外观按冻结生产源码理论效果及用户固定差异实现；禁止截图像素追分和复制锯齿/异常模糊。数学/状态、实际质量与功能分别证明。
- 全部QML控件必须遵守[自绘控件强制规则](docs/contracts/controls.md)：预制控件禁用边界、来源理论值、显式视觉属性与行为基础仅由该契约维护，后续session同样适用。
- PERF-01–07、预算与测量语义按[performance.md](docs/contracts/performance.md)，设计缺陷不能用benchmark通过豁免。
- 常规仅Release；只有明确需要堆栈才用Debug。检查范围与复用按[test-gates.md](docs/contracts/test-gates.md)，不能将暂缓写为通过。
- 缺陷修复必须补充单元回归测试，覆盖触发条件和预期结果，确保测试能暴露原缺陷并在修复后通过；优先扩展现有测试。网络单元测试使用本地回环服务或可控的请求/响应替身，不依赖公网可用性。只有确实无法自动化、且提供可核查的技术限制与权威依据、替代验证方案时，才允许例外；当前环境缺少工具或设备只能记为待验，不能免除测试要求。
- 所有验证窗口/采集遵守[test-window-policy.md](docs/contracts/test-window-policy.md)。用户指定轮次可见验证即授权该轮前台，无需再确认；结束清开关并恢复默认隐藏策略，授权不跨轮次。
- 普通音乐只用QtMultimedia公开API；歌曲/BGM/UI音效独立，不做无缝/PCM/自建音频引擎。联网资料的Provider、来源/缓存/状态与配置遵守library-data契约。
- 三平台完整实现，按本轮实际能力执行可靠检查；源码可确定缺陷当轮修复，独立缺环境的目标事项按计划后置分工完整移交。
- 保留作者/许可/来源，不自动发布、上传私人数据、改发行标签或写用户歌曲。
- Git只收源码、产品资产、必要测试输入与简明契约。[verification-artifacts.md](docs/contracts/verification-artifacts.md)定义输出边界；按明确路径暂存并检查数量/体积，不批量提交验证产物。
