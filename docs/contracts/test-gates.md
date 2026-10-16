# 测试分组与执行范围

本文只维护CTest标签/preset、选择与复用规则；窗口行为归[test-window-policy.md](test-window-policy.md)，格式命令归[code-style.md](code-style.md)，指标/正式采样归[performance.md](performance.md)，组件断言在各组件契约。

| 入口 | 范围与使用时机 |
| --- | --- |
| `ctest --preset qt-release -R 对应测试` | fast：只选择本次逻辑改动影响的源码/算法/状态/资产/模块/格式检查 |
| `ctest --preset qt-release-render -R 组件` | 按需选择真正隐藏渲染项 |
| `ctest --preset qt-release-full` | fast+隐藏集成；仅在用户明确要求完整测试/全量回归时执行，不因共享代码变更或准备提交自动触发 |
| `ctest --preset qt-release-visible -R 具体测试` | 显式窗口/输入/焦点/呈现/相关生命周期，不默认全选 |

- `cmake/TestPolicy.cmake`按真实入口标fast/render-integration/hidden-render/native-visible/legacy-image-gate，GPU/窗口均RUN_SERIAL。改标签不能掩盖可靠检查失败。
- fast/full清YALAHOO_VERIFY_VISIBLE/FOREGROUND/LIFECYCLE并排除native-visible/legacy-image-gate，fast另排除render-integration。macOS默认可见OFF，disabled如实暂缓；默认窗口策略只调用hiddenNativeWindow槽。
- 先按被测行为、依赖、输入和运行条件判断已有结果是否仍有效，并尽可能复用。无关模块、Git版本号或重新链接导致二进制hash变化，本身不使结果失效；相关逻辑/输入/依赖或环境变化仅使对应结果失效。不影响行为的注释、文档、脚本等改动无需专门运行测试；代码逻辑变更只执行对应测试。共享代码应沿实际调用链确定受影响项，不自动扩大到整个fast集或完整回归。正常功能一次有效回放；三次仅正式性能/成本或具体不稳定复核，十次重建/三十次换片仅相关所有权/缓存/上下文压力，verify-detail/overview用显式--lifecycle并报告实际次数。
- OpenGL动态检查统一由迁移计划R35执行，R20–R34只运行适用的非OpenGL生产后端；Linux默认OpenGL动态入口同样后置。算法/源码/模块/构建和qsb跨目标静态检查照常执行；这不减少R35的原门槛。
- 对需要新验证的受影响行为按风险选择尺寸/主题/DPR，只覆盖其实际依赖的范围；未被改动触及的最终目标矩阵结果继续复用，未执行的目标门槛仍保留。报告区分本次实际、复用、未选、暂缓及原因。格式与官方qmllint的适用选择归code-style.md，同样不扩大为功能回归。
- 旧图像入口disabled并由全部preset排除，裸CTest也不能运行；R19各组件逐项恢复非像素检查，合成输入的ImageComparison单元测试不等于运行源图评分。

## 当前Review入口分类

| 实际入口 | 分类 |
| --- | --- |
| rhi-bokeh-gpu、rhi-smaa-gpu | 直接QRhi offscreen，各平台入口均无可见窗口；实际后端能力另验 |
| playback-qml、library-panels-qml、ui-drawing、ruler-drawing、presentation-qml、overview-qml、archive-pool-gpu、real-columns-gpu、runtime-materials、runtime-lighting、rhi-opaque/shell/flicker/ao/fog/shadow/postprocess/scale-gpu | macOS当前RenderControl真正隐藏；其他系统当前需native窗口，按能力变化重评 |
| render-backend-gpu-audit、postprocess-gpu-audit、occlusion-gpu-audit | legacy-image-gate，disabled且直接运行也拒绝；逐项覆盖映射见source-fidelity-review.md的R19D4B2 |

Review OFF不注册相应开发测试。archive/real-columns/ruler源数学输入用对应导出器生成frozen-poses/frozen-fields至本地；普通构建不依赖它们。组件具体状态数/门槛只在组件契约维护。
