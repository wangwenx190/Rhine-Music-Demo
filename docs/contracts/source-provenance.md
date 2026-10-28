# 来源标识与独立原生输入

本文维护来源身份、原生资产和独立回归输入的边界。产品规范由专项契约维护；来源标识用于追溯，不参与构建、生成、测试或运行时文件查找。

## 来源身份

视觉和运动的历史基准为[Rhine-Music-Demo V0.4.1](https://github.com/RonaldDeng/Rhine-Music-Demo)，原提交标识`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，本仓库保留的等价历史对象为`df251a2eb0406b1ccde11a0e9f8a5017c0293185`。音乐入口的历史路径为`src/main.ts → src/music-app.ts`。历史工程只保存在Git历史中，当前源码树不保留旧Web应用或其依赖。

[source-map.json](source-map.json)记录历史模块名/hash、导入关系及当前原生职责；[source-fidelity-review.md](source-fidelity-review.md)维护适配依据。manifest和fixture中的`baseline`、`baselineCommit`、`source`及`upstream:`标识是归属元数据，不是需要存在的本地路径。保留来源值，不下载或执行上游代码补齐这些标识。

## 原生资产与生成

`resources/`包含完整产品输入：GLB、Qt派生mesh、字体、图标、音频、封面、环境/lookup、shader源码及许可。CMake直接消费明确资源清单，同Kit qsb编译当前shader；资产准备只从该目录验证清单、加载字体及重导入模型，详见[assets.md](assets.md)。原创品牌与原生shader适配工具只读取当前项目输入。

原生构建、资产生成、测试和部署均不读取旧工程目录、旧版本Git源码对象或锁定Web依赖。Qt官方工具、CMake/Ninja、Python及个别使用Node标准库的维护脚本按各自入口准备；不需要npm安装、TypeScript、Three.js、rolling-number包或浏览器。普通产品的运行依赖见[product.md](product.md)。

## 独立回归输入

`tests/fixtures/`保留独立提取的数学、结构和材质参数及来源hash；它们直接作为回归输入，不从当前被测实现重算期望，不要求重建旧应用。修改算法先核对相关专项的公式、输入和容差；只有设计或明确来源变化时才维护对应独立期望。

`tests/fixtures/web-baseline/anonymous.json`保留受控viewport、DPR、主题、质量、稳定专辑ID、选择、时间和事件。空/单/奇偶/混合/480专辑库使用qrc封面，roots为空且曲目无音频URL；`docs/contracts/library-order.json`固定已有列顺序，不换成本机排序结果。目录名仅表达这些输入的历史来源。

目标后端与设备检查使用当前原生单元、QML及产品回放入口；不依赖旧DOM采集或跨工程字体报告。目标剩余事实和原有数学/结构/实际质量门槛由迁移计划R36与[test-gates.md](test-gates.md)维护。
