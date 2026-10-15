# 验证门禁与窗口分组

迁移计划的源码绝对基准、动态验证能力与门禁分层是现行要求。默认回归不显示窗口；完整自动回归与原生窗口专项分别执行，不再把所有历史窗口混进一次常规CTest。

| 入口 | 范围 | 使用时机 |
| --- | --- | --- |
| `ctest --preset qt-release` | `fast`：源码/算法/状态、资产/模块、格式 | 修改后与提交前；不代表GUI或全平台验收 |
| `ctest --preset qt-release-render -R overview-qml` | 选择受影响的真正隐藏渲染集成 | QML/渲染/结构/资源变化，按组件选择 |
| `ctest --preset qt-release-full` | 快速项及当前真正隐藏的完整自动回归 | 共享状态/渲染/模块/资源或构建集成边界，最终执行一次 |
| `ctest --preset qt-release-visible -R '^runtime-overview$'` | 显式选择实际需要原生窗口的项目 | 输入/焦点/原生窗口/呈现及相关生命周期专项 |

`cmake/TestPolicy.cmake`按实际入口标注`fast`、`render-integration`、`hidden-render`、`native-visible`及旧`legacy-image-gate`，GPU/窗口集成均为`RUN_SERIAL`。当前OffscreenSurface仅在macOS实际选择RenderControl，因此其他系统相同集成入口标为需要窗口；这描述当前实现，后续按真实能力变化更新，不是永久平台限制。

快速/完整自动preset显式清除`YALAHOO_VERIFY_VISIBLE`、`YALAHOO_VERIFY_FOREGROUND`和`YALAHOO_VERIFY_LIFECYCLE`，并排除`native-visible`与旧原图比较入口；快速preset另排除完整渲染集成。macOS日常配置保持`YALAHOO_MACOS_VISIBLE_TESTS=OFF`，原生窗口项保留disabled事实。隐藏窗口策略CTest只调用`hiddenNativeWindow`槽，不再顺带枚举可见窗口槽。

确有必要做原生窗口验证时，macOS可显式配置`YALAHOO_MACOS_VISIBLE_TESTS=ON`；该开关本身不授予前台权限，也不让日常preset自动弹窗。原生窗口preset不设置授权环境变量，使用前按当轮授权和测试窗口契约选择前台或默认置底/拒绝焦点策略。授权前台示例：

```sh
cmake --preset qt-release -DYALAHOO_MACOS_VISIBLE_TESTS=ON
YALAHOO_VERIFY_FOREGROUND=1 ctest --preset qt-release-visible -R '^runtime-overview$'
cmake --preset qt-release -DYALAHOO_MACOS_VISIBLE_TESTS=OFF
ctest --preset qt-release -R verification-window-policy
```

不默认执行全部可见项目。`verify-detail.py`与`verify-overview.py`默认单次普通功能，只有加`--lifecycle`才执行三十次换片/十次重建等对应压力序列；报告明确请求模式及实际次数。固定压力序列只在相关所有权/缓存/上下文边界验证，复用同版本同环境有效事实；普通功能一次有效回放即可，三次独立运行仅用于正式性能/成本/噪声复核。需要后端/DPR/主题完整矩阵的变化按风险覆盖，最终目标平台要求仍保留；不因日常筛选而宣称未执行项目通过。

旧原图/浏览器witness与功能资源耦合的入口先保留`legacy-image-gate`标签并显式disabled，所有preset排除；裸CTest也不会运行，由R19A–R19H逐组件分离后恢复其中必要的非像素检查。不得直接调用工具恢复这些原图匹配。源码数学、投影、文字/字体身份、实际质量、功能与资源门禁保留；图像算法工具的合成输入单元测试不读取或拟合旧实机图。

未通过检查不能通过改标签藏起；现行变化调整的是执行时机与范围，不改功能断言或数值门槛。最终报告注明选择/复用/未运行项目及原因；格式、两组官方qmllint按改动范围执行，生成输出保持忽略。

R19B 的 Review 配置新增两个可选择的隐藏项：`archive-pool-gpu` 与 `real-columns-gpu`。macOS 默认使用 RenderControl，完整自动 preset 会选入；其他平台仍使用原生窗口并标为 native-visible。它们不再调用 SSIM/RGB 比较，检查源矩阵/拾取/身份/cover fallback、上传及真实 scene graph 失效后的重放。运行前用对应源码导出器在当前系统目录生成 `frozen-poses.json` / `frozen-fields.json`；这些是忽略的开发输入，普通构建不依赖它们。`YALAHOO_REVIEW=OFF` 时不注册这两个开发测试。

R19C2A将`runtime-materials`和`runtime-lighting`恢复为独立的非像素`render-integration`项：macOS使用真正隐藏Metal并标`hidden-render`，进入完整自动/渲染preset；其他平台使用置底拒绝焦点窗口并标`native-visible`。前者覆盖99个源材质/纸面状态和十次完整root重建，后者覆盖48个灯光状态和一次完整root重建；不读取浏览器截图或执行ROI评分。RHI/后处理旧图像项仍disabled，按R19C2B/R19D处理，不因此自动启用。

R19C2B1将`rhi-opaque-gpu`从旧图像disabled列表移出，macOS标为真正隐藏的render-integration/hidden-render，进入完整自动preset；其他系统仍为native-visible。入口使用源平面/法向/投影和纸面公式、真实格式、resize/封面revision/读回共享及十次重建，不加载Quick3D或浏览器图像。旧rhi-shell/postprocess evaluator仍disabled；不把本项通过外推为光学或后处理已复审。

R19C2B2恢复`rhi-shell-gpu`为源公式/真实graph/自身输出门禁，macOS真正隐藏，完整自动preset包含；其他系统为native-visible。原`runtime-rhi-shell-quick3d`替代渲染步骤已移除，测试直接消费源JSON；61状态、10数学样本、排序/隔离、alpha和10次重建独立验证。后处理旧图像项仍disabled，等R19D拆耦；本项不替代连续阵列质量检查。

R19C2C新增`rhi-flicker-gpu`，macOS真正隐藏且进入完整自动preset，其他平台为native-visible。它以8个跨批量阈值/MSAA输入和16×60帧生产序列验证多pass位置不变性、有限光学和资源稳定；交替正常输出与Review诊断，不执行旧图像比较，不代表正常窗口/VSYNC。

R19D1新增`rhi-ao-gpu`非图像门禁：27个背景/倍率/核/尺寸状态、243个源数学点（含box及.22）、实际R16F/缓存复用/十次重建。macOS真正隐藏并进入完整自动preset，其他系统为native-visible；不会启用旧occlusion-gpu-audit或原图比较。

R19D4A新增`rhi-bokeh-gpu`：直接QRhi offscreen frame，不创建可见窗口，因此各平台均标`render-integration/hidden-render`并串行执行；它不依赖OffscreenSurface在不同平台是否显示窗口。当前真实执行环境仍须逐项记录。46组独立源数学/十次Postprocess资源重建，以及既有`rhi-flicker-gpu`1200帧中的实际Bokeh uniform/完整depth检查分别覆盖孤立算法和生产集成，不启用旧图像门禁。

R19D4B1新增`rhi-smaa-gpu`：与Bokeh独立入口一样直接使用QRhi offscreen frame，各平台按实际入口标为`render-integration/hidden-render`并串行进入完整自动preset。34解析状态分别验证生产edges/weights/blend、两张lookup、RGBA16F/资源复用、240-byte前缀及十次Postprocess重建；当前只实际验证macOS Metal/OpenGL，其他目标保留R34。它不启用旧四套图像门禁；其非像素覆盖闭包和真实MSAA/resolve仍R19D4B2。
