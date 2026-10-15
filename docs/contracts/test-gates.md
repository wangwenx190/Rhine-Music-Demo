# 测试分组与执行范围

本文只维护CTest标签/preset、选择与复用规则；窗口行为归[test-window-policy.md](test-window-policy.md)，格式命令归[code-style.md](code-style.md)，指标/正式采样归[performance.md](performance.md)，组件断言在各组件契约。

| 入口 | 范围与使用时机 |
| --- | --- |
| `ctest --preset qt-release` | fast：源码/算法/状态/资产/模块/格式；修改后与提交前 |
| `ctest --preset qt-release-render -R 组件` | 按需选择真正隐藏渲染项 |
| `ctest --preset qt-release-full` | fast+隐藏集成；共享状态/渲染/模块/资源/构建边界最终一次 |
| `ctest --preset qt-release-visible -R 具体测试` | 显式窗口/输入/焦点/呈现/相关生命周期，不默认全选 |

- `cmake/TestPolicy.cmake`按真实入口标fast/render-integration/hidden-render/native-visible/legacy-image-gate，GPU/窗口均RUN_SERIAL。改标签不能掩盖可靠检查失败。
- fast/full清YALAHOO_VERIFY_VISIBLE/FOREGROUND/LIFECYCLE并排除native-visible/legacy-image-gate，fast另排除render-integration。macOS默认可见OFF，disabled如实暂缓；默认窗口策略只调用hiddenNativeWindow槽。
- 正常功能一次有效回放；同二进制/同环境有效结果复用。三次仅正式性能/成本或具体不稳定复核；十次重建/三十次换片仅相关所有权/缓存/上下文压力，verify-detail/overview用显式--lifecycle并报告实际次数。
- 按风险选尺寸/主题/DPR，字体/采样/布局/共享后端变化覆盖对应完整范围；缩小日常门禁不取消最终目标矩阵。报告区分实际、复用、未选、暂缓及原因。
- 旧图像入口disabled并由全部preset排除，裸CTest也不能运行；R19各组件逐项恢复非像素检查，合成输入的ImageComparison单元测试不等于运行源图评分。

## 当前Review入口分类

| 实际入口 | 分类 |
| --- | --- |
| rhi-bokeh-gpu、rhi-smaa-gpu | 直接QRhi offscreen，各平台入口均无可见窗口；实际后端能力另验 |
| ui-drawing、presentation-qml、overview-qml、archive-pool-gpu、real-columns-gpu、runtime-materials、runtime-lighting、rhi-opaque/shell/flicker/ao/fog/shadow/postprocess-gpu | macOS当前RenderControl真正隐藏；其他系统当前需native窗口，按能力变化重评 |
| render-backend-gpu-audit、postprocess-gpu-audit、occlusion-gpu-audit | legacy-image-gate，disabled且直接运行也拒绝；逐项覆盖映射见source-fidelity-review.md的R19D4B2 |

Review OFF不注册相应开发测试。archive/real-columns源数学输入用对应导出器生成frozen-poses/frozen-fields至本地；普通构建不依赖它们。组件具体状态数/门槛只在组件契约维护。
