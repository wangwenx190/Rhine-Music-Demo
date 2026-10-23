# yalahOO!!! · 原生项目开发入口

- [QT_QUICK_MIGRATION_PLAN.md](QT_QUICK_MIGRATION_PLAN.md)为项目内最高优先级契约，用户最新明确要求优先。先读公共执行/文档规则及专项索引，再读涉及主题；按当前顺序推进，完成小节直接移除，独立缺环境事项按原门槛交R36。
- 产品范围、技术身份与目录职责唯一见[product.md](docs/contracts/product.md)；冻结来源及其历史文档语境见[web-baseline.md](docs/contracts/web-baseline.md)。
- C++语言/接口、Qt对象/线程/模型和QML/C++职责分别执行[cpp23-best-practices.md](docs/contracts/cpp23-best-practices.md)、[qt-best-practices.md](docs/contracts/qt-best-practices.md)、[qml-best-practices.md](docs/contracts/qml-best-practices.md)。
- Kit/C++23/模块闭包执行[native-bootstrap.md](docs/contracts/native-bootstrap.md)，源码格式、check-format/Release source-format和官方qmllint执行[code-style.md](docs/contracts/code-style.md)。
- 全部QML控件执行[controls.md](docs/contracts/controls.md)的自绘强制规则；数学/结构、实际质量、功能与设备事实按[test-gates.md](docs/contracts/test-gates.md)分别证明，缺陷回归要求亦由该契约维护。
- 性能严格静态审查及必要最小实测唯一执行[performance.md](docs/contracts/performance.md)；窗口策略及当轮可见授权唯一执行[test-window-policy.md](docs/contracts/test-window-policy.md)，授权不跨轮。
- 歌曲/BGM/UI声音边界见[playback.md](docs/contracts/playback.md)，扫描/资料Provider见[library-data.md](docs/contracts/library-data.md)，设置/偏好/实例仲裁见[settings.md](docs/contracts/settings.md)。
- 三平台源码可确定缺陷当轮修复，实际构建/部署/设备缺口只由迁移计划管理，不把暂缓记为通过。作者/许可/来源保留，不自动发布、上传私人数据、改发行标签或写用户歌曲。
- 生成输出与Git边界执行[verification-artifacts.md](docs/contracts/verification-artifacts.md)；按明确路径暂存并检查数量/体积，不批量提交验证产物。
