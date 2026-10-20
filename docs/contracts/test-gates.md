# 测试分组与执行范围

本文只维护CTest标签/preset、选择与复用规则；窗口行为归[test-window-policy.md](test-window-policy.md)，格式命令归[code-style.md](code-style.md)，性能的静态优先策略及最小实测门槛唯一归[performance.md](performance.md)，组件断言在各组件契约。

| 入口 | 范围与使用时机 |
| --- | --- |
| `ctest --preset qt-release -R 对应测试` | fast：只选择本次逻辑改动影响的源码/算法/状态/资产/模块/格式检查 |
| `ctest --preset qt-release-render -R 组件` | 按需选择真正隐藏渲染项 |
| `ctest --preset qt-release-stress -R 组件` | 仅静态审计仍无法确定的具体资源/寿命问题确需压力时选用最小范围；修改所有权代码或首次目标验收本身不触发 |
| `ctest --preset qt-release-full` | fast+隐藏功能集成，不含独立压力；仅在用户明确要求完整测试/全量回归时执行，不因共享代码变更或准备提交自动触发 |
| `ctest --preset qt-release-visible -R 具体测试` | 显式窗口/输入/焦点/呈现/相关生命周期，不默认全选 |

- `cmake/TestPolicy.cmake`按真实入口标fast/render-integration/hidden-render/native-visible/resource-stress，GPU/窗口均RUN_SERIAL。改标签不能掩盖可靠检查失败。
- fast/full/render清YALAHOO_VERIFY_VISIBLE/FOREGROUND/LIFECYCLE/STRESS；常规入口排除resource-stress，fast/full/render另排除native-visible，fast另排除render-integration。stress preset仅选择隐藏resource-stress；其他系统的原生压力用组件显式工具运行。macOS默认可见OFF，disabled如实暂缓；默认窗口策略只调用hiddenNativeWindow槽。
- 先按被测行为、依赖、输入和运行条件判断已有结果是否仍有效，并尽可能复用。无关模块、Git版本号或重新链接导致二进制hash变化，本身不使结果失效；相关逻辑/输入/依赖或环境变化仅使对应结果失效。不影响行为的注释、文档或脚本注释改动无需专门运行测试；脚本逻辑变化检查其参数、错误传播与实际入口；代码逻辑变更只执行必要的对应检查。共享代码沿实际调用链确定范围，不自动扩大到fast集或完整回归。功能回放与性能问题分别选择；局部成本、计时、资源增长及重复重建压力先满足performance.md的静态未决门槛，不默认各跑一次。显式压力入口为verify-startup的--stress、verify-detail/overview的--lifecycle，报告实际完成次数；已有有效结果复用，真实失败仍保留。
- OpenGL动态检查统一由迁移计划R35执行，R20–R34只运行适用的非OpenGL生产后端；Linux默认OpenGL动态入口同样后置。算法/源码/模块/构建和qsb跨目标静态检查照常执行；这不减少R35的原门槛。
- 对需要新验证的受影响行为按风险选择尺寸/主题/DPR，只覆盖其实际依赖的范围；未被改动触及的最终目标矩阵结果继续复用，未执行的目标门槛仍保留。报告区分本次实际、复用、未选、暂缓及原因。首次目标验收仍覆盖组件契约所列独立边界，不能用代表组合冲销尚未执行项。格式与官方qmllint的适用选择归code-style.md，同样不扩大为功能回归。

## 积欠项的合并与矩阵选择

- 执行单元先列最小检查清单和退出条件，以及唯一测试名/所选状态数、采样秒数×次数、独立压力次数；按工具实际参数核对计划成本，不能把TIMEOUT上界当预期耗时，也不能先跑整套来估时。无新增风险不追加无关对照或重复。正式长采样按固定场景拆单元，不将积欠项汇成十二场景、多模式和压力一起执行的大型命令。
- 同一目标/后端/输入/相关源码下，一次有效执行可同时满足多个任务ID；按报告中的实际断言逐项对应，不能因待办分属不同小节重复运行。一次构建、格式、qmllint或qsb结果覆盖同一执行单元，修改其依赖后仅重做失效部分。不同系统/后端、物理输入和真实窗口事实不能互相代替。
- 验证包装默认消费已准备的Release产物，不自动格式化源码、切换Review配置或附带全套前置回归。构建/官方qmllint/格式、冻结源导出及CTest名称并集由共用准备单元执行一次；同环境跨后端不重复生成与后端无关的源数学输入，字体/平台布局输入仍随实际目标环境校验。
- 布局先在DPR1覆盖契约所列尺寸及断点；双主题在代表桌面/窄屏核对颜色、合成和主题状态，只有主题改变几何时再扩展对应尺寸。DPR1.5/2在代表桌面与受DPR影响的窄屏/文字/附件边界补验，不重复所有尺寸、交互和压力。颜色/主题耦合算法与奇数尺寸/MSAA/格式等真实组合依赖仍保留；便宜的解析/状态边界不为减少数量而删除。
- 长交互序列在代表桌面/窄屏覆盖；其他布局状态只做进入、关键几何/合成和正常返回，布局导致不同输入分支时才补对应交互。生产文字/滑尺路径必验；备用绘制路径的质量/成本对比仅在生产路径失败、具体性能调查或明确选型时显式选择，不成为每个目标的默认验收。
- 原生窗口hide/minimize/restore、物理跨屏、完整root重建和独立pass资源重建按实际需要分别证明。同一原生宿主的一次观测可覆盖多个待办；独立renderer/资源所有者的释放不能互相代替。性能/资源问题能从所有权和释放代码确定时不追加重复压力；确有未决运行行为时只观察相关所有者和必要转换，重复次数由该问题决定。
- 同一Linux系统/Kit/GPU/驱动/QRhi后端在X11与Wayland间共享构建、源数学、独立pass公式及无会话依赖的组件结果；每个QPA会话仍须真实证明上下文/swapchain、格式/MSAA/resolve、窗口/输入/焦点/DPR及呈现。窗口系统或提交分支相关的差异另验，不将整个组件矩阵再复制一次；呈现长采样的会话选择归performance.md。
- 确定性生产帧序列按完整状态/过渡覆盖，不叠加长时间采样或外层三次重复。相同rhi-flicker生产序列的有效报告可共同证明各pass上传、有限值/覆盖、资源和组合输出；独立pass公式测试仍各运行一次。
- 工具尚不支持所需范围选择时，所属积欠项先补选择器及实际覆盖报告，再执行缩小后的入口；不得先跑整套再称为局部，也不能把未覆盖项标为通过。已有小型解析矩阵无需为拆分增加工具复杂度。
- 组件文档所列成本采样、固定帧数和重建/换片压力是工具覆盖能力，不自动形成性能必跑清单；执行条件按performance.md。若功能/画质入口捆绑了不需要的性能或重复压力部分，先解开相关分支或使用更小入口，不因包装限制执行整套。

## 当前Review入口分类

| 实际入口 | 分类 |
| --- | --- |
| rhi-bokeh-gpu、rhi-smaa-gpu | 直接QRhi offscreen，各平台入口均无可见窗口；实际后端能力另验 |
| startup-qml、startup-stress、controls-qml、experiments-qml、settings-qml、playback-qml、library-panels-qml、ui-drawing、ruler-drawing、presentation-qml、overview-qml、archive-pool-gpu、real-columns-gpu、runtime-materials、runtime-lighting、rhi-opaque/shell/flicker/ao/fog/shadow/postprocess/scale-gpu | macOS当前RenderControl真正隐藏；其他系统当前需native窗口，按能力变化重评 |

整场GUI显式入口为`python3 tools/build/verify-gui.py`，范围与采集语义见[rhi-integration.md](rhi-integration.md)；它不隐式加入fast/full集合。

Review OFF不注册相应开发测试。archive/real-columns/ruler源数学输入用对应导出器生成frozen-poses/frozen-fields至本地；普通构建不依赖它们。组件具体状态数/门槛只在组件契约维护。
